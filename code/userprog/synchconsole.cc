#ifdef CHANGED
#include"copyright.h"
#include "system.h"
#include "synchconsole.h"
#include "synch.h"

static Semaphore *readAvail;
static Semaphore *writeDone;

static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

SynchConsole::SynchConsole(char *readFile, char *writeFile){
	readAvail = new Semaphore("read avail", 0);
	writeDone = new Semaphore("write done", 0);
	console = new Console(readFile, writeFile, ReadAvail, WriteDone, 0);
}
SynchConsole::~SynchConsole(){
	delete console;
	delete writeDone;
	delete readAvail;
}
void SynchConsole::SynchPutChar(const char ch){
	console->PutChar(ch);
	writeDone->P();
}
char SynchConsole::SynchGetChar(){
   	readAvail->P();
    return console->GetChar();
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
	copyStringFromMachine((int)s, STRING_BUFFER, MAX_STRING_SIZE);
	unsigned int i = 0;
	while (STRING_BUFFER[i] != '\0' || i == MAX_STRING_SIZE) {
		SynchPutChar(STRING_BUFFER[i]);
		i++;
	}
	memset(STRING_BUFFER,'\0', MAX_STRING_SIZE);
}
void SynchConsole::SynchGetString(char *s, int n){
// ...
}
#endif //CHANGED
