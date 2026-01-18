// network/reliable.cc
#include "reliable.h"
#include "system.h"     // stats, currentThread, interrupt, postOffice global etc.
#include "thread.h"
#include "utility.h"
#include <string.h>     // memcpy, memset
#include <stdio.h>      // sprintf


static long long PeerKey(int mach, int mbox) {
    return (((long long)mach) << 16) | (mbox & 0xFFFF);
}

static int MinInt(int a, int b) { return (a < b) ? a : b; }

ReliablePost::ReliablePost(PostOffice *po, int localMailbox, const char *debugName)
: postOffice(po),
  localMbox(localMailbox),
  name(debugName),
  rxThread(0),
  timerThread(0),
  lock(new Lock("reliable lock")),
  //nextSendSeq(0),
  pending(0),
  fixedQ(new SynchList()),
  varQ(new SynchList()),
  peers(new SynchList()),
  shuttingDown(false)
{
    rxThread = new Thread("reliable receiver");
    rxThread->Fork(ReceiverTrampoline, (int)this);

    timerThread = new Thread("reliable timer");
    timerThread->Fork(TimerTrampoline, (int)this);
}

ReliablePost::~ReliablePost() {
    lock->Acquire();
    shuttingDown = true;
    if (pending && !pending->done) {
        pending->acked = false;
        pending->done = true;
        pending->waiter->V();
    }
    lock->Release();
    // Like most NachOS labs, we don’t try to unblock Receive() to join threads.
}

void ReliablePost::ReceiverTrampoline(int arg) {
    ((ReliablePost*)arg)->ReceiverLoop();
}
void ReliablePost::TimerTrampoline(int arg) {
    ((ReliablePost*)arg)->TimerLoop();
}

void ReliablePost::SendRaw(int dstMachine, int dstMailbox, const char *buf, int nBytes) {
    PacketHeader pkt;
    MailHeader mail;

    pkt.to = dstMachine;

    mail.to = dstMailbox;
    mail.from = localMbox;
    mail.length = nBytes;

    postOffice->Send(pkt, mail, buf);
}

void ReliablePost::SendAck(int dstMachine, int dstMailbox, unsigned short seq) {
    char out[MaxMailSize];
    ReliableHeader h;

    h.magic = REL_MAGIC;
    h.type  = REL_ACK;
    h.flags = 0;
    h.seq   = seq;
    h.len   = 0;

    ASSERT((int)sizeof(ReliableHeader) <= MaxMailSize);
    memcpy(out, &h, sizeof(ReliableHeader));
    SendRaw(dstMachine, dstMailbox, out, (int)sizeof(ReliableHeader));
}

ReliablePost::PeerState *ReliablePost::GetOrCreatePeer(int srcMachine, int srcMailbox) {
    long long key = PeerKey(srcMachine, srcMailbox);
    PeerState *p = (PeerState*)peers->SortedGet(key);
    if (p) return p;

    p = new PeerState(srcMachine, srcMailbox);
    peers->SortedInsert((void*)p, key);
    return p;
}

void ReliablePost::EnqueueFixed(int srcMachine, int srcMailbox, const char *data, int len) {
    AppMsg *m = new AppMsg;
    m->srcMachine = srcMachine;
    m->srcMailbox = srcMailbox;
    m->len = len;
    m->data = new char[len];
    memcpy(m->data, data, len);
    fixedQ->Append((void*)m);
}

void ReliablePost::HandleVarFragment(PeerState *peer,
                                     int srcMachine, int srcMailbox,
                                     const char *frag, int fragLen, unsigned char flags)
{
    int needed = peer->partialLen + fragLen;
    if (needed > peer->partialCap) {
        int newCap = (peer->partialCap == 0) ? 128 : peer->partialCap;
        while (newCap < needed) newCap *= 2;
        char *nb = new char[newCap];
        if (peer->partial && peer->partialLen > 0) {
            memcpy(nb, peer->partial, peer->partialLen);
        }
        delete [] peer->partial;
        peer->partial = nb;
        peer->partialCap = newCap;
    }

    memcpy(peer->partial + peer->partialLen, frag, fragLen);
    peer->partialLen += fragLen;

    if (flags & REL_LAST) {
        AppMsg *m = new AppMsg;
        m->srcMachine = srcMachine;
        m->srcMailbox = srcMailbox;
        m->len = peer->partialLen;
        m->data = new char[m->len];
        memcpy(m->data, peer->partial, m->len);

        peer->partialLen = 0; // reset for next message

        varQ->Append((void*)m);
    }
}

void ReliablePost::HandleIncoming(int srcMachine, int srcMailbox,
                                  const char *buf, int nBytes)
{
    if (nBytes < (int)sizeof(ReliableHeader)) return;

    ReliableHeader h;
    memcpy(&h, buf, sizeof(ReliableHeader));
    if (h.magic != REL_MAGIC) return;

    if (h.type == REL_ACK) {
        lock->Acquire();
        if (pending && !pending->done &&
            pending->dstMachine == srcMachine &&
            pending->dstMailbox == srcMailbox &&
            pending->seq == h.seq)
        {
            pending->acked = true;
            pending->done = true;
            pending->waiter->V();
        }
        lock->Release();
        return;
    }

    if (h.type == REL_DATA) {
        // ACK every DATA (even duplicates)
        SendAck(srcMachine, srcMailbox, h.seq);

        int payloadAvail = nBytes - (int)sizeof(ReliableHeader);
        if ((int)h.len > payloadAvail) return;

        // duplicate filtering
        lock->Acquire();
        PeerState *peer = GetOrCreatePeer(srcMachine, srcMailbox);
        if (h.seq != peer->expectedSeq) {
            lock->Release();
            return; // duplicate or unexpected
        }
        peer->expectedSeq ^= 1;
        lock->Release();

        const char *payload = buf + sizeof(ReliableHeader);
        int payloadLen = (int)h.len;

        if (h.flags == 0) {
            EnqueueFixed(srcMachine, srcMailbox, payload, payloadLen);
        } else {
            lock->Acquire();
            PeerState *peer2 = GetOrCreatePeer(srcMachine, srcMailbox);
            HandleVarFragment(peer2, srcMachine, srcMailbox, payload, payloadLen, h.flags);
            lock->Release();
        }
    }
}

void ReliablePost::ReceiverLoop() {
    PacketHeader pkt;
    MailHeader mail;
    char in[MaxMailSize];

    while (true) {
        lock->Acquire();
        bool stop = shuttingDown;
        lock->Release();
        if (stop) return;

        postOffice->Receive(localMbox, &pkt, &mail, in);

        int srcMachine = pkt.from;
        int srcMailbox = mail.from;
        int nBytes = mail.length;
        if (nBytes > (int)MaxMailSize) nBytes = (int)MaxMailSize;

        HandleIncoming(srcMachine, srcMailbox, in, nBytes);
    }
}

void ReliablePost::TimerLoop() {
    while (true) {
        lock->Acquire();
        bool stop = shuttingDown;
        PendingSend *p = pending;
        lock->Release();
        if (stop) return;

        if (p) {
            lock->Acquire();
            if (pending && !pending->done) {
                int elapsed = stats->totalTicks - pending->sendTick;
                if (elapsed >= TEMPO) {
                    if (pending->retransmissions >= MAXREEMISSIONS) {
                        pending->acked = false;
                        pending->done = true;
                        pending->waiter->V();
                    } else {
                        SendRaw(pending->dstMachine, pending->dstMailbox,
                                pending->packet, pending->packetLen);
                        pending->sendTick = stats->totalTicks;
                        pending->retransmissions++;
                    }
                }
            }
            lock->Release();
        }

        currentThread->Yield();
    }
}

bool ReliablePost::SendFixed(int dstMachine, int dstMailbox, const char *data, int nBytes) {
    int maxPayload = MaxMailSize - (int)sizeof(ReliableHeader);
    if (nBytes < 0) nBytes = 0;
    if (nBytes > maxPayload) nBytes = maxPayload;

    lock->Acquire();
    while (pending != 0) {
        lock->Release();
        currentThread->Yield();
        lock->Acquire();
    }

    PendingSend *p = new PendingSend;
    p->dstMachine = dstMachine;
    p->dstMailbox = dstMailbox;
    //p->seq = nextSendSeq;
    //nextSendSeq ^= 1;
    PeerState *dst = GetOrCreatePeer(dstMachine, dstMailbox);
p->seq = dst->nextSendSeqToPeer;
dst->nextSendSeqToPeer ^= 1;
    p->sendTick = stats->totalTicks;
    p->retransmissions = 0;
    p->acked = false;
    p->done = false;
    p->waiter = new Semaphore("reliable send waiter", 0);

    ReliableHeader h;
    h.magic = REL_MAGIC;
    h.type  = REL_DATA;
    h.flags = 0;
    h.seq   = p->seq;
    h.len   = (unsigned short)nBytes;

    memcpy(p->packet, &h, sizeof(ReliableHeader));
    if (nBytes > 0) memcpy(p->packet + sizeof(ReliableHeader), data, nBytes);
    p->packetLen = (int)sizeof(ReliableHeader) + nBytes;

    pending = p;
    lock->Release();

    SendRaw(dstMachine, dstMailbox, p->packet, p->packetLen);

    p->waiter->P(); // wait for ACK or failure

    lock->Acquire();
    bool ok = p->acked;
    pending = 0;
    lock->Release();

    delete p->waiter;
    delete p;
    return ok;
}

int ReliablePost::ReceiveFixed(char *outBuffer, int outCapacity,
                              int *outSrcMachine, int *outSrcMailbox)
{
    AppMsg *m = (AppMsg*)fixedQ->Remove();

    if (outSrcMachine) *outSrcMachine = m->srcMachine;
    if (outSrcMailbox) *outSrcMailbox = m->srcMailbox;

    int n = MinInt(m->len, outCapacity);
    if (n > 0) memcpy(outBuffer, m->data, n);

    delete [] m->data;
    delete m;
    return n;
}

bool ReliablePost::SendVar(int dstMachine, int dstMailbox, const char *data, int nBytes) {
    if (nBytes < 0) nBytes = 0;

    int maxPayload = MaxMailSize - (int)sizeof(ReliableHeader);
    if (maxPayload <= 0) return false;

    // send fragments sequentially
    int offset = 0;
    while (offset < nBytes) {
        int chunk = MinInt(maxPayload, nBytes - offset);
        bool last = (offset + chunk == nBytes);

        // Build one fragment packet and send reliably (reuse SendFixed mechanism with flags)
        lock->Acquire();
        while (pending != 0) {
            lock->Release();
            currentThread->Yield();
            lock->Acquire();
        }

        PendingSend *p = new PendingSend;
        p->dstMachine = dstMachine;
        p->dstMailbox = dstMailbox;
        //p->seq = nextSendSeq;
        //nextSendSeq ^= 1;
	PeerState *dst = GetOrCreatePeer(dstMachine, dstMailbox);
p->seq = dst->nextSendSeqToPeer;
dst->nextSendSeqToPeer ^= 1;
        p->sendTick = stats->totalTicks;
        p->retransmissions = 0;
        p->acked = false;
        p->done = false;
        p->waiter = new Semaphore("reliable var send waiter", 0);

        ReliableHeader h;
        h.magic = REL_MAGIC;
        h.type  = REL_DATA;
        h.flags = last ? REL_LAST : REL_MORE;
        h.seq   = p->seq;
        h.len   = (unsigned short)chunk;

        memcpy(p->packet, &h, sizeof(ReliableHeader));
        if (chunk > 0) memcpy(p->packet + sizeof(ReliableHeader), data + offset, chunk);
        p->packetLen = (int)sizeof(ReliableHeader) + chunk;

        pending = p;
        lock->Release();

        SendRaw(dstMachine, dstMailbox, p->packet, p->packetLen);

        p->waiter->P();

        lock->Acquire();
        bool ok = p->acked;
        pending = 0;
        lock->Release();

        delete p->waiter;
        delete p;

        if (!ok) return false;
        offset += chunk;
    }

    // empty message => send one LAST fragment with len 0
    if (nBytes == 0) {
        ReliableHeader h;
        h.magic = REL_MAGIC;
        h.type  = REL_DATA;
        h.flags = REL_LAST;
        h.seq   = 0; // will be overwritten by send logic

        // simplest: send fixed with zero bytes but with LAST flag:
        // we’ll just do the same loop with chunk=0 once.

        lock->Acquire();
        while (pending != 0) {
            lock->Release();
            currentThread->Yield();
            lock->Acquire();
        }

        PendingSend *p = new PendingSend;
        p->dstMachine = dstMachine;
        p->dstMailbox = dstMailbox;
        //p->seq = nextSendSeq;
        //nextSendSeq ^= 1;
	PeerState *dst = GetOrCreatePeer(dstMachine, dstMailbox);
p->seq = dst->nextSendSeqToPeer;
dst->nextSendSeqToPeer ^= 1;
        p->sendTick = stats->totalTicks;
        p->retransmissions = 0;
        p->acked = false;
        p->done = false;
        p->waiter = new Semaphore("reliable empty var send waiter", 0);

        h.seq = p->seq;
        h.len = 0;

        memcpy(p->packet, &h, sizeof(ReliableHeader));
        p->packetLen = (int)sizeof(ReliableHeader);

        pending = p;
        lock->Release();

        SendRaw(dstMachine, dstMailbox, p->packet, p->packetLen);
        p->waiter->P();

        lock->Acquire();
        bool ok = p->acked;
        pending = 0;
        lock->Release();

        delete p->waiter;
        delete p;

        if (!ok) return false;
    }

    return true;
}

void ReliablePost::ReceiveVar(char **outData, int *outLen,
                             int *outSrcMachine, int *outSrcMailbox)
{
    AppMsg *m = (AppMsg*)varQ->Remove();

    if (outSrcMachine) *outSrcMachine = m->srcMachine;
    if (outSrcMailbox) *outSrcMailbox = m->srcMailbox;

    *outLen = m->len;
    *outData = m->data; // transfer ownership
    delete m;
}

