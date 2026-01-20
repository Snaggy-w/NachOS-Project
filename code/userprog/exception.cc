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
		case SC_Open: {
			char filename[MAX_FILENAME_LEN];
			int userAddr = machine->ReadRegister(4);

			if (!Helpers::CopyStringFromUser(userAddr, filename, MAX_FILENAME_LEN)) {
				machine->WriteRegister(2, -1);
				break;
			}

			OpenFile* file = fileSystem->Open(filename);
			if (file == NULL) {
				machine->WriteRegister(2, -1);
				break;
			}

			int fd = fileSystem->ReturnFileFd(file);
			machine->WriteRegister(2, fd);
			break;
		}
		case SC_Create: {
			char filename[MAX_FILENAME_LEN];
			int userAddr = machine->ReadRegister(4);
			int size = machine->ReadRegister(5);

			if (!Helpers::CopyStringFromUser(userAddr, filename, MAX_FILENAME_LEN)) {
				machine->WriteRegister(2, (int)FALSE);
				break;
			}

			bool res = fileSystem->Create(filename, size);
			machine->WriteRegister(2, res ? (int)TRUE : (int)FALSE);
			break;
		}
		case SC_Read: {
			int userBufAddr = machine->ReadRegister(4);
			int numBytes = machine->ReadRegister(5);
			int fd = machine->ReadRegister(6);

			OpenFileListEntry* entry = (OpenFileListEntry*) openFileList->GetAt(fd);

			if (entry == NULL) {
				machine->WriteRegister(2, -1);
				break;
			}

			char* kernelBuf = new char[numBytes];

			int bytesRead = entry->file->Read(kernelBuf, numBytes);

			for (int i = 0; i < bytesRead; i++) {
				machine->WriteMem(userBufAddr + i, 1, kernelBuf[i]);
			}

			delete[] kernelBuf;
			machine->WriteRegister(2, bytesRead);
			break;
		}
		case SC_Write: {
			int userBufAddr = machine->ReadRegister(4);
			int numBytes = machine->ReadRegister(5);
			int fd = machine->ReadRegister(6);

			OpenFileListEntry* entry = (OpenFileListEntry*) openFileList->GetAt(fd);

			if (entry == NULL) {
				machine->WriteRegister(2, -1);
				break;
			}

			char* kernelBuf = new char[numBytes];

			for (int i = 0; i < numBytes; i++) {
				int value;
				machine->ReadMem(userBufAddr + i, 1, &value);
				kernelBuf[i] = (char)value;
			}

			int bytesWritten = entry->file->Write(kernelBuf, numBytes);

			delete[] kernelBuf;
			machine->WriteRegister(2, bytesWritten);
			break;
		}
		case SC_Close: {
			char filename[MAX_FILENAME_LEN];
			int userAddr = machine->ReadRegister(4);

			if (!Helpers::CopyStringFromUser(userAddr, filename, MAX_FILENAME_LEN)) {
				break;
			}

			fileSystem->Close(filename);
			break;
		}
		case SC_Remove: {
			char filename[MAX_FILENAME_LEN];
			int userFileNameAddr = (int) machine->ReadRegister(4);

			if (!Helpers::CopyStringFromUser(userFileNameAddr, filename, MAX_FILENAME_LEN)) {
				machine->WriteRegister(2, (int)FALSE);
				break;
			}

			bool res = fileSystem->Remove(filename);
			machine->WriteRegister(2, res ? (int)TRUE : (int)FALSE);
			break;
		}
		case SC_List: {
			fileSystem->List();
			break;
		}
		case SC_CreateDirectory: {
			char dirname[MAX_FILENAME_LEN];
			int userDirNameAddr = (int) machine->ReadRegister(4);

			if (!Helpers::CopyStringFromUser(userDirNameAddr, dirname, MAX_FILENAME_LEN)) {
				machine->WriteRegister(2, (int)FALSE);
				break;
			}

			bool res = fileSystem->createDirectory(dirname);
			machine->WriteRegister(2, res ? (int)TRUE : (int)FALSE);
			break;
		}
		case SC_ChangeDirectory: {
			char dirname[MAX_FILENAME_LEN];
			int userDirNameAddr = (int) machine->ReadRegister(4);

			if (!Helpers::CopyStringFromUser(userDirNameAddr, dirname, MAX_FILENAME_LEN)) {
				machine->WriteRegister(2, (int)FALSE);
				break;
			}

			bool res = fileSystem->changeDirectory(dirname);
			machine->WriteRegister(2, res ? (int)TRUE : (int)FALSE);
			break;
		}
		case SC_DeleteDirectory: {
			char dirname[MAX_FILENAME_LEN];
			int userDirNameAddr = (int) machine->ReadRegister(4);

			if (!Helpers::CopyStringFromUser(userDirNameAddr, dirname, MAX_FILENAME_LEN)) {
				machine->WriteRegister(2, (int)FALSE);
				break;
			}

			bool res = fileSystem->deleteDirectory(dirname);
			machine->WriteRegister(2, res ? (int)TRUE : (int)FALSE);
			break;
		}
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
