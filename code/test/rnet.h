// test/rnet.h
#ifndef RNET_H
#define RNET_H

#include "syscall.h"

static inline int rnet_socket(int localMailbox) {
    RNetArgs a;
    a.a0 = localMailbox;
    return RNetSocket(&a);
}

static inline int rnet_close(int h) {
    RNetArgs a;
    a.a0 = h;
    return RNetClose(&a);
}

static inline int rnet_send_fixed(int h, int dstM, int dstB, const void* buf, int len) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = dstM;
    a.a2 = dstB;
    a.a3 = (int)buf;
    a.a4 = len;
    return RNetSendFixed(&a);
}

static inline int rnet_recv_fixed(int h, void* buf, int cap, int* srcM, int* srcB) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = (int)buf;
    a.a2 = cap;
    a.a3 = (int)srcM;
    a.a4 = (int)srcB;
    return RNetRecvFixed(&a); // returns bytes
}

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
static inline int rnet_recv_var_begin(int h, int* outLen, int* srcM, int* srcB) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = (int)outLen;
    a.a2 = (int)srcM;
    a.a3 = (int)srcB;
    return RNetRecvVarBegin(&a);
}

static inline int rnet_recv_var_read(int h, void* buf, int cap) {
    RNetArgs a;
    a.a0 = h;
    a.a1 = (int)buf;
    a.a2 = cap;
    return RNetRecvVarRead(&a); // returns bytes copied
}

static inline int rnet_recv_var_end(int h) {
    RNetArgs a;
    a.a0 = h;
    return RNetRecvVarEnd(&a);
}

#endif

