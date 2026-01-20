#include "syscall.h"
#include "rnet.h"
#include "libstr.h"
#include "liballoc.h"

#define MAX_LINE 256
#define MAX_NAME 128
#define SERVER_MBOX 0
#define CLIENT_MBOX 0

static void putnl() { PutString("\n"); }

// Very small helper: split "GET <fn> <out>" into tokens using parse_str_cmd
static int parse_get(char *line, char *cmd, char *fn, char *out) {
    // parse_str_cmd(index, in, out, max)
    // index 0 => cmd, 1 => fn, 2 => out
    if (!parse_str_cmd(0, line, cmd, MAX_NAME)) return 0;
    if (!parse_str_cmd(1, line, fn,  MAX_NAME)) return 0;
    if (!parse_str_cmd(2, line, out, MAX_NAME)) return 0;
    return 1;
}

int main() {
    PutString("[server] RNet file server starting...\n");
    PutString("[server] Waiting for GET request...\n");

    int s = rnet_socket(SERVER_MBOX);
    if (s < 0) {
        PutString("[server] rnet_socket failed\n");
        Exit(-1);
    }

    // Wait for one request
    char req[MAX_LINE];
    int srcM = -1, srcB = -1;
    int n = rnet_recv_fixed(s, req, MAX_LINE, &srcM, &srcB);
    if (n < 0) {
        PutString("[server] recv_fixed failed\n");
        rnet_close(s);
        Exit(-1);
    }

    char cmd[MAX_NAME], filename[MAX_NAME], outname[MAX_NAME];
    cmd[0] = filename[0] = outname[0] = '\0';

    if (!parse_get(req, cmd, filename, outname) || str_cmp(cmd, "GET") != 0) {
        PutString("[server] bad request: ");
        PutString(req);
        putnl();
        rnet_send_fixed(s, srcM, CLIENT_MBOX, "ERR", 4);
        rnet_close(s);
        Exit(0);
    }
    rstrip_ws(filename);
    rstrip_ws(outname);
    PutString("[server] request from machine ");
    PutInt(srcM);
    PutString(": file=");
    PutString(filename);
    PutString(" (client wants out name=");
    PutString(outname);
    PutString(")\n");

    OpenFileId fid = Open(filename);
    if (fid < 0) {
	PutString("[server] filename bytes: ");
	    for (int i = 0; i < 32 && filename[i] != 0; i++) {
    	    	PutInt((int)(unsigned char)filename[i]);
		PutChar(' ');
 	    }
	PutChar('\n');
        PutString("[server] Open failed\n");
        rnet_send_fixed(s, srcM, CLIENT_MBOX, "ERR", 4);
        rnet_close(s);
        Exit(0);
    }

    // Read whole file into memory (stub fs is small; still do it robustly)
    // We'll read in chunks and grow buffer.
    int cap = 1024;
    int len = 0;
    char *buf = (char*)malloc(cap);
    if (!buf) {
        PutString("[server] malloc failed\n");
        Close(fid);
        rnet_send_fixed(s, srcM, CLIENT_MBOX, "ERR", 4);
        rnet_close(s);
        Exit(0);
    }

    for (;;) {
        char tmp[256];
        int r = Read(tmp, 256, fid);
        if (r <= 0) break;

        if (len + r > cap) {
            int newCap = cap;
            while (newCap < len + r) newCap *= 2;
            char *nb = (char*)malloc(newCap);
            if (!nb) {
                PutString("[server] realloc malloc failed\n");
                free(buf);
                Close(fid);
                rnet_send_fixed(s, srcM, CLIENT_MBOX, "ERR", 4);
                rnet_close(s);
                Exit(0);
            }
            for (int i = 0; i < len; i++) nb[i] = buf[i];
            free(buf);
            buf = nb;
            cap = newCap;
        }

        for (int i = 0; i < r; i++) buf[len + i] = tmp[i];
        len += r;
    }

    Close(fid);

    // Send OK <size> first
    char okline[64];
    // Build "OK <len>"
    // (no sprintf in userland by default; do it manually)
    strcpy(okline, "OK ", 64);
    // append decimal len
    char digits[16];
    int t = 0, x = len;
    if (x == 0) digits[t++] = '0';
    while (x > 0 && t < 15) { digits[t++] = (char)('0' + (x % 10)); x /= 10; }
    int p = strlen(okline);
    for (int i = t - 1; i >= 0 && p < 63; i--) okline[p++] = digits[i];
    okline[p] = '\0';

    rnet_send_fixed(s, srcM, CLIENT_MBOX, okline, strlen(okline) + 1);

    // Send the file bytes as a var message
    PutString("[server] sending ");
    PutInt(len);
    PutString(" bytes...\n");

    int ok = rnet_send_var(s, srcM, CLIENT_MBOX, buf, len);
    free(buf);

    if (ok < 0) {
        PutString("[server] send_var failed\n");
        rnet_close(s);
        Exit(0);
    }

    PutString("[server] done.\n");

    // Optional: wait for DONE (so server doesn’t exit before client prints)
    char done[32];
    int sm2, sb2;
    rnet_recv_fixed(s, done, 32, &sm2, &sb2);

    rnet_close(s);
    Exit(0);
}

