#ifndef OPENTABLE_H
#define OPENTABLE_H

#include "openfile.h"   // OpenFile
#include "synch.h"      // Lock

// set the max file name length in case it is undefined
#ifndef MAX_FILENAME_LEN
#define MAX_FILENAME_LEN 256
#endif

class OpenTable {
public:
    OpenTable();
    ~OpenTable();

    // adds a file to the file descriptor table
    int Add(OpenFile* f);

    // store filename metadata with the descriptor
    int AddWithName(OpenFile* f, const char* name);

    OpenFile* Get(int fd); // return a file handle for the descriptor provided
    int Remove(int fd); // closes + deletes entry and returns 0/-1

    // Helpers (useful for migration / debugging)
    bool IsUsed(int fd); // returns whether an fd slot is currently active or not
    const char* Name(int fd); // returns the filename associated with the file
    int TellPos(int fd); // gets the current cursor offset of an open file

    // useful to put back the file in the same slot it was on in the source machine after migration
    bool InstallAt(int fd, OpenFile* f, const char* name, int pos); // pos is used to seek to the position in the file needed

private:
    // each fd will map to one of these structures of type Entry
    struct Entry {
        bool used;
        OpenFile* f;
        char name[MAX_FILENAME_LEN];
        int pos;
    };
    // increase the capacity of the table when it is full
    void Grow();

    Lock* lock;
    Entry* slots; // fd table
    int capacity; // capacity of fd table
};

#endif // OPENTABLE_H

