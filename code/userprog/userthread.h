#ifndef USERTHREAD_H
#define USERTHREAD_H

#include "thread.h"
#include "system.h"
#include "machine.h"

extern void StartUserThread();
extern int do_UserThreadCreate(int f, int arg);

#endif