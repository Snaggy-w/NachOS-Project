#ifndef RNETMANAGER_H
#define RNETMANAGER_H

#ifdef NETWORK

class Lock;
class ReliablePost;

#define MAX_RNET_HANDLES 16

class RNetManager {
public:
    RNetManager();
    ~RNetManager();

    int Socket(int localMailbox);
    int Close(int h);

    int SendFixed(int h, int dstM, int dstB, int userBuf, int len);
    int RecvFixed(int h, int userBuf, int cap, int userSrcM, int userSrcB);

    int SendVar(int h, int dstM, int dstB, int userBuf, int len);

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
    Handle table[MAX_RNET_HANDLES];

    bool Valid(int h) const;
};

#endif // NETWORK
#endif // RNETMANAGER_H

