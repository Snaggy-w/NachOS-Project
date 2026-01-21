// userprog/migrateproto.h
#ifndef MIGRATEPROTO_H
#define MIGRATEPROTO_H

#define MIG_MAGIC 0x4D494731  // "MIG1"
			

struct MigBegin {
    int magic; // the magic header
    int numPages; // number of virtual pages in the migrating process
    int brk; // the process heap break
    int regsBytes;    // NumTotalRegs*sizeof(int) -> size of the register blob
    int memBytes;     // numPages*PageSize -> size of memory snapshot
    int filesBytes;   // optional blob to handle file table entries...
};

#endif

