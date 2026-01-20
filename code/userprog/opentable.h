#ifndef OPENTABLE_H
#define OPENTABLE_H

#include "openfile.h"

class Lock;
class OpenTable {
public:
    OpenTable();
    ~OpenTable();

    // Returns a small int fd >= 2 (0/1 reserved for console in case we want to assign them an fd for some reason).
    int Add(OpenFile* f);
    OpenFile* Get(int fd);
    int Remove(int fd); // closes + deletes entry, returns 0 or -1

private:
    Lock* lock;
    OpenFile** slots;
    int capacity;
};

#endif

