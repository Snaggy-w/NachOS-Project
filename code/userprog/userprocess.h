#ifndef USERPROCESS_H
#define USERPROCESS_H

// forks a kernel thread which should run a process of the specified executable
int do_ForkExec(char* executable);
void do_Exit(int status);
#endif

