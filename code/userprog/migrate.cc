#ifdef NETWORK

#include "system.h"
#include "thread.h"
#include "addrspace.h"
#include "reliable.h"
#include "userprocess.h"     // do_Exit
#include "migrateproto.h"
#include <string.h>          // memcpy


int do_Migrate(int dstMachine) {
    int me = postOffice->GetAddress(); // get the address of the machine
    if (dstMachine == me) return -1; // to prevent migrating to self by mistake
    if (dstMachine < 0) return -1; 
    if (!currentThread || !currentThread->space) return -1; // if the pointers aren't setup correctly -> fail
    if (!migratePost) return -1; // if the migrate post service is not initialized

    // save CPU regs from machine -> thread->userRegisters
    currentThread->SaveUserState();

    AddrSpace* as = currentThread->space; // get a reference to the address space of the process to migrate
    int np = (int)as->GetNumPages(); 
    int brk = (int)as->GetBrk();

    const int regsBytes = NumTotalRegs * (int)sizeof(int); // the length of the user registers array
    const int memBytes  = np * PageSize; // the size in pages of the process
    const int filesBytes = 0; // for now it is 0, it could be used for file transfers

    MigBegin hdr; // create a migration bundle
    hdr.magic = MIG_MAGIC;
    hdr.numPages = np;
    hdr.brk = brk;
    hdr.regsBytes = regsBytes;
    hdr.memBytes = memBytes;
    hdr.filesBytes = filesBytes;
    // the size of the paylaod itself
    const int payloadBytes = regsBytes + memBytes + filesBytes;
    char* payload = new char[payloadBytes];

    int regs[NumTotalRegs];
    // populate the user register array
    currentThread->GetUserRegisters(regs);

    // advance PC so the migrated process resumes after the syscall so we don't endup executing the same syscall over and over. That was a nasty bug...
    regs[PrevPCReg] = regs[PCReg];
    regs[PCReg]     = regs[NextPCReg];
    regs[NextPCReg] = regs[NextPCReg] + 4;
    memcpy(payload, regs, regsBytes);

    // copy out the memory pages to the payload
    if (!as->CopyOutMemory(payload + regsBytes, memBytes)) {
        delete [] payload;
        return -1;
    }

    // send control header (fixed)
    if (!migratePost->SendFixed(dstMachine, /*dstMailbox=*/1, (char*)&hdr, (int)sizeof(hdr))) {
        delete [] payload;
        return -1;
    }

    // send payload (var)
    if (!migratePost->SendVar(dstMachine, /*dstMailbox=*/1, payload, payloadBytes)) {
        delete [] payload;
        return -1;
    }

    delete [] payload;

    // kill local instance since the process continues remotely.
    do_Exit(0);
    return 0; // unreachable
}

#endif

