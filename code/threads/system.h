// system.h
//      All global variables used in Nachos are defined here.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef SYSTEM_H
#define SYSTEM_H

#include "copyright.h"
#include "interrupt.h"
#include "scheduler.h"
#include "stats.h"
#include "thread.h"
#include "timer.h"
#include "utility.h"
#include "synchconsole.h"
#include "synchlist.h"
#include "synch.h"

// Initialization and cleanup routines
extern void Initialize(int argc, char **argv); // Initialization,
                                               // called before anything else
extern void Cleanup();                         // Cleanup, called when
                                               // Nachos is done.

extern Thread *currentThread;       // the thread holding the CPU
extern Thread *threadToBeDestroyed; // the thread that just finished
extern Scheduler *scheduler;        // the ready list
extern Interrupt *interrupt;        // interrupt status
extern Statistics *stats;           // performance metrics
extern Timer *timer;                // the hardware alarm clock

#define MAX_STRING_SIZE 201 // Can't put more since large sizes can crash the kernel stack
			    // the last byte is to account for the Null byte terminator at the end
#define MAX_OPEN_FILES 10

#ifdef USER_PROGRAM
#include "machine.h"
class FrameProvider;
extern Machine *machine; // user program memory and registers
extern FrameProvider *frameProvider;

/* 
 * A global extern of class instances like the console should be done here and initialized in the header file "system.cc"
 */
extern SynchConsole *synchconsole;
extern char * STRING_BUFFER;
extern SynchList *PIDTable;
extern int PIDCounter;
extern Lock *PIDCounterLock;
extern Lock *ProcessLoopLock;

extern Condition *AnyProcessExited;
extern Lock *AnyProcessExitedLock;
extern Semaphore *SemExitedProcesses;
#endif


#ifdef FILESYS_NEEDED // FILESYS or FILESYS_STUB
#include "filesys.h"
extern FileSystem *fileSystem;
extern Directory *currentDirectory;
extern SynchList *openFileList;
#endif

#ifdef FILESYS
#include "synchdisk.h"
extern SynchDisk *synchDisk;
#endif

#ifdef NETWORK
#include "post.h"
extern PostOffice *postOffice;
#endif

#endif // SYSTEM_H
