// dirtest.cc
// Test routines for directory support in Nachos filesystem

#include "copyright.h"
#include "utility.h"
#include "filesys.h"
#include "system.h"
#include <stdio.h>

#define ROOT_FILE           "rootFile"
#define TEST_DIR            "./testDir"
#define TEST_DIR_CHILD_DIR  "./testDir/testDir1"
#define INNER_FILE          "innerFile"
#define CONTENTS            "HelloNachos"
#define CONTENT_SIZE 12

//----------------------------------------------------------------------
// CreateFile
//	Simple helper to create a file and write some content
//----------------------------------------------------------------------

// static void
// CreateFile(const char* name)
// {
//     OpenFile* openFile;

//     printf("Creating file: %s\n", name);

//     if (!fileSystem->Create(name, CONTENT_SIZE)) {
//         printf("Failed to create file %s\n", name);
//         return;
//     }

//     openFile = fileSystem->Open(name);
//     if (openFile == NULL) {
//         printf("Failed to open file %s\n", name);
//         return;
//     }

    openFile->Write(CONTENTS, CONTENT_SIZE);
    fileSystem->Close(name);
}

//----------------------------------------------------------------------
// DirectoryTest
//	Test directory creation, navigation, and deletion
//----------------------------------------------------------------------

void
DirectoryTest()
{
    printf("\n===== Starting Directory Test =====\n");

    // List root directory
    printf("\nListing root directory (initial):\n");
    fileSystem->List();

    // Create a directory in root
    printf("\nCreating directory '%s' in root\n", TEST_DIR);
    if (!fileSystem->createDirectory(TEST_DIR)) {
        printf("Failed to create directory %s\n", TEST_DIR);
        return;
    }

    // Create a child directory for the newly created testDir
    printf("\nCreating one child directory '%s'\n", TEST_DIR_CHILD_DIR);
    if (!fileSystem->createDirectory(TEST_DIR_CHILD_DIR)) {
        printf("Failed to create directory %s\n", TEST_DIR_CHILD_DIR);
        return;
    }

    // Create a file in root directory
    // CreateFile(ROOT_FILE);
    // char buf[CONTENT_SIZE];

    // OpenFile* file1 = fileSystem->Open(ROOT_FILE);
    // file1->Read(buf, CONTENT_SIZE);
    // printf("\nContent of file %s is %s\n", ROOT_FILE, buf);
    
    fileSystem->Close(ROOT_FILE);
    memset(buf, '\0', CONTENT_SIZE);


    // List root directory
    printf("\nListing root directory (after creations):\n");
    fileSystem->List();

    // Change into testDir
    printf("\nChanging into directory '%s'\n", TEST_DIR);
    if (!fileSystem->changeDirectory(TEST_DIR)) {
        printf("Failed to change directory to %s\n", TEST_DIR);
        return;
    }

    // List contents of testDir
    printf("\nListing contents of '%s':\n", TEST_DIR);
    fileSystem->List();

    // Create a file inside testDir
    // CreateFile(INNER_FILE);

    // OpenFile* file2 = fileSystem->Open(INNER_FILE);

    // file2->Read(buf, CONTENT_SIZE);
    
    // printf("\nContent of file %s is %s\n", INNER_FILE, buf);

    fileSystem->Close(INNER_FILE);
    memset(buf, '\0', CONTENT_SIZE);

    // List contents again
    printf("\nListing contents of '%s' after file creation:\n", TEST_DIR);
    fileSystem->List();


        // ------------------------------------------------------------
    // Remove file inside TEST_DIR
    // ------------------------------------------------------------
    // printf("\nRemoving file '%s' inside '%s'\n", INNER_FILE, TEST_DIR);
    // if (!fileSystem->Remove(INNER_FILE)) {
    //     printf("Failed to remove file %s\n", INNER_FILE);
    //     return;
    // }

    // List TEST_DIR after removal
    printf("\nListing contents of '%s' after removing file:\n", TEST_DIR);
    fileSystem->List();

    // ------------------------------------------------------------
    // Change back to root directory
    // ------------------------------------------------------------
    printf("\nChanging back to root directory\n");
    if (!fileSystem->changeDirectory("..")) {
        printf("Failed to return to root directory\n");
        return;
    }

    // ------------------------------------------------------------
    // Delete TEST_DIR
    // ------------------------------------------------------------
    printf("\nDeleting directory '%s'\n", TEST_DIR);
    if (!fileSystem->deleteDirectory(TEST_DIR)) {
        printf("Failed to delete directory %s\n", TEST_DIR);
        return;
    }

    // ------------------------------------------------------------
    // Final state of root directory
    // ------------------------------------------------------------
    printf("\nListing root directory after deleting '%s':\n", TEST_DIR);
    fileSystem->List();

    printf("\n===== Directory Test Complete =====\n");
}
