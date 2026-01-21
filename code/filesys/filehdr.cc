// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
//	Allocate data blocks for the file out of the map of free disk blocks
//  whenever additional "size" bytes for the files are required.
//	Return FALSE if there are not enough space to accomodate
//	"size" bytes.
//
//	"freeMap" is the bit map of free disk sectors
//	"size" is the number of bytes that need to be allocated
//   in addition to the exisiting file bytes
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int size)
{ 
    int resultNumBytes = numBytes + size;
    int numNewSectors = divRoundUp(size, SectorSize);
    int resultNumSectors = numSectors + numNewSectors;

    DEBUG('f', "Allocating %d data sectors for an existing file of %d sectors...\n", numNewSectors, numSectors);


    bool needNewIndirectHdr = (numSectors <= (int)NumPrimary) && (resultNumSectors > (int)NumPrimary);
    int indirectHdrSectorNum = needNewIndirectHdr ? 1 : 0;
    int totalNumNewSectors = numNewSectors + indirectHdrSectorNum;

    if (freeMap->NumClear() < totalNumNewSectors || resultNumBytes > (int)MaxFileSize) {
	    return FALSE;		// not enough space
    }

    // allocating second level sectors for the first time
    if (needNewIndirectHdr) {
        DEBUG('f', "Allocating indirect file header...\n", size);
        indirectIndexHdr = new FileHeader;
        dataSectors[NumPrimary] = freeMap->Find();
        DEBUG('f', "Allocated indirect file header at sector %d\n", dataSectors[NumPrimary]);
    }

    for (int i = 0; i < numNewSectors; i++) {
        int sectorId = i + numSectors;
        if (sectorId < (int)NumPrimary) {
	        dataSectors[sectorId] = freeMap->Find();
        } else {
            ASSERT(indirectIndexHdr != NULL);
            indirectIndexHdr->dataSectors[sectorId - (int)NumPrimary] = freeMap->Find();
        }
    }

    numBytes = resultNumBytes;
    numSectors = resultNumSectors;
    
    DEBUG('f', "Allocated data sectors!\n");

    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    DEBUG('f', "Data sectors deallocating...\n");
    int sector;
    for (int i = 0; i < numSectors; i++) {
        if (i < (int)NumPrimary) {
            sector = dataSectors[i];
        } else {
            sector = indirectIndexHdr->dataSectors[i - NumPrimary];
        }
	    ASSERT(freeMap->Test(sector));  // ought to be marked!
	    freeMap->Clear(sector);
    }
    if (numSectors > (int)NumPrimary) {
        sector = dataSectors[NumPrimary];
        ASSERT(freeMap->Test(sector));  // ought to be marked!
	    freeMap->Clear(sector);
    }
    DEBUG('f', "Deallocated data sectors...\n");

}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    DEBUG('f', "Fetching file header from disk sector %d...\n", sector);

    synchDisk->ReadSector(sector, (char *)this);

    // the second-level data blocks are present
    // this condition is never satisfied within indirect index headers
    if (numSectors > (int)NumPrimary) { 
        indirectIndexHdr->FetchFrom(dataSectors[NumPrimary]);
        DEBUG('f', "Fetched secondary file header from disk sector %d...\n", 
            dataSectors[NumPrimary]);

    }
    DEBUG('f', "Fetched file header from disk! \n", sector);

}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    DEBUG('f', "Writing file header to disk sector %d...\n", sector);

    synchDisk->WriteSector(sector, (char *)this); 

    if (indirectIndexHdr != NULL) {
        indirectIndexHdr->WriteBack(dataSectors[NumPrimary]);
        DEBUG('f', "Done writing secondary file header to disk sector %d...\n", dataSectors[NumPrimary]);

    }
    DEBUG('f', "Done writing file header to disk!\n");

}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    int sectorId = offset / SectorSize;
    if (sectorId <= (int)NumPrimary) {
        return(dataSectors[offset / SectorSize]);
    } else { // search among second-level data sectors
        int secondarySectorId = sectorId - NumPrimary;
        return indirectIndexHdr->dataSectors[secondarySectorId];
    }
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++) {
        if (i < (int)NumPrimary) {
	        printf("%d ", dataSectors[i]);
        } else {
            int secondaryLevelSectorId = i - NumPrimary;
            printf("%d ", indirectIndexHdr->dataSectors[secondaryLevelSectorId]);

        }
    }
    printf("\nFile contents:\n");
    int sector;
    for (i = k = 0; i < numSectors; i++) {
        if (i < (int)NumPrimary) {
	        sector = dataSectors[i];
        } else {
            int secondaryLevelSectorId = i - NumPrimary;
            sector = indirectIndexHdr->dataSectors[secondaryLevelSectorId];

        }
	    synchDisk->ReadSector(sector, data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	        if ('\040' <= data[j] && data[j] <= '\176')  { // isprint(data[j])
		        printf("%c", data[j]);
            } else {
		    printf("\\%x", (unsigned char)data[j]);
            }
	    }
        printf("\n"); 
    }
    delete [] data;
}
