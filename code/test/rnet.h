#ifndef RNET_H
#define RNET_H

#include "syscall.h"

/*
 * Inline wrappers to the RNet system calls
 */

/* rnet_socket:
 *	opens a socket (handle in the kernel) and returns the descriptor of that socket
 */
static inline int rnet_socket(int localMailbox) {
    RNetArgs a;
    a.a0 = localMailbox;
    return RNetSocket(&a);
}

/* rnet_close:
 *	closes the socket (cleans the handle in the kernel) and returns status (success: 0, fail: -1)
 */
static inline int rnet_close(int h) {
    RNetArgs a;
    a.a0 = h;
    return RNetClose(&a);
}

/* rnet_send_fixed:
 *	sends fixed data packet and returns teh status
 */
static inline int rnet_send_fixed(int h, int dstM, int dstB, const void* buf, int len) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = dstM;
    a.a2 = dstB;
    a.a3 = (int)buf;
    a.a4 = len;
    return RNetSendFixed(&a);
}

/* rnet_recv_fixed:
 *	received fixed sized data and returns the number of bytes received (or -1 on fail)
 */
static inline int rnet_recv_fixed(int h, void* buf, int cap, int* srcM, int* srcB) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = (int)buf;
    a.a2 = cap;
    a.a3 = (int)srcM;
    a.a4 = (int)srcB;
    return RNetRecvFixed(&a); // returns bytes
}

/* rnet_send_var:
 *	send variable sized data and return the status of the send
 */
static inline int rnet_send_var(int h, int dstM, int dstB, const void* buf, int len) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = dstM;
    a.a2 = dstB;
    a.a3 = (int)buf;
    a.a4 = len;
    return RNetSendVar(&a);
}

// 2-phase recvvar
/* rnet_recv_var_begin:
 *	starts the receiving process of variable sized data and returns the status
 */
static inline int rnet_recv_var_begin(int h, int* outLen, int* srcM, int* srcB) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = (int)outLen;
    a.a2 = (int)srcM;
    a.a3 = (int)srcB;
    return RNetRecvVarBegin(&a);
}
/* rnet_recv_var_read:
 *	copy the received data fromm the kernel and return the number of bytes copied/received
 *	returns -1 on failure
 */
static inline int rnet_recv_var_read(int h, void* buf, int cap) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = (int)buf;
    a.a2 = cap;
    return RNetRecvVarRead(&a); // returns bytes copied
}

/* rnet_recv_var_end:
 *	cleans the handle data field to allow other data receives
 */
static inline int rnet_recv_var_end(int h) {
    RNetArgs a;
    a.a0 = h;
    return RNetRecvVarEnd(&a);
}

/* rnet_recv_var:
 *	a wrapper around the 2-phase recvvar to make it more convenient for the user
 *	in case the peers can negotiate how large the user buffer should be (for example for
 *	file transfer, report how large the file is beforehand)
 *	returns -1 on failure or number of bytes received on success.
 */
static inline int rnet_recv_var(int h,
                                void* buf, int cap,
                                int* outLen,
                                int* srcM, int* srcB)
{
    int len = 0, sm = -1, sb = -1;
    if (rnet_recv_var_begin(h, &len, &sm, &sb) < 0) return -1;

    int n = rnet_recv_var_read(h, buf, cap);
    int endrc = rnet_recv_var_end(h);

    if (srcM) *srcM = sm;
    if (srcB) *srcB = sb;
    if (outLen) *outLen = len;

    if (endrc < 0) return -1;
    return n; // possibly truncated if cap < len
}

#endif

