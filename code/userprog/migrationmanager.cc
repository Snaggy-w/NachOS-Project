#ifdef NETWORK

#include "migrationmanager.h"

#include "reliable.h"     // ReliablePost
#include "system.h"       // postOffice, stats, etc.
#include "thread.h"       // Thread
#include "synch.h"        // Lock
#include "utility.h"      // DEBUG, ASSERT

#include <string.h>       // memset
#include "userprocess.h"  // StartMigratedUserProcess
#include "addrspace.h"
#include "machine.h"      // for the memory copying
#include "migrateproto.h" // MigBegin struct

// a tiny "wakeup" payload that won't be mistaken for real protocol. It is used to wakeup the receiver thread.
static const char* kWakeMsg = "__MIG_WAKE__";

// constructor list
MigrationManager::MigrationManager(ReliablePost* rp_)
: rp(rp_),
  rxThread(0),
  lock(new Lock("migration manager lock")),
  shuttingDown(false)
{
    ASSERT(rp != 0);

    rxThread = new Thread("migration receiver");
    rxThread->Fork(RxTrampoline, (int)this);
}

MigrationManager::~MigrationManager() {
    lock->Acquire();
    shuttingDown = true;
    lock->Release();

    // unblock ReceiveFixed() in RxLoop by sending a message to ourselves.
    SendWakeupToSelf();

    // our rx loop will exit after it wakes and sees shuttingDown = true.
    delete lock;
    lock = 0;
}

int MigrationManager::RequestMigrate(int targetMachine) {
    // stub for now and unused
    (void)targetMachine;
    return 0;
}


/* RxTrampoline:
 *	a trampoline to execute RxLoop() internal function in  a thread
 *
 */
void MigrationManager::RxTrampoline(int arg) {
    ((MigrationManager*)arg)->RxLoop();
}

/* SendWakeupToSelf:
 *	a hacky function that sends a wakeup message to the receiver thread to unblock it in case it was waiting
 */
void MigrationManager::SendWakeupToSelf() {
    // We send to our own machine, mailbox 1.
    // ReliablePost::SendFixed(dstMachine, dstMailbox, data, len)
    // We don't care about success here.
    int me = postOffice->GetAddress();
    rp->SendFixed(me, /*dstMailbox=*/1, kWakeMsg, (int)strlen(kWakeMsg) + 1);
}



void MigrationManager::RxLoop() {
    // setup the buffers and constants
    char buf[MaxMailSize]; // holds one fixed size message payload
    int srcM = -1, srcB = -1; // the src details

    const int wakeLen = (int)strlen(kWakeMsg) + 1; // the length of the wakeup string header

    while (true) { // the thread infinite loop
        int n = rp->ReceiveFixed(buf, (int)sizeof(buf), &srcM, &srcB); // block and receive a fixed size packet

        lock->Acquire();
        bool stop = shuttingDown; // see if we need to shutdown and stop the thread runner
        lock->Release();
        if (stop) return;

        if (n <= 0) continue; 

        // validate received message. we use memcmp since we are comparing binary data that could contain a null byte
        if (n == wakeLen && memcmp(buf, kWakeMsg, wakeLen) == 0)
            continue;

        if (n != (int)sizeof(MigBegin)) { // debug when we receive a fixed message with size != size of MigBegin 
            DEBUG('n', "[MigrationManager] bad header size %d (expected %d)\n",
                  n, (int)sizeof(MigBegin));
            continue;
        }

        MigBegin hdr; // copy the header from the buffer holding received data
        memcpy(&hdr, buf, sizeof(hdr));

        if (hdr.magic != MIG_MAGIC) { // debug in case of malformed header magic
            DEBUG('n', "[MigrationManager] bad magic 0x%x\n", hdr.magic);
            continue;
        }

        // receive var payload after setting up buffer pointer, payload length and teh source machine properties
        char* payload = 0; // only a pointer as the buffer is created and owned by ReceiveVar so we just transfer ownership
        int payloadLen = 0;
        int psrcM = -1, psrcB = -1;
        rp->ReceiveVar(&payload, &payloadLen, &psrcM, &psrcB);

	// debug to ensure the the machine sendng the serialized payload is the same as the one that sent the initial message
        if (psrcM != srcM) {
            DEBUG('n', "[MigrationManager] payload src mismatch (%d != %d)\n", psrcM, srcM);
            if (payload) delete [] payload;
            continue;
        }
	
	// validate payload length matches the header promised
        int need = hdr.regsBytes + hdr.memBytes + hdr.filesBytes;
        if (!payload || payloadLen < need) {
            DEBUG('n', "[MigrationManager] bad payloadLen=%d need=%d\n", payloadLen, need);
            if (payload) delete [] payload;
            continue;
        }
	
	// rebuild a new address space on the destination machine
        AddrSpace* space = AddrSpace::CreateEmpty((unsigned)hdr.numPages, (unsigned)hdr.brk);
        if (!space) {
            DEBUG('n', "[MigrationManager] CreateEmpty failed\n");
            delete [] payload;
            continue;
        }

        if (!space->CopyInMemory(payload + hdr.regsBytes, hdr.memBytes)) {
            DEBUG('n', "[MigrationManager] CopyInMemory failed\n");
            delete space;
            delete [] payload;
            continue;
        }
	// create the start bundle for the new process thread
        MigratedStart* ms = new MigratedStart;
        ms->space = space;
        memcpy(ms->regs, payload, hdr.regsBytes); // copy the user register states from the payload to the bundle
        delete [] payload;

	// create the thread that will start the migrated process
        Thread* t = new Thread("migrated process");
        t->Fork(StartMigratedUserProcess, (int)ms);

        DEBUG('n', "[MigrationManager] migrated process from %d started\n", srcM);
    }
}



#endif // NETWORK

