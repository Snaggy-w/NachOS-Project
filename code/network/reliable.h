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

    bool SendFixed(int dstMachine, int dstMailbox, const char *data, int nBytes);

    int ReceiveFixed(char *outBuffer, int outCapacity,
                     int *outSrcMachine = 0, int *outSrcMailbox = 0);

    bool SendVar(int dstMachine, int dstMailbox, const char *data, int nBytes);

    void ReceiveVar(char **outData, int *outLen,
                    int *outSrcMachine = 0, int *outSrcMailbox = 0);

  private:
    struct AppMsg { // a struct to wrap around just the useful data that could be used by the application including the data
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
    struct PeerState { // a struct that contains peer connection info
    int srcMachine;
    int srcMailbox;

    unsigned short expectedSeq;        // next expected incoming seq (from peer)
    unsigned short nextSendSeqToPeer;  // next outgoing seq (to peer)

    char *partial; // the data to pull from the connection and assemble (essentially an assembly station or buffer of var length data)
    int partialLen;
    int partialCap;

    PeerState(int m, int b)
      : srcMachine(m), srcMailbox(b),
        expectedSeq(0), nextSendSeqToPeer(0),
        partial(0), partialLen(0), partialCap(0) {}
    };

    struct PendingSend { // a struct to enqueue a transmission on the line
        int dstMachine;
        int dstMailbox;
        unsigned short seq;

        int sendTick; // keep track of when it was sent
        int retransmissions; // keep track of how many times it was sent
        bool acked; // see if it was ever received
        bool done; // mark if the transmission is done

        int packetLen; 
        char packet[MaxMailSize];  // must fit in one mail payload

        Semaphore *waiter; // synchronize the sending and the receiving for ACK messages for example
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
    PostOffice *postOffice; // reference to any arbitrary post office, but we will use the global one anyways
    int localMbox; // the local mail box that will be used
    const char *name;

    Thread *rxThread; // the receiver thread running the receiver loop
    Thread *timerThread; // the timer thread running the timer loop

    Lock *lock; // a protective lock for internal data structures
    //unsigned short nextSendSeq;

    PendingSend *pending;

    SynchList *fixedQ; // queue holding the received fixed sized (single) packets
    SynchList *varQ;   // queue holding the received variable sized messages that are assembled beforehand
    SynchList *peers;  // list containing all the connections made to the machine

    bool shuttingDown; // flag to signal machine connection setup termination
};

#endif // RELIABLE_H

