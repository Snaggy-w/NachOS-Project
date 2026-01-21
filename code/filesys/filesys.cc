// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "filehdr.h"
#include "filesys.h"
#include "system.h"
#include <cstdlib>

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
#define NumDirEntries 		10
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

void PrintOpenFileEntry(int item) {
    OpenFileListEntry *entry = (OpenFileListEntry *) item;

    ASSERT(entry != NULL);

    DEBUG('f',"OpenFileEntry: sector = %d, file = %p\n",
           entry->sector,
           entry->file);
}

bool MatchSector(void* item, void* arg) {
    ASSERT(item != nullptr);
    OpenFileListEntry* entry = (OpenFileListEntry*) item;
    ASSERT(entry->file != nullptr);
    int sector = *(int*) arg;
    return entry->sector == sector;
}

bool MatchOpenFilePtr(void* item, void* arg) {
    OpenFileListEntry* entry = (OpenFileListEntry*) item;
    OpenFile* file = (OpenFile*) arg;
    return entry->file == file;
}

int FileSystem::ReturnFileFd(OpenFile* file) { //returns -1 if file not in the list
        return openFileList->FindIf(MatchOpenFilePtr, file);
}

FileSystem::FileSystem(bool format)
{ 
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        directory->sector = DirectorySector;
	FileHeader *mapHdr = new FileHeader;
	FileHeader *dirHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

    // First, allocate space for FileHeaders for the directory and bitmap
    // (make sure no one else grabs these!)
	freeMap->Mark(FreeMapSector);	    
	freeMap->Mark(DirectorySector);

    // Second, allocate space for the data blocks containing the contents
    // of the directory and bitmap files.  There better be enough space!

	ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
	ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

    // Flush the bitmap and directory FileHeaders back to disk
    // We need to do this before we can "Open" the file, since open
    // reads the file header off of disk (and currently the disk has garbage
    // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
	mapHdr->WriteBack(FreeMapSector);    
	dirHdr->WriteBack(DirectorySector);

    // OK to open the bitmap and directory files now
    // The file system operations assume these two files are left open
    // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
    // Once we have the files "open", we can write the initial version
    // of each file back to disk.  The directory at this point is completely
    // empty; but the bitmap has been changed to reflect the fact that
    // sectors on the disk have been allocated for the file headers and
    // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
	freeMap->WriteBack(freeMapFile);	 // flush changes to disk
	directory->WriteBack(directoryFile);

	if (DebugIsEnabled('f')) {
	    freeMap->Print();
	    directory->Print();

        delete freeMap; 
	delete directory; 
	delete mapHdr; 
	delete dirHdr;
	}
    } else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector); // for now the current directory open file is pointing to the root directory
    }
    // currentDirectory is initialized in system.h and system.cc
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool
FileSystem::Create(const char *path, int initialSize)
{
    char fileName[FileNameMaxLen];
    Directory* parentDir = resolvePath(path, fileName);
    if (parentDir == nullptr)
        return FALSE;

    DEBUG('f', "Creating file %s of size %d in directory at sector %d\n", fileName, initialSize, parentDir->sector);

    if (parentDir->Find(fileName) != -1) {
      return FALSE;			// file is already in directory
    }	

    BitMap *freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    int sector = freeMap->Find();	// find a sector to hold the file header
    if (sector == -1) 	{	
        delete freeMap;
        return FALSE;		// no free block for file header 
    }    

    if (!parentDir->Add(fileName, sector)) {
        delete freeMap;
        return FALSE;	// no space in directory
    }

    FileHeader* hdr = new FileHeader;
    if (!hdr->Allocate(freeMap, initialSize)) {
        delete freeMap;
        return FALSE;	// no space on disk for data
    }
    	
    // everything worked, flush all changes back to disk
    hdr->WriteBack(sector); 	
    OpenFile* parentDirFile = new OpenFile(parentDir->sector);
    parentDir->WriteBack(parentDirFile);   
    freeMap->WriteBack(freeMapFile);

    delete hdr;
    delete freeMap;
    delete parentDirFile;

    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------
OpenFile *
FileSystem::Open(const char *path)
{ 
    char fileName[FileNameMaxLen];
    Directory* parentDir = resolvePath(path, fileName);
    if (parentDir == nullptr) {
        return NULL;
    }

    DEBUG('f', "Directory from sector %d\n", parentDir->sector);
    DEBUG('f', "Opening file %s\n", fileName);

    int sector = parentDir->Find(fileName);
    DEBUG('f', "File header sector is %d\n", sector);
    if (sector < 0)
        return NULL;

    if (openFileList->Size() >= MAX_OPEN_FILES)
        return NULL;

    OpenFile* openFile = new OpenFile(sector);
    OpenFileListEntry* newEntry = new OpenFileListEntry;
    newEntry->file = openFile;
    newEntry->sector = sector;
    DEBUG('f', "OpenFile pointer = %p\n", (void*)newEntry->file);

    openFileList->Append(newEntry);

    openFileList->Mapcar(PrintOpenFileEntry);

    return openFile;
}

void FileSystem::Close(const char* path) {
    char fileName[FileNameMaxLen];
    Directory* parentDir = resolvePath(path, fileName);
    if (parentDir == nullptr) {
        return;
    }
    int sector = parentDir->Find(fileName);
    ASSERT(sector >= 0);

    int fileIndex = openFileList->FindIf(MatchSector, &sector);
    ASSERT(fileIndex != -1);

    OpenFileListEntry* entry = (OpenFileListEntry*) openFileList->RemoveAt(fileIndex);
    delete entry->file;
    delete entry;
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(const char *path)
{ 
    char fileName[FileNameMaxLen];
    Directory* parentDir = resolvePath(path, fileName);
    if (parentDir == nullptr)
        return FALSE;

    DEBUG('f', "Removing file %s from its parent directory at sector %d\n", fileName, parentDir->sector);

    //Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    
    sector = parentDir->Find(fileName);
    if (sector == -1) {
       return FALSE;			 // file not found 
    }

    if (openFileList->FindIf(MatchSector, &sector) != -1) //if file is somewhere opened, there's a reason for it
        return FALSE;
  
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    parentDir->Remove(fileName);

    freeMap->WriteBack(freeMapFile);		// flush to disk

    OpenFile* parentDirFile = new OpenFile(parentDir->sector);
    parentDir->WriteBack(parentDirFile);   
    delete fileHdr;
    delete freeMap;
    delete parentDirFile;

    return TRUE;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List()
{
    Directory *directory = currentDirectory;
    directory->List();
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}

Directory* FileSystem::createRootDirectory(){
    Directory* dir = new Directory(NumDirEntries);

    // for now directoryFile is basically the rootDirectoryFile (does not exist explicitly in the system)
    ASSERT(directoryFile != NULL);
    dir->FetchFrom(directoryFile);
    dir->sector = DirectorySector;

    // "."
    dir->table[0].inUse = TRUE;
    dir->table[0].isDirectory = TRUE;
    strcpy(dir->table[0].name, ".");
    dir->table[0].sector = dir->sector;

    dir->WriteBack(directoryFile);

    return dir;
}

bool FileSystem::createDirectory(const char* path)
{
    DEBUG('f', "Starting to create directory %s\n", path);

    char dirName[FileNameMaxLen];
    Directory* parentDir = resolvePath(path, dirName);
    if (parentDir == nullptr)
        return FALSE;

    DEBUG('f', "Parent directory, sector %d\n", parentDir->sector);

    BitMap* freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    if (parentDir->Find(dirName) != -1) {
        delete freeMap;
        return FALSE;
    }

    int sector = freeMap->Find();
    if (sector == -1) {
        delete freeMap;
        return FALSE;
    }

    Directory* newDir = new Directory(NumDirEntries);

    newDir->sector = sector;

    // "."
    newDir->table[0].inUse = TRUE;
    newDir->table[0].isDirectory = TRUE;
    strcpy(newDir->table[0].name, ".");
    newDir->table[0].sector = sector;

    // ".."
    newDir->table[1].inUse = TRUE;
    newDir->table[1].isDirectory = TRUE;
    strcpy(newDir->table[1].name, "..");
    newDir->table[1].sector = parentDir->sector;

    DEBUG('f', "Creating directory %s, size %d\n", dirName, NumDirEntries);

    FileHeader* hdr = new FileHeader;
    bool success = hdr->Allocate(
        freeMap,
        DirectoryFileSize);

    if (!success || !(parentDir->Add(dirName, sector, true))) {
        delete hdr;
        delete freeMap;
        delete newDir;
        return FALSE;
    }

    hdr->WriteBack(sector);
    
    OpenFile* parentDirFile = new OpenFile(parentDir->sector);

    parentDir->WriteBack(parentDirFile);
    freeMap->WriteBack(freeMapFile);

    OpenFile* newDirFile = new OpenFile(sector);
    newDir->WriteBack(newDirFile);

    delete parentDirFile;
    delete newDirFile;
    delete hdr;
    delete freeMap;
    delete newDir;

    if (parentDir->sector == currentDirectory->sector) {
        currentDirectory->FetchFrom(directoryFile);
    }

    return TRUE;
}


bool FileSystem::changeDirectory(const char* path) {
    char dirName[FileNameMaxLen];
    int dirSector = getDirectorySector(path, dirName);
    if (dirSector == -1)
        return FALSE;
    Directory* dir = new Directory(NumDirEntries);
    dir->sector = dirSector;

    currentDirectory->WriteBack(directoryFile); 
    
    directoryFile = new OpenFile(dirSector);// this member file always stays open, like the freemap header file throughout the exec of Nachos.
    DEBUG('f', "Changing to dir residing in sector %d\n", dirSector);

    dir->FetchFrom(directoryFile);

    currentDirectory = dir;

    return TRUE;
}

bool FileSystem::deleteDirectory(const char* path) {
    DEBUG('f', "Starting to delete directory %s\n", path);

    char dirName[FileNameMaxLen];
    int dirSector = getDirectorySector(path, dirName);
    Directory* parentDir = resolvePath(path, dirName);
    if (parentDir == nullptr) {
        return FALSE;
    }
    if (dirSector == -1) {
        DEBUG('f', "Directory not found\n");
        return FALSE;
    }
    DEBUG('f', "Parent directory, sector %d\n", parentDir->sector);

    Directory* dir = new Directory(NumDirEntries);
    OpenFile* dirFile = new OpenFile(dirSector);

    OpenFile* parentDirFile = new OpenFile(parentDir->sector);

    dir->FetchFrom(dirFile);

    for (int k = 2; k < dir->tableSize; k++) { // from k = 2 because we don't care about .. and .
        if (dir->table[k].inUse == TRUE) {
            delete dirFile;
            delete dir;
            DEBUG('f', "Directory is not empty, cannot delete it\n");
            return FALSE;
        }
    }

    FileHeader* fileHdr = new FileHeader;
    fileHdr->FetchFrom(dirSector);

    BitMap* freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);
    freeMap->Clear(dirSector);
    
    parentDir->Remove(dirName);

    freeMap->WriteBack(freeMapFile);

    parentDir->WriteBack(parentDirFile);

    delete freeMap;
    delete fileHdr;
    delete dirFile;
    delete dir;

    if (parentDir->sector == currentDirectory->sector) {
        currentDirectory->FetchFrom(directoryFile);
    }

    return TRUE;
}

List_t* FileSystem::parsePath(char* path) {
    if (path == nullptr) {
        return new List_t();
    }

    List_t* result = new List_t();

    char* start = path;
    char* curr  = path;

    while (*curr != '\0') {
        if (*curr == PATH_DELIM) {
            if (curr > start) {
                size_t len = curr - start;
                char* token = new char[len + 1];
                strncpy(token, start, len);
                token[len] = '\0';

                result->Append((void*)token);
            }
            start = curr + 1;
        }
        curr++;
    }

    //last component
    if (curr > start) {
        size_t len = curr - start;
        char* token = new char[len + 1];
        strncpy(token, start, len);
        token[len] = '\0';

        result->Append((void*)token);
    }

    return result;
}

Directory* FileSystem::resolvePath(const char* path, char* lastName)
{
    List_t* parts = parsePath((char*)path);
    if (parts->IsEmpty()) return nullptr;

    Directory* currDir = new Directory(NumDirEntries);

    // absolute path 
    if (path[0] == PATH_DELIM) {
        currDir = rootDirectory;
    } 
    // Relative path 
    else {
        currDir = currentDirectory;
    }

    int n = parts->Size();

    // traverse all but last component
    for (int i = 0; i < n - 1; i++) {
        char* name = (char*)parts->GetAt(i);
        // skip '.'
        if (strcmp(name, ".") == 0) {
            continue;
        }
        int sector = currDir->Find(name);

        if (sector == -1) {
            //delete currDir;
            return nullptr;
        }

        Directory* nextDir = new Directory(NumDirEntries);
        OpenFile* f = new OpenFile(sector);
        nextDir->FetchFrom(f);
        nextDir->sector = sector;

        //delete f;
        //delete currDir;
        currDir = nextDir;
    }

    strcpy(lastName, (char*)parts->GetAt(n - 1));
    return currDir;  
}

int FileSystem::getDirectorySector(const char* path, char* dirName)
{
    List_t* parts = parsePath((char*)path);
    if (parts->IsEmpty()) return -1;

    Directory* currDir = new Directory(NumDirEntries);

    // absolute path ( start from the root)
    if (path[0] == PATH_DELIM) {
        currDir = rootDirectory;
    } 
    // relative path ( start from currentDirectory)
    else {
        currDir = currentDirectory;
    }

    int n = parts->Size();

    // traverse all components - if there is no error
    for (int i = 0; i < n; i++) {
        char* name = (char*)parts->GetAt(i);
        // just skipping '.'
        if (strcmp(name, ".") == 0) {
            continue;
        }
        int sector = currDir->FindDirectory(name);

        if (sector == -1) {
            //delete currDir;
            return -1;
        }

        Directory* nextDir = new Directory(NumDirEntries);
        OpenFile* f = new OpenFile(sector);
        nextDir->FetchFrom(f);
        nextDir->sector = sector;

        //delete f;
        //delete currDir;
        currDir = nextDir;
    }

    strcpy(dirName, (char*)parts->GetAt(n - 1));
    return currDir->sector;  
}


