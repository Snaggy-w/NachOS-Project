#include "opentable.h"
#include <string.h>     // memset, strncpy

/* helper to copy file name safely */
static void CopyName(char* dst, const char* src) {
    if (!dst) return;
    if (!src) { dst[0] = 0; return; }

    // safe copy + NUL terminate
    int i = 0;
    for (; i < MAX_FILENAME_LEN - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}
// constructor
OpenTable::OpenTable()
: lock(new Lock("OpenTable lock")),
  slots(0),
  capacity(16)
{
    slots = new Entry[capacity];
    for (int i = 0; i < capacity; i++) {
        slots[i].used = false;
        slots[i].f = 0;
        slots[i].name[0] = 0;
        slots[i].pos = 0;
    }
}
// destructor
OpenTable::~OpenTable() {
    lock->Acquire();
    for (int i = 0; i < capacity; i++) {
        if (slots[i].used && slots[i].f) {
            delete slots[i].f;
            slots[i].f = 0;
        }
        slots[i].used = false;
        slots[i].name[0] = 0;
        slots[i].pos = 0;
    }
    lock->Release();

    delete [] slots;
    delete lock;
}

/* Grow:
 *	increases the size of the fd table
 */
void OpenTable::Grow() {
    int oldCap = capacity;
    int newCap = capacity * 2;

    Entry* ns = new Entry[newCap]; // create a larger buffer
    for (int i = 0; i < newCap; i++) {
        ns[i].used = false;
        ns[i].f = 0;
        ns[i].name[0] = 0;
        ns[i].pos = 0;
    }

    for (int i = 0; i < oldCap; i++) ns[i] = slots[i]; // copy the old small buffer contents

    delete [] slots; // delete the old buffer
    slots = ns; // re-assign to the new buffer
    capacity = newCap; // update the capacity
}

// a wrapper around AddWithName that just doesn't set a filename
int OpenTable::Add(OpenFile* f) {
    return AddWithName(f, 0);
}

int OpenTable::AddWithName(OpenFile* f, const char* name) {
    if (!f) return -1;
    lock->Acquire();

    // reserve 0/1 in case we want these to refer to the console in/out for example
    for (int i = 2; i < capacity; i++) {
        if (!slots[i].used) {
            slots[i].used = true;
            slots[i].f = f;
            CopyName(slots[i].name, name);
            slots[i].pos = f->Tell(); // get the seek position
            lock->Release();
            return i;
        }
    }

    Grow();

    // first new slot after grow is oldCap
    int fd = capacity / 2;
    if (fd < 2) fd = 2;
    // setup the fd entry
    slots[fd].used = true;
    slots[fd].f = f;
    CopyName(slots[fd].name, name);
    slots[fd].pos = f->Tell();

    lock->Release();
    return fd;
}

/* Get:
 *	gets a file handler with descriptor
 */
OpenFile* OpenTable::Get(int fd) {
    lock->Acquire();
    OpenFile* f = 0;
    if (fd >= 0 && fd < capacity && slots[fd].used) f = slots[fd].f;
    lock->Release();
    return f;
}

/* Remove:
 *	removes and cleans an entry in the descriptor table using an fd
 */
int OpenTable::Remove(int fd) {
    if (fd < 2) return -1;

    lock->Acquire();
    if (fd < 0 || fd >= capacity || !slots[fd].used) {
        lock->Release();
        return -1;
    }

    OpenFile* f = slots[fd].f;
    slots[fd].used = false;
    slots[fd].f = 0;
    slots[fd].name[0] = 0;
    slots[fd].pos = 0;

    lock->Release();

    delete f;
    return 0;
}

/* IsUsed:
 *	checks whether a slot in the descriptor table is used
 */
bool OpenTable::IsUsed(int fd) {
    lock->Acquire();
    bool u = (fd >= 0 && fd < capacity && slots[fd].used);
    lock->Release();
    return u;
}

/* Name:
 *	gets the name of the file based on its descriptor
 */
const char* OpenTable::Name(int fd) {
    lock->Acquire();
    const char* n = 0;
    if (fd >= 0 && fd < capacity && slots[fd].used) n = slots[fd].name;
    lock->Release();
    return n;
}

/* TellPos:
 *	gets the seek position of a file based on the fd provided
 */
int OpenTable::TellPos(int fd) {
    lock->Acquire();
    int p = -1;
    if (fd >= 0 && fd < capacity && slots[fd].used && slots[fd].f) {
        p = slots[fd].f->Tell();
        slots[fd].pos = p;
    }
    lock->Release();
    return p;
}

/* InstallAt:
 *	helper function useful for migrated processes to install a specific file 
 *	in the handle entry that is the same in the source machine
 */
bool OpenTable::InstallAt(int fd, OpenFile* f, const char* name, int pos) {
    if (!f) return false;
    if (fd < 2) return false;

    lock->Acquire();
    while (fd >= capacity) Grow(); // increase if needed

    if (slots[fd].used && slots[fd].f) {
        delete slots[fd].f; // overwrite the file at that slot
    }

    slots[fd].used = true;
    slots[fd].f = f;
    CopyName(slots[fd].name, name);
    slots[fd].pos = pos;

    lock->Release();

    f->Seek(pos); // seek to the position of that file
    return true;
}

