// addrspace.cc
//      Routines to manage address spaces (executing user programs).
//
//      In order to run a user program, you must:
//
//      1. link with the -N -T 0 option
//      2. run coff2noff to convert the object file to Nachos format
//              (Nachos object code format is essentially just a simpler
//              version of the UNIX executable object code format)
//      3. load the NOFF file into the Nachos file system
//              (if you haven't implemented the file system yet, you
//              don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "addrspace.h"
#include "copyright.h"
#include "noff.h"
#include "system.h"
#include "frameprovider.h"
#include <strings.h> /* for bzero */

//----------------------------------------------------------------------
// SwapHeader
//      Do little endian to big endian conversion on the bytes in the
//      object file header, in case the file was generated on a little
//      endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void SwapHeader(NoffHeader *noffH) {
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
//      Create an address space to run a user program.
//      Load the program from a file "executable", and set everything
//      up so that we can start executing user instructions.
//
//      Assumes that the object code file is in NOFF format.
//
//      First, set up the translation from program memory to physical
//      memory.  For now, this is really simple (1:1), since we are
//      only uniprogramming, and we have a single unsegmented page table
//
//      "executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable) {
    NoffHeader noffH;
    unsigned int i, size;
    this->creation_status = 0;
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    // how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size +
           UserStackSize + ProcessHeapSize; // we need to increase the size
    // to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= NumPhysPages); // check we're not trying
    // to run anything too big --
    // at least until we have
    // virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", numPages,
          size);
    // first, set up the translation
    if((int)numPages > frameProvider->NumAvailFrame()) {
        DEBUG('a', "Not enough space for the appending process.");
        numPages = 0; 
	this->creation_status = -1;
        return; 
    }
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        int physFrame = frameProvider->GetEmptyFrame(false);

        pageTable[i].virtualPage = i; // for now, virtual page # = phys page #
        pageTable[i].physicalPage = physFrame;
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE; // if the code segment was entirely on
                                       // a separate page, we could set its
                                       // pages to be read-only
    }

    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
    // then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
              noffH.code.virtualAddr, noffH.code.size);
        // executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),
        //                    noffH.code.size, noffH.code.inFileAddr);
        this->ReadAtVirtual(executable, noffH.code.virtualAddr, noffH.code.size,
            noffH.code.inFileAddr, pageTable, 0);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
              noffH.initData.virtualAddr, noffH.initData.size);


        this->ReadAtVirtual(executable, noffH.initData.virtualAddr, noffH.initData.size,
            noffH.initData.inFileAddr, pageTable, 0);
        // executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),
        //                    noffH.initData.size, noffH.initData.inFileAddr);
    }

    // initializing the bitmap for user stack segments (stored within this address space)
    this->userStackSegmentMap = new BitMap((int)(UserStackSize / UserStackSegment));
    this->userStackSegmentMap->Mark(0); // marking the segment for the "default" main thread
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
//      Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace() {
    // LB: Missing [] for delete
    // delete pageTable;
    for(int i = 0; i < (int)numPages; i++){
        frameProvider->ReleaseFrame(pageTable[i].physicalPage);
    }
    delete[] pageTable;
    delete userStackSegmentMap; 
    // End of modification
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
//      Set the initial values for the user-level register set.
//
//      We write these directly into the "machine" registers, so
//      that we can immediately jump to user code.  Note that these
//      will be saved/restored into the currentThread->userRegisters
//      when this thread is context switched out.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters() {
    int i;

    for (i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we don't
    // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
//      On a context switch, save any machine state, specific
//      to this address space, that needs saving.
//
//      For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() {}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
//      On a context switch, restore the machine state so that
//      this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() {
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

unsigned int AddrSpace::GetNumPages() {
	return numPages;
}

// !!!for now, we assume that only one process can be run on our machine
/// TODO: multiprocess system will have a different implementation
// requiring atomicity of the whole address space initialization in the context
// of the whole system
void AddrSpace:: ReadAtVirtual(
      OpenFile *executable, int virtualaddr, int numBytes, int position,
      TranslationEntry *pageTable, unsigned numPages) {

    char* tmpBuffer = new char[numBytes];
    executable->ReadAt(tmpBuffer, numBytes, position);
      
    int vpn = (unsigned)virtualaddr / PageSize;
    int offset = (unsigned)virtualaddr % PageSize;

    int i = 0;
    //DEBUG('t',"Creating an address space...\n");
    while(i <= numBytes){
	//DEBUG('t',"Infinite loop?\n");
        TranslationEntry page = pageTable[vpn];
        while(offset < PageSize && i <= (int)numBytes) {
            machine->WriteMem((page.virtualPage * PageSize) + offset, 1, (int)tmpBuffer[i]);
            i++;
            offset++;
        }
        vpn++;
        offset = 0;
    }
    /// TODO: we could manipulate 4-byte words instead of byte by byte writing
    //for (int byteInd = 0; byteInd < numBytes; byteInd++) {
      //  machine->WriteMem(virtualaddr + byteInd, 1, (int)tmpBuffer[byteInd]);
    //}
}

int AddrSpace::GetBrk() {
    return this->brk;
}
