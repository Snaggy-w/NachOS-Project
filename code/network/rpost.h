#ifndef RPOST_H
#define RPOST_H

#include "post.h"
#include "synch.h"
#include "thread.h"

#ifndef RPOST_BOX
#define RPOST_BOX 2
#endif

#ifndef TEMPO_TICKS
#define TEMPO_TICKS (5 * NetworkTime)
#endif

#ifndef MAXREEMISSIONS
#define MAXREEMISSIONS 8
#endif

// Ports are a local demux mechanism for your reliable layer
#ifndef RPOST_NPORTS
#define RPOST_NPORTS 16
#endif

// Max payload in one reliable message (Part II)
struct RHeader;
#define RPOST_MAX_PAYLOAD ((int)MaxMailSize - (int)sizeof(RHeader))

enum RType {
    R_DATA = 1,
    R_ACK  = 2
};

struct RHeader {
    unsigned char type;     // R_DATA / R_ACK
    unsigned char srcPort;  // sender local port
    unsigned char dstPort;  // receiver local port
    unsigned char seq;      // 0/1 alternating bit
    unsigned short len;     // payload bytes after header
};

// Delivered message node (simple linked queue)
struct RMessage {
    NetworkAddress from;
    unsigned char fromPort;
    int len;
    char *data;
    RMessage *next;
    RMessage(NetworkAddress f, unsigned char fp, const char *buf, int n);
    ~RMessage();
};

class ReliablePostOffice {
public:
    ReliablePostOffice(PostOffice *po, int box = RPOST_BOX, int nPorts = RPOST_NPORTS);
    ~ReliablePostOffice();

    // Part II: fixed-size reliable send
    bool Send(NetworkAddress toAddr, int toPort, int fromPort, const char *data, int len);

    // Part II: receive one delivered message for a local port (blocks)
    bool Receive(int localPort, NetworkAddress *fromAddr, int *fromPort,
                 char *out, int maxLen, int *outLen);

private:
    PostOffice *po;
    int box;
    int nPorts;

    // Receiver thread
    Thread *rxThread;
    static void RxHelper(int arg);
    void RxLoop();

    // Per-port incoming queue
    struct PortQ {
        Lock lock;
        Condition cv;
        RMessage *head;
        RMessage *tail;
        PortQ(const char *name);
        ~PortQ();
        void Push(RMessage *m);
        RMessage *PopBlocking();
    };
    PortQ **ports;

    // Per-flow state (peerAddr + myPort + peerPort)
    struct FlowState {
        bool used;
        NetworkAddress peer;
        unsigned char myPort;
        unsigned char peerPort;

        // send side
        unsigned char sendSeq;
        bool waitingAck;
        unsigned char waitingSeq;
        int generation;
        Semaphore ackSem;
        Lock lock;

        // recv side
        unsigned char expectSeq;

        FlowState();
    };

    // Fixed table: at most nPorts * 255 flows (peerPort is 0..255)
    // We index by (myPort, peerPort) and store peer address inside.
    FlowState *flows;

    FlowState *GetFlow(NetworkAddress peer, int myPort, int peerPort);

    // timeout trampoline
    struct TimeoutEvent {
        FlowState *fs;
        int gen;
        unsigned char seq;
    };
    static void TimeoutTrampoline(int arg);

    // helpers
    void SendRaw(NetworkAddress to, const RHeader &h, const char *payload, int len);
    void SendAck(NetworkAddress to, unsigned char srcPort, unsigned char dstPort, unsigned char seq);
};

#endif

