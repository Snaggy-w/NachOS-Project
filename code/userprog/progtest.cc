// progtest.cc
//      Test routines for demonstrating that Nachos can load
//      a user program and execute it.
//
//      Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "addrspace.h"
#include "console.h"
#include "copyright.h"
#include "synch.h"
#include "system.h"
#include "synchconsole.h"

//----------------------------------------------------------------------
// StartProcess
//      Run a user program.  Open the executable, load it into
//      memory, and jump to it.
//----------------------------------------------------------------------

void StartProcess(char *filename) {
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    if (executable == NULL) {
        printf("Unable to open file %s\n", filename);
        return;
    }
    space = new AddrSpace(executable);
    currentThread->space = space;

    delete executable; // close file

    space->InitRegisters(); // set the initial register values
    space->RestoreState();  // load page table register

    machine->Run(); // jump to the user progam
    ASSERT(FALSE);  // machine->Run never returns;
    // the address space exits
    // by doing the syscall "exit"
}

// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
//      Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

static void ReadAvail(int arg) { readAvail->V(); }

static void WriteDone(int arg) { writeDone->V(); }

//----------------------------------------------------------------------
// ConsoleTest
//      Test the console by echoing characters typed at the input onto
//      the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------
void ConsoleTest(char *in, char *out) {
    char ch;

    console = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);

    for (;;) {
        readAvail->P();
        ch = console->GetChar();
        if (ch == 'q' || ch == EOF || ch == '\n') {console->PutChar('\n'); return;}; 
	    console->PutChar('<');
        writeDone->P(); 
        console->PutChar(ch); 
        writeDone->P(); 
        console->PutChar('>');
	    writeDone->P(); 
    }
}

void SynchConsoleTest(char *in, char *out){
    char ch;
    //SynchConsole *synchconsole = new SynchConsole(in, out);
    synchconsole = new SynchConsole(in, out); // synchconsole object already globaly defined now
    ch = synchconsole->SynchGetChar();
    while (ch != EOF && ch != 'q') {
        synchconsole->SynchPutChar(ch);
        ch = synchconsole->SynchGetChar();
    }       
    synchconsole->SynchPutChar('\n');
    fprintf(stdout, "Solaris: EOF detected in SynchConsole!\n");
}

