#include "syscall.h"
#include "rnet.h"
#include "libstr.h"
#include "liballoc.h"

#define MAX_LINE 256
#define MAX_NAME 128
#define SERVER_MBOX 0
#define CLIENT_MBOX 0

static void putnl() { PutString("\n"); }

static void build_get(char *out, int cap, const char *fn, const char *outname) {
    // "GET <fn> <outname>"
    out[0] = '\0';
    strcpy(out, "GET ", cap);
    int p = strlen(out);

    // append fn
    for (int i = 0; fn[i] && p < cap - 1; i++) out[p++] = fn[i];
    if (p < cap - 1) out[p++] = ' ';

    for (int i = 0; outname[i] && p < cap - 1; i++) out[p++] = outname[i];
    out[p] = '\0';
}

static int parse_ok_size(char *line) {
    // line: "OK <num>"
    // parse_str_cmd(0)=OK, (1)=num
    char w0[32], w1[32];
    if (!parse_str_cmd(0, line, w0, 32)) return -1;
    if (!parse_str_cmd(1, line, w1, 32)) return -1;
    if (str_cmp(w0, "OK") != 0) return -1;
    return atoi(w1);
}

int main() {
    PutString("[client] RNet file client\n");
    PutString("Enter server machine id: ");
    char line[64];
    GetString(line, 64);
    rstrip_ws(line);
    int serverM = atoi(line);

    PutString("Enter remote file name to fetch: ");
    char fname[MAX_NAME];
    GetString(fname, MAX_NAME);
    rstrip_ws(fname);

    PutString("Enter local output file name to write: ");
    char outname[MAX_NAME];
    GetString(outname, MAX_NAME);
    rstrip_ws(outname);
    int s = rnet_socket(CLIENT_MBOX);
    if (s < 0) {
        PutString("[client] rnet_socket failed\n");
        Exit(-1);
    }

    // Send GET
    char req[MAX_LINE];
    build_get(req, MAX_LINE, fname, outname);

    if (rnet_send_fixed(s, serverM, SERVER_MBOX, req, strlen(req) + 1) < 0) {
        PutString("[client] send GET failed\n");
        rnet_close(s);
        Exit(-1);
    }

    // Receive OK <size> or ERR
    char resp[64];
    int srcM, srcB;
    int nr = rnet_recv_fixed(s, resp, 64, &srcM, &srcB);
    if (nr < 0) {
        PutString("[client] recv resp failed\n");
        rnet_close(s);
        Exit(-1);
    }

    if (str_cmp(resp, "ERR") == 0) {
        PutString("[client] server returned ERR\n");
        rnet_close(s);
        Exit(0);
    }

    int expected = parse_ok_size(resp);
    if (expected < 0) {
        PutString("[client] bad server response: ");
        PutString(resp);
        putnl();
        rnet_close(s);
        Exit(-1);
    }

    PutString("[client] expecting ");
    PutInt(expected);
    PutString(" bytes\n");

    // Allocate buffer and receive var
    char *buf = (char*)malloc(expected);
    if (!buf && expected > 0) {
        PutString("[client] malloc failed\n");
        rnet_close(s);
        Exit(-1);
    }

    int outLen = 0;
    int sm2, sb2;
    int got = rnet_recv_var(s, buf, expected, &outLen, &sm2, &sb2);
    if (got < 0) {
        PutString("[client] recv_var failed\n");
        if (buf) free(buf);
        rnet_close(s);
        Exit(-1);
    }

    if (outLen != expected) {
        PutString("[client] warning: header size != expected\n");
        PutString(" server said=");
        PutInt(expected);
        PutString(" begin said=");
        PutInt(outLen);
        putnl();
    }

    if (got != expected) {
        PutString("[client] warning: received bytes truncated? got=");
        PutInt(got);
        PutString(" expected=");
        PutInt(expected);
        putnl();
    }

    // Write to disk under outname
    Create(outname);
    OpenFileId of = Open(outname);
    if (of < 0) {
        PutString("[client] Open(outname) failed\n");
        if (buf) free(buf);
        rnet_close(s);
        Exit(-1);
    }

    // Write may return less; loop
    int off = 0;
    while (off < got) {
        int chunk = got - off;
        if (chunk > 256) chunk = 256;
        Write(buf + off, chunk, of);
        off += chunk;
    }
    Close(of);

    // Print file contents to console
    PutString("----- received file content -----\n");
    // Not necessarily null-terminated; print bytes as chars
    for (int i = 0; i < got; i++) {
        PutChar(buf[i]);
    }
    PutString("\n----- end -----\n");

    if (buf) free(buf);

    // Tell server DONE (optional)
    rnet_send_fixed(s, serverM, SERVER_MBOX, "DONE", 5);

    rnet_close(s);
    Exit(0);
}

