#ifdef CHANGED
#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H
#include "copyright.h"
#include "utility.h"
#include "console.h"
class SynchConsole {
	public:
		bool feof() const;
		SynchConsole(char *readFile, char *writeFile);
	// initialize the hardware console device
		~SynchConsole();
	 // clean up console emulation
		void SynchPutChar(const char ch);
		int SynchGetChar();
	// Unix putchar(3S)
	// Unix getchar(3S)
		void SynchPutString(const char *s); // Unix puts(3S)
		void SynchGetString(char *s, int n);
	 // Unix fgets(3S)
		void SynchPutInt(int n);
		void SynchGetInt(int *n);
	private:
		bool eof = false;
		Console *console;
		void copyStringFromMachine( int from, char *to, unsigned size);
};
#endif 	//SYNCHCONSOLE_H
#endif //CHANGED
