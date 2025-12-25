#ifdef CHANGED
#include"copyright.h"
#include "system.h"
#include "synchconsole.h"
#include "synch.h"

static Semaphore *readAvail;
static Semaphore *writeDone;
static Semaphore *consoleLock; // A lock to block other threads from calling PutChar before the function is finished

static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

static Lock *stringBuffLock; // Lock to protect the string buffer
static Lock *stringConsoleLock; // Lock to avoid race conditions on reading strings

SynchConsole::SynchConsole(char *readFile, char *writeFile){
	readAvail = new Semaphore("read avail", 0);
	writeDone = new Semaphore("write done", 0);
	consoleLock = new Semaphore("Console Lock",1);
	console = new Console(readFile, writeFile, ReadAvail, WriteDone, 0);
	// initialize locks ??
	stringBuffLock = new Lock("String output Buffer Lock");
	stringConsoleLock = new Lock("console string reading lock");
}
SynchConsole::~SynchConsole(){
	delete console;
	delete writeDone;
	delete readAvail;
	delete consoleLock;
	delete stringBuffLock;
	delete stringConsoleLock;
}
void SynchConsole::SynchPutChar(const char ch){
	consoleLock->P(); // lock access to the console
	console->PutChar(ch);
	writeDone->P();
	consoleLock->V();
}

bool SynchConsole::feof() const {
	return this->eof;
}

int SynchConsole::SynchGetChar(){
   	readAvail->P();

        int ch = console->GetChar();

	if(ch == -1){
		eof = true;
		return EOF;
	}

	return ch & 0xFF;
}

void SynchConsole::copyStringFromMachine( int from, char *to, unsigned size) {
	int ch;
	unsigned int i = 0;
	
	while (i < size) {
		machine->ReadMem(from + i, 1, &ch);
		to[i] = (char)(ch);
		if ((char)ch == '\0') {
			break;
		}
		i++;
	}
	to[i] = '\0';
}

void SynchConsole::SynchPutString(const char s[]){
	// making put string thread safe with a lock
	// The copy and memset functions completely overwrite the buffer corrupting data
	// so they have to be in a critical section
	stringBuffLock->Acquire(); {
		copyStringFromMachine((int)s, STRING_BUFFER, MAX_STRING_SIZE);
		unsigned int i = 0;
		while (STRING_BUFFER[i] != '\0' && i < MAX_STRING_SIZE) {
			SynchPutChar(STRING_BUFFER[i]);
			i++;
		}
		memset(STRING_BUFFER,'\0', MAX_STRING_SIZE);
	} stringBuffLock->Release();
}
void SynchConsole::SynchGetString(char *s, int n){
	stringConsoleLock->Acquire(); {
		int i = 0;
		while (i < n) {
			int ch = SynchGetChar();
			if (ch == EOF) {
				i++;
				break;
			} 
			if (ch == '\n'){
				//s[i] = ch;
				machine->WriteMem((int)(s+i),1,(int)ch);
				i++;
				break;
			} 
			//s[i] = ch;
			machine->WriteMem((int)(s+i),1,(int)ch);
			i++;
		}
		machine->WriteMem((int)(s+i),1,0); // \0 
	} stringConsoleLock->Release();
}

void SynchConsole::SynchPutInt(int n){
	// max sined value is 2147483647 -> 10 characters max
	char txt[12]; // account for minus character and Null terminator
	snprintf(txt,sizeof(txt),"%d",n);
	// Beware that we cannot reuse SynchPutString here since it assumes a string comming from user space
	// Here the string is created in kernel space
	for (unsigned i = 0; txt[i] != '\0'; i++) {
        	this->SynchPutChar(txt[i]);
    	}
}

void SynchConsole::SynchGetInt(int *n){
	int i = 0;
	char s[12];
	while (i < 11) {
		char ch = SynchGetChar();
		if(ch < 48 || ch > 57) // The character is not a number
			break;
		 
		s[i] = ch;
		i++;
	}
	s[i] = '\0';
	
	/*
	 * In case of input that starts with a non-numerical value -> s = '\0'
	 * sscanf will fail and return 0 and not write to the buffer n
	 * so n will contain garbage data since it is not initialized
	 * 
	 * To fix this issue we add a check on the return of sscanf and in case of failure, initialize the buffer with 0 
	 */

	if(sscanf(s,"%d",n) != 1)
		*n = 0;
}
#endif //CHANGED
