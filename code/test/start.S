/* Start.s 
 *	Assembly language assist for user programs running on top of Nachos.
 *
 *	Since we don't want to pull in the entire C library, we define
 *	what we need for a user program here, namely Start and the system
 *	calls.
 */

#include "syscall.h"

        .text   
        .align  2

/* -------------------------------------------------------------
 * __start
 *	Initialize running a C program, by calling "main". 
 *
 * 	NOTE: This has to be first, so that it gets loaded at location 0.
 *	The Nachos kernel always starts a program by jumping to location 0.
 * -------------------------------------------------------------
 */

.weak	user_init

	.globl __start
	.ent	__start
__start:
	//jal	main
	jal	premain // jump to stub that will setup process before main
	move	$4,$0		
	jal	Exit	 /* if we return from main, exit(0) */
	.end __start


	.globl	premain
	.ent	premain
premain:
	jal	user_init
	nop
	jal	main
	move	$4,$0
	j Exit
	.end	premain


/* -------------------------------------------------------------
 * System call stubs:
 *	Assembly language assist to make system calls to the Nachos kernel.
 *	There is one stub per system call, that places the code for the
 *	system call into register r2, and leaves the arguments to the
 *	system call alone (in other words, arg1 is in r4, arg2 is 
 *	in r5, arg3 is in r6, arg4 is in r7)
 *
 * 	The return value is in r2. This follows the standard C calling
 * 	convention on the MIPS.
 * -------------------------------------------------------------
 */

	.globl Halt
	.ent	Halt
Halt:
	addiu $2,$0,SC_Halt
	syscall
	j	$31
	.end Halt

	.globl Exit
	.ent	Exit
Exit:
	addiu $2,$0,SC_Exit
	syscall
	j	$31
	.end Exit

	.globl Exec
	.ent	Exec
Exec:
	addiu $2,$0,SC_Exec
	syscall
	j	$31
	.end Exec

	.globl Join
	.ent	Join
Join:
	addiu $2,$0,SC_Join
	syscall
	j	$31
	.end Join

	.globl Create
	.ent	Create
Create:
	addiu $2,$0,SC_Create
	syscall
	j	$31
	.end Create

	.globl Open
	.ent	Open
Open:
	addiu $2,$0,SC_Open
	syscall
	j	$31
	.end Open

	.globl Read
	.ent	Read
Read:
	addiu $2,$0,SC_Read
	syscall
	j	$31
	.end Read

	.globl Write
	.ent	Write
Write:
	addiu $2,$0,SC_Write
	syscall
	j	$31
	.end Write

	.globl Close
	.ent	Close
Close:
	addiu $2,$0,SC_Close
	syscall
	j	$31
	.end Close

	.globl Fork
	.ent	Fork
Fork:
	addiu $2,$0,SC_Fork
	syscall
	j	$31
	.end Fork

	.globl Yield
	.ent	Yield
Yield:
	addiu $2,$0,SC_Yield
	syscall
	j	$31
	.end Yield

	
/*
 *
 * The PutChar stub, it will be triggered by the system call SC_PutChar defined
 * in userprog/syscall.h as syscall 11
 *
 *
*/
	.globl  PutChar
	.ent	PutChar
PutChar:
	addiu $2, $0,SC_PutChar
	syscall
	j	$31
	.end PutChar


	.globl  PutString
	.ent	PutString
PutString:
	addiu $2, $0,SC_PutString
	syscall
	j	$31
	.end PutString

	.globl  GetChar
	.ent	GetChar
GetChar:
	addiu $2, $0,SC_GetChar
	syscall
	j	$31
	.end GetChar

	.globl  GetString
	.ent	GetString
GetString:
	addiu $2, $0,SC_GetString
	syscall
	j	$31
	.end GetString

	.globl  PutInt
	.ent	PutInt
PutInt:
	addiu $2, $0,SC_PutInt
	syscall
	j	$31
	.end PutInt

	.globl  GetInt
	.ent	GetInt
GetInt:
	addiu $2, $0,SC_GetInt
	syscall
	j	$31
	.end GetInt

	.globl  feof
	.ent	feof
feof: // kernel based state getter, return whether a real EOF is detected regardless of encoding (hopefully)
	addiu $2, $0,SC_feof
	syscall
	j	$31
	.end feof
/*
	 The function entry is now in the libthread library
	.globl  UserThreadCreate
	.ent	UserThreadCreate
UserThreadCreate: 
	addiu $2, $0, SC_UserThreadCreate
	syscall
	j	$31
	.end UserThreadCreate
*/
	.globl  UserThreadJoin
	.ent	UserThreadJoin
UserThreadJoin: 
	addiu $2, $0, SC_UserThreadJoin
	syscall
	j	$31
	.end UserThreadJoin
	

	.globl  UserThreadExit
	.ent	UserThreadExit
UserThreadExit: 
	addiu $2, $0, SC_UserThreadExit
	syscall
	j	$31
	.end UserThreadExit
	
	.globl  SemInit
	.ent	SemInit
SemInit:
	addiu $2, $0, SC_SemInit
	syscall
	j	$31
	.end 	SemInit

	.globl  SemWait
	.ent	SemWait
SemWait:
	addiu $2, $0, SC_SemWait
	syscall
	j	$31
	.end 	SemWait

	.globl  SemPost
	.ent	SemPost
SemPost:
	addiu $2, $0, SC_SemPost
	syscall
	j	$31
	.end 	SemPost

	.globl  SemDestroy
	.ent	SemDestroy
SemDestroy:
	addiu $2, $0, SC_SemDestroy
	syscall
	j	$31
	.end 	SemDestroy

	.globl  ForkExec
	.ent	ForkExec
ForkExec:
	addiu $2, $0, SC_ForkExec
	syscall
	j	$31
	.end 	ForkExec

	.globl  KDEBUG
	.ent	KDEBUG
KDEBUG:
	addiu $2, $0, SC_KDEBUG
	syscall
	j	$31
	.end 	KDEBUG

	.globl  ProcessJoin
	.ent	ProcessJoin
ProcessJoin:
	addiu $2, $0, SC_ProcessJoin
	syscall
	j	$31
	.end 	ProcessJoin

	.globl  GetProcessHeapSize
	.ent	GetProcessHeapSize
GetProcessHeapSize:
	addiu $2, $0, SC_GetProcessHeapSize
	syscall
	j	$31
	.end 	GetProcessHeapSize

	.globl  Sbrk
	.ent	Sbrk
Sbrk:
	addiu $2, $0, SC_Sbrk
	syscall
	j	$31
	.end 	Sbrk

	.globl  RNetSocket
	.ent    RNetSocket
RNetSocket:
    	addiu $2,$0,SC_RNetSocket
   	syscall
    	j   $31
    	.end RNetSocket

    	.globl  RNetClose
    	.ent    RNetClose
RNetClose:
    	addiu $2,$0,SC_RNetClose
    	syscall
    	j   $31
    	.end RNetClose

    	.globl  RNetSendFixed
    	.ent    RNetSendFixed
RNetSendFixed:
    	addiu $2,$0,SC_RNetSendFixed
    	syscall
    	j   $31
    	.end RNetSendFixed

    	.globl  RNetRecvFixed
    	.ent    RNetRecvFixed
RNetRecvFixed:
    	addiu $2,$0,SC_RNetRecvFixed
    	syscall
    	j   $31
    	.end RNetRecvFixed

    	.globl  RNetSendVar
    	.ent    RNetSendVar
RNetSendVar:
    	addiu $2,$0,SC_RNetSendVar
    	syscall
    	j   $31
    	.end RNetSendVar

    	.globl  RNetRecvVarBegin
    	.ent    RNetRecvVarBegin
RNetRecvVarBegin:
    	addiu $2,$0,SC_RNetRecvVarBegin
    	syscall
    	j   $31
    	.end RNetRecvVarBegin

    	.globl  RNetRecvVarRead
    	.ent    RNetRecvVarRead
RNetRecvVarRead:
    	addiu $2,$0,SC_RNetRecvVarRead
    	syscall
    	j   $31
    	.end RNetRecvVarRead

    	.globl  RNetRecvVarEnd
    	.ent    RNetRecvVarEnd
RNetRecvVarEnd:
    	addiu $2,$0,SC_RNetRecvVarEnd
    	syscall
    	j   $31
    	.end RNetRecvVarEnd


	.globl  Remove
	.ent	Remove
Remove:
	addiu $2, $0, SC_Remove
	syscall
	j	$31
	.end 	Remove

	.globl  List
	.ent	List
List:
	addiu $2, $0, SC_List
	syscall
	j	$31
	.end 	List

	.globl  CreateDirectory
	.ent	CreateDirectory
CreateDirectory:
	addiu $2, $0, SC_CreateDirectory
	syscall
	j	$31
	.end 	CreateDirectory

	.globl  ChangeDirectory
	.ent	ChangeDirectory
ChangeDirectory:
	addiu $2, $0, SC_ChangeDirectory
	syscall
	j	$31
	.end 	ChangeDirectory

	.globl  DeleteDirectory
	.ent	DeleteDirectory
DeleteDirectory:
	addiu $2, $0, SC_DeleteDirectory
	syscall
	j	$31
	.end 	DeleteDirectory

/* dummy function to keep gcc happy */
        .globl  __main
        .ent    __main
__main:
        j       $31
        .end    __main

