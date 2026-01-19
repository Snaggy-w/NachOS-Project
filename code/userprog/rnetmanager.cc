// userprog/rnetmanager.cc
#ifdef NETWORK

#include "synch.h"
#include "reliable.h"
#include "rnetmanager.h"
#include "system.h"   // machine, postOffice
#include "thread.h"
#include <string.h>

static bool CopyBytesFromUser(int userAddr, char* dst, int n) {
    for (int i = 0; i < n; i++) {
        int v;
        if (!machine->ReadMem(userAddr + i, 1, &v)) return false;
        dst[i] = (char)(v & 0xFF);
    }
    return true;
}

static bool CopyBytesToUser(int userAddr, const char* src, int n) {
    for (int i = 0; i < n; i++) {
        if (!machine->WriteMem(userAddr + i, 1, (int)(unsigned char)src[i])) return false;
    }
    return true;
}

static bool WriteIntToUser(int userAddr, int value) {
    return machine->WriteMem(userAddr, (int)sizeof(int), value);
}

RNetManager::RNetManager()
: lock(new Lock("rnet manager lock"))
{
    for (int i = 0; i < MAX_RNET_HANDLES; i++) {
        table[i].used = false;
        table[i].localMailbox = -1;
        table[i].rp = 0;

        table[i].haveVar = false;
        table[i].varData = 0;
        table[i].varLen = 0;
        table[i].varSrcM = 0;
        table[i].varSrcB = 0;
    }
}

RNetManager::~RNetManager() {
    lock->Acquire();
    for (int i = 0; i < MAX_RNET_HANDLES; i++) {
        if (!table[i].used) continue;

        if (table[i].haveVar && table[i].varData) {
            delete [] table[i].varData;
        }
        table[i].haveVar = false;
        table[i].varData = 0;
        table[i].varLen = 0;

        delete table[i].rp;
        table[i].rp = 0;

        table[i].used = false;
        table[i].localMailbox = -1;
    }
    lock->Release();
    delete lock;
    lock = 0;
}

bool RNetManager::Valid(int h) const {
    return (h >= 0 && h < MAX_RNET_HANDLES && table[h].used && table[h].rp != 0);
}

int RNetManager::Socket(int localMailbox) {
    lock->Acquire();

    // Optional: prevent same mailbox twice within the same process
    for (int i = 0; i < MAX_RNET_HANDLES; i++) {
        if (table[i].used && table[i].localMailbox == localMailbox) {
            lock->Release();
            return -1;
        }
    }

    int slot = -1;
    for (int i = 0; i < MAX_RNET_HANDLES; i++) {
        if (!table[i].used) { slot = i; break; }
    }
    if (slot < 0) { lock->Release(); return -1; }

    table[slot].used = true;
    table[slot].localMailbox = localMailbox;
    table[slot].haveVar = false;
    table[slot].varData = 0;
    table[slot].varLen = 0;

    table[slot].rp = new ReliablePost(postOffice, localMailbox, "rnet");

    lock->Release();
    return slot;
}

int RNetManager::Close(int h) {
    lock->Acquire();
    if (!Valid(h)) { lock->Release(); return -1; }

    if (table[h].haveVar && table[h].varData) {
        delete [] table[h].varData;
    }
    table[h].haveVar = false;
    table[h].varData = 0;
    table[h].varLen = 0;

    delete table[h].rp;
    table[h].rp = 0;

    table[h].used = false;
    table[h].localMailbox = -1;

    lock->Release();
    return 0;
}

int RNetManager::SendFixed(int h, int dstM, int dstB, int userBuf, int len) {
    if (len < 0) return -1;

    lock->Acquire();
    if (!Valid(h)) { lock->Release(); return -1; }
    ReliablePost* rp = table[h].rp;
    lock->Release();

    char* kbuf = new char[len > 0 ? len : 1];
    if (len > 0 && !CopyBytesFromUser(userBuf, kbuf, len)) {
        delete [] kbuf;
        return -1;
    }

    bool ok = rp->SendFixed(dstM, dstB, kbuf, len);
    delete [] kbuf;
    return ok ? 0 : -1;
}

int RNetManager::RecvFixed(int h, int userBuf, int cap, int userSrcM, int userSrcB) {
    if (cap < 0) return -1;

    lock->Acquire();
    if (!Valid(h)) { lock->Release(); return -1; }
    ReliablePost* rp = table[h].rp;
    lock->Release();

    char* tmp = new char[cap > 0 ? cap : 1];
    int srcM = 0, srcB = 0;
    int n = rp->ReceiveFixed(tmp, cap, &srcM, &srcB);

    if (n > 0 && !CopyBytesToUser(userBuf, tmp, n)) {
        delete [] tmp;
        return -1;
    }
    delete [] tmp;

    if (!WriteIntToUser(userSrcM, srcM)) return -1;
    if (!WriteIntToUser(userSrcB, srcB)) return -1;

    return n; // bytes copied
}

int RNetManager::SendVar(int h, int dstM, int dstB, int userBuf, int len) {
    if (len < 0) return -1;

    lock->Acquire();
    if (!Valid(h)) { lock->Release(); return -1; }
    ReliablePost* rp = table[h].rp;
    lock->Release();

    char* kbuf = new char[len > 0 ? len : 1];
    if (len > 0 && !CopyBytesFromUser(userBuf, kbuf, len)) {
        delete [] kbuf;
        return -1;
    }

    bool ok = rp->SendVar(dstM, dstB, kbuf, len);
    delete [] kbuf;
    return ok ? 0 : -1;
}

int RNetManager::RecvVarBegin(int h, int userOutLen, int userSrcM, int userSrcB) {
    lock->Acquire();
    if (!Valid(h)) { lock->Release(); return -1; }
    if (table[h].haveVar) { lock->Release(); return -1; } // must End first
    ReliablePost* rp = table[h].rp;
    lock->Release();

    char* data = 0;
    int len = 0;
    int srcM = 0, srcB = 0;

    rp->ReceiveVar(&data, &len, &srcM, &srcB); // blocks

    lock->Acquire();
    // store inside handle
    table[h].haveVar = true;
    table[h].varData = data;
    table[h].varLen = len;
    table[h].varSrcM = srcM;
    table[h].varSrcB = srcB;
    lock->Release();

    if (!WriteIntToUser(userOutLen, len)) return -1;
    if (!WriteIntToUser(userSrcM, srcM)) return -1;
    if (!WriteIntToUser(userSrcB, srcB)) return -1;

    return 0;
}

int RNetManager::RecvVarRead(int h, int userBuf, int cap) {
    if (cap < 0) return -1;

    lock->Acquire();
    if (!Valid(h) || !table[h].haveVar) { lock->Release(); return -1; }

    int n = (cap < table[h].varLen) ? cap : table[h].varLen;
    const char* src = table[h].varData;
    lock->Release();

    if (n > 0 && !CopyBytesToUser(userBuf, src, n)) return -1;
    return n;
}

int RNetManager::RecvVarEnd(int h) {
    lock->Acquire();
    if (!Valid(h) || !table[h].haveVar) { lock->Release(); return -1; }

    delete [] table[h].varData;
    table[h].varData = 0;
    table[h].varLen = 0;
    table[h].haveVar = false;

    lock->Release();
    return 0;
}

#endif // NETWORK

