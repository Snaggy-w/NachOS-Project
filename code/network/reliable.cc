// network/reliable.cc
#include "reliable.h"
#include "system.h"     // stats, currentThread, interrupt, postOffice global etc.
#include "thread.h"
#include "utility.h"
#include <string.h>     // memcpy, memset
#include <stdio.h>      // sprintf




/* PeerKey:
 *
 * Peer mapping to machine + box. We have peer connection list (like a list of sockets established)
 * We identify the entry by assigning the higher 48 bits of the 64 bits of the long long integer to the machine ID
 * while we use the lower 16 to associate the box. So in theory we can have 2^48 machines each with 2^16 boxes each
 *
 */
static long long PeerKey(int mach, int mbox) {
    return (((long long)mach) << 16) | (mbox & 0xFFFF);
}

static int MinInt(int a, int b) { return (a < b) ? a : b; }

ReliablePost::ReliablePost(PostOffice *po, int localMailbox, const char *debugName)
: postOffice(po), // The unreliable post office to use. It's dynamic but we will only use the already available postOffice global object
  localMbox(localMailbox), // The mail box that this interface must listen on
  name(debugName),
  rxThread(0), // receiver thread initialized after the constructor list
  timerThread(0), // timeout/retransmission worker thread also initialized after the constructor list below
  lock(new Lock("reliable lock")), // lock to protect the internal state of the reliable interface like the peer list
  //nextSendSeq(0),
  pending(0),  // pointer to structure that holds the packet plus state info. We have only one in-flight message at a time so this aids us with that
  fixedQ(new SynchList()), // queue of delivered fixed-size reliable messages ready for the application, used to filter out duplicates
  varQ(new SynchList()), // queue of complete reassembled variable-size messages, when it sees REL_LAST, it packages the full message and appends to varQ.
  peers(new SynchList()), // connection table of peers, holds per-peer protocol states in each PeerState entry
  shuttingDown(false) // signal worker threads to stop looping (receiver and timer threads)
{
    // Initialization of objects
    rxThread = new Thread("reliable receiver");
    rxThread->Fork(ReceiverTrampoline, (int)this);

    timerThread = new Thread("reliable timer");
    timerThread->Fork(TimerTrampoline, (int)this);
}

// destructor that releases waiters/loopers
ReliablePost::~ReliablePost() {
    lock->Acquire();
    shuttingDown = true;
    if (pending && !pending->done) {
        pending->acked = false;
        pending->done = true;
        pending->waiter->V();
    }
    lock->Release();
}


// We cannot pass member methods directly to Fork, so we use wrappers that take the "this" object and calls the member state from this object.
void ReliablePost::ReceiverTrampoline(int arg) {
    ((ReliablePost*)arg)->ReceiverLoop();
}
void ReliablePost::TimerTrampoline(int arg) {
    ((ReliablePost*)arg)->TimerLoop();
}


/* SendRaw:
 * 	Sends a raw packet using the post office directly
 */ 
void ReliablePost::SendRaw(int dstMachine, int dstMailbox, const char *buf, int nBytes) {
    PacketHeader pkt;
    MailHeader mail;

    pkt.to = dstMachine;

    mail.to = dstMailbox;
    mail.from = localMbox;
    mail.length = nBytes;

    postOffice->Send(pkt, mail, buf);
}

/* SendAck:
 * 	Sends an ACK with a sequence identifier
 */
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

/* GetOrCreatePeer:
 * 	Gets a peer state based on the machine/box identifier key or creates a state if new
 */
ReliablePost::PeerState *ReliablePost::GetOrCreatePeer(int srcMachine, int srcMailbox) {
    long long key = PeerKey(srcMachine, srcMailbox);
    PeerState *p = (PeerState*)peers->SortedGet(key);
    if (p) return p;

    p = new PeerState(srcMachine, srcMailbox);
    peers->SortedInsert((void*)p, key);
    return p;
}

/* EnqueueFixed:
 * 	Add message to queue. It takes a validated, duplicate-free payload and turns it into a message that the application can safely receive later.
 * 	This function acts like a stich between the async lossy system and the synchronous behavior by blocking (for example on Remove if it is empty)
 * 	Essentially it is used to keep track of all received packets (like a buffer of multiple packets) that will be consumed later individually.
 */
void ReliablePost::EnqueueFixed(int srcMachine, int srcMailbox, const char *data, int len) {
    AppMsg *m = new AppMsg; // application level message container
    m->srcMachine = srcMachine;
    m->srcMailbox = srcMailbox;
    m->len = len;
    m->data = new char[len]; 
    memcpy(m->data, data, len); // copy data and package App message
    fixedQ->Append((void*)m); // append to the queue
}

/* HandleVarFragment:
 *	Helper that handles the assembly and enqueueing of variable sized messages
 */

void ReliablePost::HandleVarFragment(PeerState *peer,
                                     int srcMachine, int srcMailbox,
                                     const char *frag, int fragLen, unsigned char flags)
{

    // ensure that the reassembly buffer is large enough and resize accordingly
    int needed = peer->partialLen + fragLen;
    if (needed > peer->partialCap) {
        int newCap = (peer->partialCap == 0) ? 128 : peer->partialCap;
        while (newCap < needed) newCap *= 2; // resize by doubling to aboid reallocation on every resize
        char *nb = new char[newCap];

	// copy already received bytes into the buffer
        if (peer->partial && peer->partialLen > 0) {
            memcpy(nb, peer->partial, peer->partialLen);
        }
        delete [] peer->partial; // delete old buffer
        peer->partial = nb; // assign the peer connection partial data section to the new buffer
        peer->partialCap = newCap; // increase the advertized bufefr size
    }
    
    // append the fragment bytes to the end
    memcpy(peer->partial + peer->partialLen, frag, fragLen);
    peer->partialLen += fragLen;

    // if this fragment copied is the last one, finalize the message and add it to the variable message queue
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


/* HandleIncoming:
 * 	Parser for packets. Decides the workflow of sender based on the contents of packet parsed.
 */
void ReliablePost::HandleIncoming(int srcMachine, int srcMailbox,
                                  const char *buf, int nBytes)
{
    if (nBytes < (int)sizeof(ReliableHeader)) return;

    ReliableHeader h;
    memcpy(&h, buf, sizeof(ReliableHeader)); // parse the reliable header
    if (h.magic != REL_MAGIC) return;

    // if ACK message -> match current pending send then unblock the sender
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

    // if DATA message -> send an ACK immediately
    if (h.type == REL_DATA) {
        // ACK every DATA (even duplicates). receiver got DATA -> receiver sent ACK -> ACK lost -> sender will send again forever so we ACK duplicates.
        SendAck(srcMachine, srcMailbox, h.seq);

        int payloadAvail = nBytes - (int)sizeof(ReliableHeader);
        if ((int)h.len > payloadAvail) return; // prevengt reading beyond the received bytes to avoid overflows

        // duplicate filtering
        lock->Acquire();
        PeerState *peer = GetOrCreatePeer(srcMachine, srcMailbox); // get the connection state
        if (h.seq != peer->expectedSeq) { // compare the expected sequence with the actual sequence received
            lock->Release();
            return; // duplicate or unexpected
        }
        // new message
       /*
	* Let's say we start with seq 0:
	*	A sends: DATA(seq=0, payload="Hello")
	*		it waits for ACK(seq=0)
	*	B receives DATA(seq=0)
	*		h.seq = 0 == expectedSeq = 0 -> match
	*	B send ACK(seq=0)
	*		flip expected sequence -> expectedSeq = 1      
	*	if ACK(seq=0) from B was lost:
	*		A resends DATA(seq=0, payload="Hello")
	*		at B expectedSeq = 1 != h.seq = 0 -> so we got a duplicate
	*	since messages are ordered, we can keep track of messages that are duplicates by essentially tracking if the sequence
	*	expected/sent are even or odd.
	*/
        peer->expectedSeq ^= 1; 
        lock->Release();

        const char *payload = buf + sizeof(ReliableHeader);
        int payloadLen = (int)h.len;

        if (h.flags == 0) { // Fixed-size message -> no fragmentation needed and no assembly
            EnqueueFixed(srcMachine, srcMailbox, payload, payloadLen);
        } else {
            lock->Acquire();
            PeerState *peer2 = GetOrCreatePeer(srcMachine, srcMailbox);
            HandleVarFragment(peer2, srcMachine, srcMailbox, payload, payloadLen, h.flags);
            lock->Release();
        }
    }
}


/* ReceiverLoop:
 * 	Receiver loop executed by the receiver worker thread. 
 */
void ReliablePost::ReceiverLoop() {
    // allocate space to store incoming data
    PacketHeader pkt;
    MailHeader mail;
    char in[MaxMailSize];

    while (true) {
        lock->Acquire();
        bool stop = shuttingDown; // check for shutdown
        lock->Release();
        if (stop) return;

	// block the receiver so it is waiting for mail
        postOffice->Receive(localMbox, &pkt, &mail, in); 

        int srcMachine = pkt.from;
        int srcMailbox = mail.from;
        int nBytes = mail.length;
        if (nBytes > (int)MaxMailSize) nBytes = (int)MaxMailSize;

        HandleIncoming(srcMachine, srcMailbox, in, nBytes);
    }
}

/* TimerLoop:
 * 	Looper for timer worker thread. Responsible for handling time-outs and retransmissions.
 */
void ReliablePost::TimerLoop() {
    while (true) {
        lock->Acquire();
        bool stop = shuttingDown;
        PendingSend *p = pending; // handle to the in-flight packet
        lock->Release();
        if (stop) return;

        if (p) { 
            lock->Acquire();
            if (pending && !pending->done) { // if the packet exists and not flaged as done
                int elapsed = stats->totalTicks - pending->sendTick; // calculate the elapsed time since transmission 
                if (elapsed >= TEMPO) { 
                    if (pending->retransmissions >= MAXREEMISSIONS) { 
                        pending->acked = false; // mark as failed if TEMPO elapsed and max reemissions have been tried
                        pending->done = true;
                        pending->waiter->V(); // unblock the sender in case of failure
                    } else { // try to retransmit if the elapsed time is still in TEMPO window and still in msx reemission window
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


/* SendFixed:
 * 	Send fixed sized packet for data that can fit in a single packet
 */
bool ReliablePost::SendFixed(int dstMachine, int dstMailbox, const char *data, int nBytes) {
    // Setup the payload and wait until the sending line is free
    int maxPayload = MaxMailSize - (int)sizeof(ReliableHeader);
    if (nBytes < 0) nBytes = 0;
    if (nBytes > maxPayload) nBytes = maxPayload;

    lock->Acquire();
    while (pending != 0) {
        lock->Release();
        currentThread->Yield();
        lock->Acquire();
    }
	
    // setup the high level packet wrapper
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
    h.flags = 0; // set the flag as a single message connection
    h.seq   = p->seq;
    h.len   = (unsigned short)nBytes;
    
    // copy the header + data to the packet of the sending instance
    memcpy(p->packet, &h, sizeof(ReliableHeader));
    if (nBytes > 0) memcpy(p->packet + sizeof(ReliableHeader), data, nBytes);
    p->packetLen = (int)sizeof(ReliableHeader) + nBytes;

    pending = p;
    lock->Release();

    SendRaw(dstMachine, dstMailbox, p->packet, p->packetLen);

    p->waiter->P(); // wait for ACK or failure after sending

    lock->Acquire();
    bool ok = p->acked;
    pending = 0;
    lock->Release();

    delete p->waiter;
    delete p;
    return ok;
}


/* ReceiveFixed:
 * 	Consumes one packet from the fixed length queue, if the queue is empty it will block due to how the SynchList object is implemented
 */
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


/* SendVar:
 * 	Sends a variable sized message sequence.
 *	Note: duplicate messages should not affect the correctness of sent/received messages since duplicate filtering done in other functions
 *	like HandleIncoming refuse to append duplicate messages to the queues
 */

bool ReliablePost::SendVar(int dstMachine, int dstMailbox, const char *data, int nBytes) {
    // sanitize and compute fragment size
    if (nBytes < 0) nBytes = 0;

    int maxPayload = MaxMailSize - (int)sizeof(ReliableHeader);
    if (maxPayload <= 0) return false;

    // send fragments sequentially
    int offset = 0; // the offset is where we are in the large assembled message that we want to send
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
        h.flags = last ? REL_LAST : REL_MORE; // either it is the last packet or something before that i.e MORE flag is set
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
    // special case in case the sneder wants to send a packet with no data    
    // 		-> send the packet as a LAST packet to terminate the transmission immediately with no data copying
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

/* ReceiveVar:
 *	
 */
void ReliablePost::ReceiveVar(char **outData, int *outLen,
                             int *outSrcMachine, int *outSrcMailbox)
{
    AppMsg *m = (AppMsg*)varQ->Remove(); // block until a complete var message is available
					 // the message should be assembled by HandleVarFragment

    if (outSrcMachine) *outSrcMachine = m->srcMachine;
    if (outSrcMailbox) *outSrcMailbox = m->srcMailbox;

    *outLen = m->len;
    *outData = m->data; // transfer ownership from the temp AppMsg object to the requested buffer to avoid copying huge data and waste memory
    delete m;
}

