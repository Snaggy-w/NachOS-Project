#ifndef MIGRATIONMANAGER_H
#define MIGRATIONMANAGER_H

#ifdef NETWORK


// forward declarations to avoid cyclic dependencies
class ReliablePost;
class Thread;
class Lock;

/*
 * MigrationManager
 *   Listens on a dedicated ReliablePost (mailbox 1 in your system.cc)
 *   and receives migration-related control messages
 */
class MigrationManager {
public:
    explicit MigrationManager(ReliablePost* rp); // constructor with the reliable post object to use
    ~MigrationManager(); // destructor

    // initiate the migration of the process to the target machine
    // returns 0 on "accepted", -1 on failure
    int RequestMigrate(int targetMachine);

private:
    ReliablePost* rp;     // pointer to the global reliable post object
    Thread* rxThread;     // the receiver thread running
    Lock* lock;           // a lock to protect internal data structures
    bool shuttingDown;

    // thread entry trampoline to execute the self declared loop function
    static void RxTrampoline(int arg);
    void RxLoop();

    // to send a wake on waiting thread for cleanup
    void SendWakeupToSelf();
};

#endif // NETWORK
#endif // MIGRATIONMANAGER_H

