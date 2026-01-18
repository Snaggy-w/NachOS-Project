// network/reliable.h
#ifndef RELIABLE_H
#define RELIABLE_H

#include "copyright.h"
#include "synch.h"
#include "synchlist.h"

#include "network.h"   // PacketHeader, MaxPacketSize
#include "post.h"      // PostOffice, MailHeader, MaxMailSize

// ---- knobs (override with -DTEMPO=... -DMAXREEMISSIONS=... if you want) ----
#ifndef TEMPO
#define TEMPO 2000
#endif

#ifndef MAXREEMISSIONS
#define MAXREEMISSIONS 10
#endif

#define REL_MAGIC 0xC0DE
#define REL_DATA  1
#define REL_ACK   2

#define REL_MORE  0x1
#define REL_LAST  0x2

// Header placed inside PostOffice payload (the 'data' argument).
struct ReliableHeader {
    unsigned short magic;   // REL_MAGIC
    unsigned char  type;    // REL_DATA / REL_ACK
    unsigned char  flags;   // REL_MORE / REL_LAST (0 for fixed)
    unsigned short seq;     // alternating bit 0/1
    unsigned short len;     // bytes of user payload following this header
};

class ReliablePost {
  public:
    ReliablePost(PostOffice *po, int localMailbox, const char *debugName = "reliable");
    ~ReliablePost();

    // Part II: fixed size (<= MaxMailSize - sizeof(ReliableHeader))
    bool SendFixed(int dstMachine, int dstMailbox, const char *data, int nBytes);

    // Blocks until one fixed msg arrives. Returns bytes copied.
    int ReceiveFixed(char *outBuffer, int outCapacity,
                     int *outSrcMachine = 0, int *outSrcMailbox = 0);

    // Part III: variable size
    bool SendVar(int dstMachine, int dstMailbox, const char *data, int nBytes);

    // Blocks until a complete var msg arrives. Allocates buffer; caller delete[].
    void ReceiveVar(char **outData, int *outLen,
                    int *outSrcMachine = 0, int *outSrcMailbox = 0);

  private:
    struct AppMsg {
        int srcMachine;
        int srcMailbox;
        int len;
        char *data; // owned
    };

   /* struct PeerState {
        int srcMachine;
        int srcMailbox;
        unsigned short expectedSeq;

        // Reassembly buffer for var-size
        char *partial;
        int partialLen;
        int partialCap;

        PeerState(int m, int b)
          : srcMachine(m), srcMailbox(b), expectedSeq(0),
            partial(0), partialLen(0), partialCap(0) {}
    };*/
    struct PeerState {
    int srcMachine;
    int srcMailbox;

    unsigned short expectedSeq;        // next expected incoming seq (from peer)
    unsigned short nextSendSeqToPeer;  // next outgoing seq (to peer)

    char *partial;
    int partialLen;
    int partialCap;

    PeerState(int m, int b)
      : srcMachine(m), srcMailbox(b),
        expectedSeq(0), nextSendSeqToPeer(0),
        partial(0), partialLen(0), partialCap(0) {}
    };

    struct PendingSend {
        int dstMachine;
        int dstMailbox;
        unsigned short seq;

        int sendTick;
        int retransmissions;
        bool acked;
        bool done;

        int packetLen;
        char packet[MaxMailSize];  // must fit in one mail payload

        Semaphore *waiter;
    };

    static void ReceiverTrampoline(int arg);
    static void TimerTrampoline(int arg);

    void ReceiverLoop();
    void TimerLoop();

    void SendRaw(int dstMachine, int dstMailbox, const char *buf, int nBytes);
    void SendAck(int dstMachine, int dstMailbox, unsigned short seq);

    PeerState *GetOrCreatePeer(int srcMachine, int srcMailbox);

    void HandleIncoming(int srcMachine, int srcMailbox,
                        const char *buf, int nBytes);

    void EnqueueFixed(int srcMachine, int srcMailbox, const char *data, int len);
    void HandleVarFragment(PeerState *peer,
                           int srcMachine, int srcMailbox,
                           const char *frag, int fragLen, unsigned char flags);

  private:
    PostOffice *postOffice;
    int localMbox;
    const char *name;

    Thread *rxThread;
    Thread *timerThread;

    Lock *lock;
    //unsigned short nextSendSeq;

    PendingSend *pending;

    SynchList *fixedQ; // AppMsg*
    SynchList *varQ;   // AppMsg*
    SynchList *peers;  // PeerState* sorted by key

    bool shuttingDown;
};

#endif // RELIABLE_H

