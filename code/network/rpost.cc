#include "rpost.h"
#include "system.h"
#include <string.h>

// -------------------- RMessage --------------------
RMessage::RMessage(NetworkAddress f, unsigned char fp, const char *buf, int n)
    : from(f), fromPort(fp), len(n), next(NULL) {
    data = new char[n];
    memcpy(data, buf, n);
}

RMessage::~RMessage() {
    delete[] data;
}

// -------------------- PortQ --------------------
ReliablePostOffice::PortQ::PortQ(const char *name)
    : lock(name), cv(name), head(NULL), tail(NULL) {}

ReliablePostOffice::PortQ::~PortQ() {
    // drain queue
    RMessage *p = head;
    while (p) {
        RMessage *n = p->next;
        delete p;
        p = n;
    }
    head = tail = NULL;
}

void ReliablePostOffice::PortQ::Push(RMessage *m) {
    // caller holds lock
    m->next = NULL;
    if (!tail) {
        head = tail = m;
    } else {
        tail->next = m;
        tail = m;
    }
    cv.Signal(&lock);
}

RMessage *ReliablePostOffice::PortQ::PopBlocking() {
    // caller holds lock
    while (!head) cv.Wait(&lock);
    RMessage *m = head;
    head = head->next;
    if (!head) tail = NULL;
    m->next = NULL;
    return m;
}

// -------------------- FlowState --------------------
ReliablePostOffice::FlowState::FlowState()
    : used(false),
      peer(0),
      myPort(0),
      peerPort(0),
      sendSeq(0),
      waitingAck(false),
      waitingSeq(0),
      generation(0),
      ackSem("rpost-ack", 0),
      lock("rpost-flow"),
      expectSeq(0) {}

// -------------------- Trampolines --------------------
void ReliablePostOffice::RxHelper(int arg) {
    ((ReliablePostOffice*)arg)->RxLoop();
}

void ReliablePostOffice::TimeoutTrampoline(int arg) {
    TimeoutEvent *ev = (TimeoutEvent*)arg;
    FlowState *fs = ev->fs;

    fs->lock.Acquire();
    if (fs->waitingAck && fs->generation == ev->gen && fs->waitingSeq == ev->seq) {
        fs->waitingAck = false; // timeout -> stop waiting
        fs->ackSem.V();         // wake sender to retry
    }
    fs->lock.Release();

    delete ev;
}

// -------------------- ReliablePostOffice --------------------
ReliablePostOffice::ReliablePostOffice(PostOffice *po_, int box_, int nPorts_)
    : po(po_), box(box_), nPorts(nPorts_), rxThread(NULL), ports(NULL), flows(NULL) {

    if (nPorts <= 0) nPorts = 1;
    if (nPorts > 64) nPorts = 64; // keep table reasonable

    // port queues
    ports = new PortQ*[nPorts];
    for (int i = 0; i < nPorts; i++) ports[i] = new PortQ("rpost-port");

    // flow table: [myPort][peerPort] => FlowState
    flows = new FlowState[nPorts * 256];

    rxThread = new Thread("rpost-rx");
    rxThread->Fork(RxHelper, (int)this);
}

ReliablePostOffice::~ReliablePostOffice() {
    if (ports) {
        for (int i = 0; i < nPorts; i++) delete ports[i];
        delete[] ports;
    }
    ports = NULL;

    delete[] flows;
    flows = NULL;
}

ReliablePostOffice::FlowState *ReliablePostOffice::GetFlow(NetworkAddress peer, int myPort, int peerPort) {
    if (myPort < 0) myPort = 0;
    if (myPort >= nPorts) myPort = nPorts - 1;

    if (peerPort < 0) peerPort = 0;
    if (peerPort > 255) peerPort = 255;

    FlowState *fs = &flows[myPort * 256 + peerPort];

    fs->lock.Acquire();
    if (!fs->used) {
        fs->used = true;
        fs->peer = peer;
        fs->myPort = (unsigned char)myPort;
        fs->peerPort = (unsigned char)peerPort;
        fs->sendSeq = 0;
        fs->expectSeq = 0;
        fs->waitingAck = false;
        fs->generation = 0;
    } else {
        // If table entry exists but peer differs, we still allow it:
        // overwrite peer (simple policy). If you need multiple peers per same ports, increase keying.
        fs->peer = peer;
    }
    fs->lock.Release();

    return fs;
}

// -------------------- Wire helpers --------------------
void ReliablePostOffice::SendRaw(NetworkAddress to, const RHeader &h, const char *payload, int len) {
    char buf[MaxMailSize];
    ASSERT(((int)sizeof(RHeader) + (int)len) <= (int)MaxMailSize);

    memcpy(buf, &h, sizeof(RHeader));
    if (len > 0) memcpy(buf + sizeof(RHeader), payload, len);

    PacketHeader pkt;
    MailHeader mail;
    pkt.to = to;
    mail.to = box;
    mail.from = box;
    mail.length = sizeof(RHeader) + len;

    po->Send(pkt, mail, buf);
}

void ReliablePostOffice::SendAck(NetworkAddress to, unsigned char srcPort, unsigned char dstPort, unsigned char seq) {
    RHeader h;
    h.type = (unsigned char)R_ACK;
    h.srcPort = srcPort; // receiver port
    h.dstPort = dstPort; // sender port
    h.seq = seq;
    h.len = 0;
    SendRaw(to, h, NULL, 0);
}

// -------------------- Public API --------------------
bool ReliablePostOffice::Send(NetworkAddress toAddr, int toPort, int fromPort, const char *data, int len) {
    if (fromPort < 0 || fromPort >= nPorts) return false;
    if (toPort < 0 || toPort > 255) return false;
    if (len < 0 || len > RPOST_MAX_PAYLOAD) return false;

    FlowState *fs = GetFlow(toAddr, fromPort, toPort);

    // serialize per-flow send
    fs->lock.Acquire();

    for (int attempt = 0; attempt <= MAXREEMISSIONS; ++attempt) {
        unsigned char seq = fs->sendSeq;

        // waiting BEFORE send (avoid early-ACK race)
        fs->waitingAck = true;
        fs->waitingSeq = seq;
        int gen = ++fs->generation;

        // send DATA (PostOffice::Send can block -> release lock around it)
        RHeader h;
        h.type = (unsigned char)R_DATA;
        h.srcPort = (unsigned char)fromPort;
        h.dstPort = (unsigned char)toPort;
        h.seq = seq;
        h.len = (unsigned short)len;

        fs->lock.Release();
        SendRaw(toAddr, h, data, len);

        TimeoutEvent *ev = new TimeoutEvent;
        ev->fs = fs;
        ev->gen = gen;
        ev->seq = seq;
        interrupt->Schedule(TimeoutTrampoline, (int)ev, TEMPO_TICKS, TimerInt);

        fs->ackSem.P();

        fs->lock.Acquire();
        if (fs->sendSeq != seq) { // ACK handler flips sendSeq on success
            fs->lock.Release();
            return true;
        }
        // else timeout -> retry
    }

    fs->lock.Release();
    return false;
}

bool ReliablePostOffice::Receive(int localPort, NetworkAddress *fromAddr, int *fromPort,
                                 char *out, int maxLen, int *outLen) {
    if (localPort < 0 || localPort >= nPorts) return false;

    PortQ *pq = ports[localPort];
    pq->lock.Acquire();
    RMessage *m = pq->PopBlocking();
    pq->lock.Release();

    *fromAddr = m->from;
    *fromPort = (int)m->fromPort;

    int n = m->len;
    if (n > maxLen) n = maxLen;
    memcpy(out, m->data, n);
    *outLen = n;

    delete m;
    return true;
}

// -------------------- RX loop --------------------
void ReliablePostOffice::RxLoop() {
    PacketHeader pkt;
    MailHeader mail;
    char buf[MaxMailSize];

    for (;;) {
        po->Receive(box, &pkt, &mail, buf);
        if (mail.length < (int)sizeof(RHeader)) continue;

        RHeader h;
        memcpy(&h, buf, sizeof(RHeader));
        const char *payload = buf + sizeof(RHeader);

        if (h.type == (unsigned char)R_DATA) {
            int myPort = (int)h.dstPort;
            int peerPort = (int)h.srcPort;

            // Always ACK, even duplicates (prevents sender getting stuck)
            SendAck(pkt.from, h.dstPort, h.srcPort, h.seq);

            if (myPort < 0 || myPort >= nPorts) continue;

            FlowState *fs = GetFlow(pkt.from, myPort, peerPort);

            bool deliver = false;
            fs->lock.Acquire();
            if (h.seq == fs->expectSeq) {
                deliver = true;
                fs->expectSeq ^= 1;
            }
            fs->lock.Release();

            if (deliver) {
                int n = (int)h.len;
                if (n < 0) n = 0;
                if (n > RPOST_MAX_PAYLOAD) n = RPOST_MAX_PAYLOAD;

                RMessage *m = new RMessage(pkt.from, h.srcPort, payload, n);

                PortQ *pq = ports[myPort];
                pq->lock.Acquire();
                pq->Push(m);
                pq->lock.Release();
            }
        } else if (h.type == (unsigned char)R_ACK) {
            int myPort = (int)h.dstPort;   // our fromPort
            int peerPort = (int)h.srcPort; // their port

            if (myPort < 0 || myPort >= nPorts) continue;

            FlowState *fs = GetFlow(pkt.from, myPort, peerPort);

            fs->lock.Acquire();
            if (fs->waitingAck && fs->waitingSeq == h.seq) {
                fs->waitingAck = false;
                fs->sendSeq ^= 1;   // success
                fs->ackSem.V();     // wake sender
            }
            fs->lock.Release();
        }
    }
}

