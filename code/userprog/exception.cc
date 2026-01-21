// exception.cc
//      Entry point into the Nachos kernel from user programs.
//      There are two kinds of things that can cause control to
//      transfer back to here from user code:
//
//      syscall -- The user code explicitly requests to call a procedure
//      in the Nachos kernel.  Right now, the only function we support is
//      "Halt".
//
//      exceptions -- The user code does something that the CPU can't handle.
//      For instance, accessing memory that doesn't exist, arithmetic errors,
//      etc.
//
//      Interrupts (which can also cause control to transfer from user
//      code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "syscall.h"
#include "system.h"
#include "userthread.h"
#include "helpers.h"
#include "userprocess.h"
#include "addrspace.h"


#ifndef ConsoleInput
#define ConsoleInput 0
#endif

#ifndef ConsoleOutput
#define ConsoleOutput 1
#endif

#ifdef NETWORK
#include "rnetmanager.h"
int do_Migrate(int dstMachine);
#endif

#ifdef FILESYS_STUB
#include "opentable.h"
#endif

// A helper function that is used to copy over large number of arguments
static bool CopyArgsFromUser(int userPtr, int* out, int nInts) {
    for (int i = 0; i < nInts; i++) {
        int v;
        if (!machine->ReadMem(userPtr + i*(int)sizeof(int), (int)sizeof(int), &v)) return false;
        out[i] = v;
    }
    return true;
}



//----------------------------------------------------------------------
// UpdatePC : Increments the Program Counter register in order to resume
// the user program immediately after the "syscall" instruction.
//----------------------------------------------------------------------
static void UpdatePC() {
    int pc = machine->ReadRegister(PCReg);
    machine->WriteRegister(PrevPCReg, pc);
    pc = machine->ReadRegister(NextPCReg);
    machine->WriteRegister(PCReg, pc);
    pc += 4;
    machine->WriteRegister(NextPCReg, pc);
}

//----------------------------------------------------------------------
// ExceptionHandler
//      Entry point into the Nachos kernel.  Called when a user program
//      is executing, and either does a syscall, or generates an addressing
//      or arithmetic exception.
//
//      For system calls, the following is the calling convention:
//
//      system call code -- r2
//              arg1 -- r4
//              arg2 -- r5
//              arg3 -- r6
//              arg4 -- r7
//
//      The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//      "which" is the kind of exception.  The list of possible exceptions
//      are in machine.h.
//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType which) {
    int type = machine->ReadRegister(2);
#ifndef CHANGED

    if ((which == SyscallException) && (type == SC_Halt)) {
        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
    } else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
#else // CHANGED

/*
 *
 *	That is equivalent to an IDT (interrupt descriptor table in a sense)
 *
 */

    if(which == SyscallException){
    	switch(type){
		
		case SC_Halt:
			DEBUG('a', "Shutdown, initiated by user program.\n");
			//interrupt->Halt();
			do_Halt(); // Account for multi-threaded implementation
		break;

		case SC_Exit: {
			int status = machine->ReadRegister(4);
    			do_Exit(status);
			//Exit(0); // maybe we should update the syscall to also include the exit code from a register?
			break;
		}
		//case SC_Exec:
			// We need to implement Exec and figure out argument stuff
		//break;

		//case SC_Join:
			// We need to implement Join and and figure out arguments stuff
		//break;

		//case SC_Create:
			// We need to implement Create and and figure out arguments stuff
		//break;
		
		//case SC_Open:
			// We need to implement Open and and figure out arguments stuff
		//break;

		//case SC_Read:
			// We need to implement Read and and figure out arguments stuff
		//break;
		
		//case SC_Write:
			// We need to implement Write and and figure out arguments stuff
		//break;
		
		//case SC_Close:
			// We need to implement Close and and figure out arguments stuff
		//break;

		#ifdef FILESYS_STUB
		case SC_Create: {
		    char name[MAX_FILENAME_LEN];
		    int userNameAddr = (int)machine->ReadRegister(4);

		    if (!Helpers::CopyStringFromUser(userNameAddr, name, MAX_FILENAME_LEN)) {
			machine->WriteRegister(2, -1);
			break;
		    }

		    // In NachOS, Create takes initialSize; in stub it ignores it.
		    bool ok = fileSystem->Create(name, 0);
		    machine->WriteRegister(2, ok ? 0 : -1);
		    break;
		}

		case SC_Open: {
		    char name[MAX_FILENAME_LEN];
		    int userNameAddr = (int)machine->ReadRegister(4);

		    if (!Helpers::CopyStringFromUser(userNameAddr, name, MAX_FILENAME_LEN)) {
			machine->WriteRegister(2, -1);
			break;
		    }

		    if (!currentThread->space || !currentThread->space->openTable) {
			machine->WriteRegister(2, -1);
			break;
		    }

		    OpenFile* f = fileSystem->Open(name);
		    if (!f) {
			DEBUG('f',"failed on filesystem open");
			machine->WriteRegister(2, -1);
			break;
		    }

		    int fd = currentThread->space->openTable->AddWithName(f,name);
		    if (fd < 0) {
			DEBUG('f',"failed on fd assignment thing");
			delete f;
			machine->WriteRegister(2, -1);
			break;
		    }

		    machine->WriteRegister(2, fd);
		    break;
		}

		case SC_Close: {
		    int fd = (int)machine->ReadRegister(4);

		    if (!currentThread->space || !currentThread->space->openTable) {
			machine->WriteRegister(2, -1);
			break;
		    }

		    int rc = currentThread->space->openTable->Remove(fd);
		    machine->WriteRegister(2, rc);
		    break;
		}

		case SC_Read: {
		    int userBuf = (int)machine->ReadRegister(4);
		    int size    = (int)machine->ReadRegister(5);
		    int fd      = (int)machine->ReadRegister(6);

		    if (size < 0) { machine->WriteRegister(2, -1); break; }
		    if (size == 0) { machine->WriteRegister(2, 0); break; }

		    // ConsoleInput
		    if (fd == ConsoleInput) {
			// Read one char at a time to user buffer
			int count = 0;
			for (; count < size; count++) {
			    char ch = synchconsole->SynchGetChar();
			    if (!machine->WriteMem(userBuf + count, 1, (int)ch)) break;
			}
			machine->WriteRegister(2, count);
			break;
		    }

		    if (!currentThread->space || !currentThread->space->openTable) {
			machine->WriteRegister(2, -1);
			break;
		    }

		    OpenFile* f = currentThread->space->openTable->Get(fd);
		    if (!f) { machine->WriteRegister(2, -1); break; }

		    // Read into kernel buffer then copy to user
		    char* kbuf = new char[size];
		    int n = f->Read(kbuf, size);

		    int copied = 0;
		    for (int i = 0; i < n; i++) {
			if (!machine->WriteMem(userBuf + i, 1, (int)kbuf[i])) break;
			copied++;
		    }

		    delete [] kbuf;
		    machine->WriteRegister(2, copied);
		    break;
		}

		case SC_Write: {
		    int userBuf = (int)machine->ReadRegister(4);
		    int size    = (int)machine->ReadRegister(5);
		    int fd      = (int)machine->ReadRegister(6);

		    if (size < 0) { machine->WriteRegister(2, -1); break; }
		    if (size == 0) { machine->WriteRegister(2, 0); break; }

		    // ConsoleOutput
		    if (fd == ConsoleOutput) {
			// Write from user buffer to console
			for (int i = 0; i < size; i++) {
			    int v;
			    if (!machine->ReadMem(userBuf + i, 1, &v)) break;
			    synchconsole->SynchPutChar((char)v);
			}
			machine->WriteRegister(2, size);
			break;
		    }

		    if (!currentThread->space || !currentThread->space->openTable) {
			machine->WriteRegister(2, -1);
			break;
		    }

		    OpenFile* f = currentThread->space->openTable->Get(fd);
		    if (!f) { machine->WriteRegister(2, -1); break; }

		    // Copy from user to kernel buffer then write
		    char* kbuf = new char[size];
		    int copied = 0;
		    for (int i = 0; i < size; i++) {
			int v;
			if (!machine->ReadMem(userBuf + i, 1, &v)) break;
			kbuf[i] = (char)v;
			copied++;
		    }

		    int n = f->Write(kbuf, copied);
		    delete [] kbuf;

		    machine->WriteRegister(2, n);
		    break;
		}
		#endif
		case SC_Fork: {
			VoidFunctionPtr fn_ptr = (VoidFunctionPtr)machine->ReadRegister(4);
			Thread *t = new Thread("Forked Thread");
			t->Fork(fn_ptr, (int)fn_ptr);
			break;
		}

		case SC_Yield: {
			currentThread->Yield();	
			break;
		}
		case SC_PutChar: {
			char ch = (char)machine->ReadRegister(4);
			synchconsole->SynchPutChar(ch);
			break;
		}
		case SC_PutString: {
			char * s = (char *)machine->ReadRegister(4);
			synchconsole->SynchPutString(s);
			break;
		}
		case SC_GetChar: {
			char ch = synchconsole->SynchGetChar();
			machine->WriteRegister(2, (int)ch);
			break;
		}
		case SC_GetString: {
			char * s = (char*)machine->ReadRegister(4);
			int n = (int)machine->ReadRegister(5);
			synchconsole->SynchGetString(s,n);
			machine->WriteRegister(2, (int)s);
			break;
		}
		case SC_PutInt: {
			int n = (int)machine->ReadRegister(4);
			synchconsole->SynchPutInt(n);
			break;
		}
		case SC_GetInt: {
			int  n;
			synchconsole->SynchGetInt(&n);
			machine->WriteRegister(2,n);
			break;
		}
		case SC_feof: {
			machine->WriteRegister(2,synchconsole->feof());
	  		break;		
		}
		case SC_UserThreadCreate: {
			int entry = (int)machine->ReadRegister(4); // The UserThreadRoot function reference
			int f = (int)machine->ReadRegister(5);
			int arg = (int)machine->ReadRegister(6);
			int res = do_UserThreadCreate(entry,f,arg);
			// return the result back to the user
			machine->WriteRegister(2,res);
			break;
		}
		case SC_UserThreadExit: {
			do_UserThreadExit();
			break;
		}
		case SC_UserThreadJoin: {
			int tid = (int)machine->ReadRegister(4);
			do_UserThreadJoin(tid);
			break;			
		}
		case SC_SemInit: {
			int userSemAddr = (int)machine->ReadRegister(4);
			int SemVal = (int)machine->ReadRegister(5);
			int sem_val = do_SemInit(SemVal);
			machine->WriteMem(userSemAddr, sizeof(int), sem_val);
			break;
		}
		case SC_SemWait: {
			int sem_id = (int)machine->ReadRegister(4);
			do_SemWait(sem_id);
			break;
		}
		case SC_SemPost: {
			int sem_id = (int)machine->ReadRegister(4);
			do_SemPost(sem_id);
			break;
		}
		case SC_SemDestroy: {
			int sem_id = (int)machine->ReadRegister(4);
			do_SemDestroy(sem_id);
			break;

		}
		case SC_ForkExec: {
			
			char filename[MAX_FILENAME_LEN];
			int userFileNameAddr = (int)machine->ReadRegister(4);
			
			if(!Helpers::CopyStringFromUser(userFileNameAddr,filename,MAX_FILENAME_LEN)){
				machine->WriteRegister(2,-1); // return -1 as error
				break;
			}
			int pid = do_ForkExec(filename);
			machine->WriteRegister(2,pid);
			break;
		}
		case SC_ProcessJoin: {
			int pid = (int)machine->ReadRegister(4);
			do_ProcessJoin(pid);
			break;		     
		}
		case SC_KDEBUG: {
			char flag = (char)machine->ReadRegister(4);
			char MSG[MAX_FILENAME_LEN];
			int msg_ptr = (int)machine->ReadRegister(5);
					
			Helpers::CopyStringFromUser(msg_ptr,MSG,MAX_FILENAME_LEN);
			DEBUG(flag,MSG);
			break;
		}
		case SC_GetProcessHeapSize: {
			machine->WriteRegister(2, (int)ProcessHeapSize);
			break;
		}
		case SC_Sbrk: {
			machine->WriteRegister(2, currentThread->space->GetBrk());
			break;
		}


		#ifdef NETWORK

		case SC_RNetSocket: {
		    int up = machine->ReadRegister(4);
		    int a[2];
		    if (!CopyArgsFromUser(up, a, 1)) { machine->WriteRegister(2, -1); break; }
		    if (!currentThread->space || !currentThread->space->rnet) { machine->WriteRegister(2, -1); break; }
		    int h = currentThread->space->rnet->Socket(a[0]); // a0 = localMailbox
		    machine->WriteRegister(2, h);
		    break;
		}

		case SC_RNetClose: {
		    int up = machine->ReadRegister(4);
		    int a[2];
		    if (!CopyArgsFromUser(up, a, 1)) { machine->WriteRegister(2, -1); break; }
		    if (!currentThread->space || !currentThread->space->rnet) { machine->WriteRegister(2, -1); break; }
		    machine->WriteRegister(2, currentThread->space->rnet->Close(a[0])); // a0 = handle
		    break;
		}

		case SC_RNetSendFixed: {
		    int up = machine->ReadRegister(4);
		    int a[6];
		    // a0=h a1=dstM a2=dstB a3=buf a4=len
		    if (!CopyArgsFromUser(up, a, 5)) { machine->WriteRegister(2, -1); break; }
		    if (!currentThread->space || !currentThread->space->rnet) { machine->WriteRegister(2, -1); break; }
		    int rc = currentThread->space->rnet->SendFixed(a[0], a[1], a[2], a[3], a[4]);
		    machine->WriteRegister(2, rc);
		    break;
		}

		case SC_RNetRecvFixed: {
		    int up = machine->ReadRegister(4);
		    int a[6];
		    // a0=h a1=buf a2=cap a3=&srcM a4=&srcB
		    if (!CopyArgsFromUser(up, a, 5)) { machine->WriteRegister(2, -1); break; }
		    if (!currentThread->space || !currentThread->space->rnet) { machine->WriteRegister(2, -1); break; }
		    int n = currentThread->space->rnet->RecvFixed(a[0], a[1], a[2], a[3], a[4]);
		    machine->WriteRegister(2, n);
		    break;
		}

		case SC_RNetSendVar: {
		    int up = machine->ReadRegister(4);
		    int a[6];
		    // a0=h a1=dstM a2=dstB a3=buf a4=len
		    if (!CopyArgsFromUser(up, a, 5)) { machine->WriteRegister(2, -1); break; }
		    if (!currentThread->space || !currentThread->space->rnet) { machine->WriteRegister(2, -1); break; }
		    int rc = currentThread->space->rnet->SendVar(a[0], a[1], a[2], a[3], a[4]);
		    machine->WriteRegister(2, rc);
		    break;
		}

		case SC_RNetRecvVarBegin: {
		    int up = machine->ReadRegister(4);
		    int a[6];
		    // a0=h a1=&outLen a2=&srcM a3=&srcB
		    if (!CopyArgsFromUser(up, a, 4)) { machine->WriteRegister(2, -1); break; }
		    if (!currentThread->space || !currentThread->space->rnet) { machine->WriteRegister(2, -1); break; }
		    int rc = currentThread->space->rnet->RecvVarBegin(a[0], a[1], a[2], a[3]);
		    machine->WriteRegister(2, rc);
		    break;
		}

		case SC_RNetRecvVarRead: {
		    int up = machine->ReadRegister(4);
		    int a[6];
		    // a0=h a1=buf a2=cap
		    if (!CopyArgsFromUser(up, a, 3)) { machine->WriteRegister(2, -1); break; }
		    if (!currentThread->space || !currentThread->space->rnet) { machine->WriteRegister(2, -1); break; }
		    int n = currentThread->space->rnet->RecvVarRead(a[0], a[1], a[2]);
		    machine->WriteRegister(2, n);
		    break;
		}

		case SC_RNetRecvVarEnd: {
		    int up = machine->ReadRegister(4);
		    int a[2];
		    // a0=h
		    if (!CopyArgsFromUser(up, a, 1)) { machine->WriteRegister(2, -1); break; }
		    if (!currentThread->space || !currentThread->space->rnet) { machine->WriteRegister(2, -1); break; }
		    int rc = currentThread->space->rnet->RecvVarEnd(a[0]);
		    machine->WriteRegister(2, rc);
		    break;
		}
		case SC_Migrate: {
 		    int dst = machine->ReadRegister(4);   // arg1
    		    int rc = do_Migrate(dst);             
    		    machine->WriteRegister(2, rc);
    		    break;
		}

	#endif // NETWORK

		default: {
			printf("Unexpected user mode exception %d %d\n", which, type);
			ASSERT(FALSE);
		}

	}
    }
    // LB: Do not forget to increment the pc before returning!
    UpdatePC();
    // End of addition
}

#endif // CHANGED
