#include "syscall.h"
#include "rnet.h"
#include "libstr.h"

static void FillPing(char *buf, int cap, int seq) {
    // keep it simple: "PING <seq>"
    int n = 0;
    const char *p = "PING ";
    while (*p && n < cap - 1) buf[n++] = *p++;
    // append seq as decimal
    char tmp[16];
    int x = seq;
    int t = 0;
    if (x == 0) tmp[t++] = '0';
    while (x > 0 && t < 15) {
        tmp[t++] = (char)('0' + (x % 10));
        x /= 10;
    }
    // reverse digits
    for (int i = t - 1; i >= 0 && n < cap - 1; i--) buf[n++] = tmp[i];
    buf[n] = '\0';
}

int main() {
    PutString("=== RNet Ping/Pong User Program ===\n");
    PutString("Enter target machine id (e.g. 0 or 1): ");

    char line[64];
    GetString(line, 64);
    int dstM = atoi(line);

    // We use mailbox 0 on the remote side, and bind our local socket to mailbox 0 as well.
    // (You can change local mailbox to 1 if you want separation.)
    int localMailbox = 0;
    int remoteMailbox = 0;

    int s = rnet_socket(localMailbox);
    if (s < 0) {
        PutString("[user] rnet_socket failed\n");
        Exit(-1);
    }

    PutString("[user] Socket created. Starting ping/pong...\n");

    // Decide roles:
    // - machine 0 acts as initiator if dstM != 0, otherwise if you run on machine 1 and target 0, you're responder.
    // Since user mode doesn't know its machine id, we just do:
    // - first try to send a ping, then wait for pong; if send fails, we become responder.
    // In practice, easiest is run the same program on both machines:
    //   machine A: enters B as target
    //   machine B: enters A as target
    //
    // We'll implement symmetric behavior: always do N iterations:
    // Each iteration:
    //   send PING i
    //   receive message (PING or PONG), respond appropriately.
    //
    const int N = 10;

    for (int i = 0; i < N; i++) {
        char msg[64];
        FillPing(msg, 64, i);

        // Send ping
        int rc = rnet_send_fixed(s, dstM, remoteMailbox, msg, strlen(msg) + 1);
        if (rc < 0) {
            PutString("[user] send_fixed failed (you may have too low reliability / no peer)\n");
            break;
        }

        // Wait for reply (blocking)
        char in[128];
        int srcM = -1, srcB = -1;
        int n = rnet_recv_fixed(s, in, 128, &srcM, &srcB);
        if (n < 0) {
            PutString("[user] recv_fixed failed\n");
            break;
        }

        PutString("[user] got: ");
        PutString(in);
        PutString(" (from machine ");
        PutInt(srcM);
        PutString(", box ");
        PutInt(srcB);
        PutString(")\n");

        // If we got PING, reply with PONG
        // If we got PONG, just continue
        if (in[0] == 'P' && in[1] == 'I' && in[2] == 'N' && in[3] == 'G') {
            char pong[64];
            // "PONG <seq>"
            strcpy(pong, "PONG ", 64);
            // append the same seq number by copying after "PING "
            // (very simple: take substring in+5)
            int j = 5;
            int k = 5;
            while (in[j] && k < 63) {
                pong[k++] = in[j++];
            }
            pong[k] = '\0';

            rnet_send_fixed(s, srcM, remoteMailbox, pong, strlen(pong) + 1);
        }
    }

    rnet_close(s);
    PutString("[user] Done.\n");
    Exit(0);
}
