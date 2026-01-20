#ifndef RNETMANAGER_H
#define RNETMANAGER_H


/* RNetManager:
 *	kernel-side socket layer that sits on top of reliable post 
 *	this interface holds a table of handles per process. User programs cannot have direct memory access to kernel objects
 *	thus these objects are returned as integer descriptors of the objects in a table (much like we did with semaphores and how file
 *	descriptors work). 
 *
 */





#ifdef NETWORK

class Lock;
class ReliablePost;

#define MAX_RNET_HANDLES 16 // max 16 handles could be created at once

class RNetManager {
public:
    RNetManager(); // The constructor of the object which is tied to the process by its address space
    ~RNetManager();

    int Socket(int localMailbox); // open a new handle in the handle table tied to the local machine mail box and return the handle to the entry
    int Close(int h); // clear the handle (marks the table entry as unused) from the table and delete its objects 

    int SendFixed(int h, int dstM, int dstB, int userBuf, int len); // send fixed sized bytes to (dstM: dstMachine, distB: dstMailBox)
    int RecvFixed(int h, int userBuf, int cap, int userSrcM, int userSrcB); // receive bytes and write them to user buffer

    int SendVar(int h, int dstM, int dstB, int userBuf, int len); // Same as SendFixed, but it uses ReliablePost::SendVar that will fragment
								  // the message and wait for an ACK on each fragment

    // 2-phase var receive:
    // Begin blocks until a whole var message is available, stores it internally.
    // Read copies it out to user buffer.
    // End frees the stored message.
    // as an analogy:
    int RecvVarBegin(int h, int userOutLen, int userSrcM, int userSrcB); // more like openning a file
    int RecvVarRead(int h, int userBuf, int cap); // more like reading from the file
    int RecvVarEnd(int h); // cleaning up and closing the file

private:
    struct Handle {
        bool used; // is this table slot occupied
        int localMailbox; // which mailbox (port) is this handle tied to
        ReliablePost* rp; // the reliable post object related to this handle

        bool haveVar; // the kernel has a buffer of data that the user has not consumed yet. It enforces that the user call End so the kernel 
		      // can safely free the memory
        char* varData; // pointer to kernel buffer holding variable sized message
        int varLen; // size of the var data buffer
        int varSrcM; // the source machine of the data 
        int varSrcB; // the source mailbox of the data
    };

    Lock* lock;
    Handle table[MAX_RNET_HANDLES]; // Table of handles

    bool Valid(int h) const; // helper function that cheks if the handle is valid in the handles table
};

#endif // NETWORK
#endif // RNETMANAGER_H

