#include "syscall.h"
#include "libstr.h"


// a simple shell that just executes binaries

int main() {
    char prompt[] = "$ ";
    char buffer[256];
    char arg1[255];
    char arg2[2];

    while (1) {
        PutString(prompt);
        GetString(buffer, 256);
	
	// remove the trailing \n from the string
	int i = 0;
	while (buffer[i] != '\0') {
	    if (buffer[i] == '\n' || buffer[i] == '\r') {
		    buffer[i] = '\0';
		    break;
	    }
	    i++;
	}

        arg1[0] = '\0';
        arg2[0] = '\0';
        // parse the command to see if we want to background the process or execute it in the foreground
        parse_str_cmd(0, buffer, arg1, 255);
        parse_str_cmd(1, buffer, arg2, 2);

        if (arg1[0] == '\0')
            continue; // empty command

        if(str_cmp(arg1,"exit") == 0)
            break;
        
        if (arg2[0] == '&') { // background process
            while(ForkExec(arg1) == -1){
		PutString("Out of memory");	
                ProcessJoin(-1);
	    }
        } else { // foreground process
            int pid = -1;
            while( (pid = ForkExec(arg1)) == -1){
		PutString("Out of memory");
                ProcessJoin(-1);
	    }
            if (pid >= 0)
                ProcessJoin(pid); // wait for it to exit before continuing
        }
    } 
    Halt();
}
