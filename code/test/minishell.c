#include "syscall.h"
#include "libstr.h"


//NOTE : Security checks are not made yet, we are trusting the user!

int main() {
    char prompt[] = "$ ";
    char buffer[256];
    char arg1[255];
    char arg2[255];
    char arg3[255];
    char arg4[255];
    int RET_VAL = -1;

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
        arg3[0] = '\0';
        arg4[0] = '\0';

        parse_str_cmd(0, buffer, arg1, 255);
        parse_str_cmd(1, buffer, arg2, 255);
        parse_str_cmd(2 , buffer, arg3, 255);
        parse_str_cmd(3, buffer, arg4, 255);

        if (arg1[0] == '\0')
            continue; // empty command

        if(str_cmp(arg1,"exit") == 0)
            break;
        
        if(str_cmp(arg1,"$?") == 0) //same logic as in UNIX
            PutInt(RET_VAL);
        
        if (arg2[0] == '&') {
            while(ForkExec(arg1) == -1){
		        PutString("Out of memory");	
                ProcessJoin(-1);
	        }
        } 
        else if(str_cmp(arg1,"touch") == 0) { 
           RET_VAL = Create(arg2, atoi(arg3));
        }
        else if(str_cmp(arg1,"rm") == 0) {
            RET_VAL = Remove(arg2);
        }
        else if(str_cmp(arg1,"o") == 0){
            RET_VAL = Open(arg2);
        }
        else if(str_cmp(arg1,"w") == 0){
            Write(arg2, atoi(arg3), atoi(arg4));
        }
        else if(str_cmp(arg1,"r") == 0){
            int readSize = atoi(arg2);
            int fd = atoi(arg3);

            if (readSize <= 0) {
                PutString("Invalid read size\n");
                continue;
            }

            char tmp_buf[readSize + 1];
            int n = Read(tmp_buf, readSize, fd);

            if (n > 0) {
                tmp_buf[n] = '\0';
                PutString(tmp_buf);
            }

            RET_VAL = n;
        }
        else if(str_cmp(arg1,"close") == 0) {
            Close(arg2);
        }
        else if(str_cmp(arg1,"l") == 0) { //lists current directory
            List();
        }
        else if(str_cmp(arg1,"mkdir") == 0){
            RET_VAL = CreateDirectory(arg2);
        }
        else if(str_cmp(arg1,"cd") == 0){
            RET_VAL = ChangeDirectory(arg2);
        }
        else if(str_cmp(arg1,"rmd") == 0){ 
            RET_VAL = DeleteDirectory(arg2);
        }
        else if (str_cmp(arg1, "help") == 0) {
            PutString(
                "Available commands:\n\n"
                "exit\n"
                "    Exit the shell\n\n"
                "$?\n"
                "    Print return value of last command\n\n"
                "touch <name> <size>\n"
                "    Create a file with given size in bytes\n\n"
                "rm <name>\n"
                "    Remove a file\n\n"
                "o <name>\n"
                "    Open a file, returns file descriptor\n\n"
                "w <buffer> <size> <fd>\n"
                "    Write <size> bytes from <buffer> to file <fd>\n\n"
                "r <size> <fd>\n"
                "    Read <size> bytes from file <fd> and print to console\n\n"
                "close <fd>\n"
                "    Close an open file\n\n"
                "l\n"
                "    List current directory contents\n\n"
                "mkdir <name>\n"
                "    Create a directory\n\n"
                "cd <name>\n"
                "    Change current directory\n\n"
                "rmd <name>\n"
                "    Remove a directory\n\n"
                "<program> &\n"
                "    Run program in background\n"
            );
        }
        else {
            PutString("Unknown command\n");
        }
    } 
    Halt();
}
