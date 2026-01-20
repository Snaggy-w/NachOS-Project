#include "opentable.h"
#include "utility.h"
#include "synch.h"

OpenTable::OpenTable()
: lock(new Lock("OpenTable lock")),
  slots(0),
  capacity(16)
{
    slots = new OpenFile*[capacity];
    for (int i = 0; i < capacity; i++) slots[i] = 0;
}

OpenTable::~OpenTable() {
    lock->Acquire();
    for (int i = 0; i < capacity; i++) {
        if (slots[i]) {
            delete slots[i];
            slots[i] = 0;
        }
    }
    lock->Release();
    delete [] slots;
    delete lock;
}

int OpenTable::Add(OpenFile* f) {
    if (!f) return -1;
    lock->Acquire();

    // reserve 0 and 1 for ConsoleInput/ConsoleOutput
    for (int i = 2; i < capacity; i++) {
        if (slots[i] == 0) {
            slots[i] = f;
            lock->Release();
            return i;
        }
    }

    // grow
    int oldCap = capacity;
    int newCap = capacity * 2;
    OpenFile** ns = new OpenFile*[newCap];
    for (int i = 0; i < newCap; i++) ns[i] = 0;
    for (int i = 0; i < oldCap; i++) ns[i] = slots[i];
    delete [] slots;
    slots = ns;
    capacity = newCap;

    // put in first new slot
    int fd = oldCap;
    if (fd < 2) fd = 2;
    slots[fd] = f;

    lock->Release();
    return fd;
}

OpenFile* OpenTable::Get(int fd) {
    lock->Acquire();
    OpenFile* f = 0;
    if (fd >= 0 && fd < capacity) f = slots[fd];
    lock->Release();
    return f;
}

int OpenTable::Remove(int fd) {
    if (fd < 2) return -1; // don't close console here
    lock->Acquire();
    if (fd < 0 || fd >= capacity || slots[fd] == 0) {
        lock->Release();
        return -1;
    }
    OpenFile* f = slots[fd];
    slots[fd] = 0;
    lock->Release();
    delete f;
    return 0;
}

