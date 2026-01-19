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
    int RecvVarBegin(int h, int userOutLen, int userSrcM, int userSrcB); 
    int RecvVarRead(int h, int userBuf, int cap);
    int RecvVarEnd(int h);

private:
    struct Handle {
        bool used;
        int localMailbox;
        ReliablePost* rp;

        bool haveVar;
        char* varData;
        int varLen;
        int varSrcM;
        int varSrcB;
    };

    Lock* lock;
    Handle table[MAX_RNET_HANDLES]; // Table of 

    bool Valid(int h) const;
};

#endif // NETWORK
#endif // RNETMANAGER_H

