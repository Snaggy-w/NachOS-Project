// nettest.cc 
//	Test out message delivery between two "Nachos" machines,
//	using the Post Office to coordinate delivery.
//
//	Two caveats:
//	  1. Two copies of Nachos must be running, with machine ID's 0 and 1:
//		./nachos -m 0 -o 1 &
//		./nachos -m 1 -o 0 &
//
//	  2. You need an implementation of condition variables,
//	     which is *not* provided as part of the baseline threads 
//	     implementation.  The Post Office won't work without
//	     a correct implementation of condition variables.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "network.h"
#include "post.h"
#include "interrupt.h"
#include "reliable.h"
// Test out message delivery, by doing the following:
//	1. send a message to the machine with ID "farAddr", at mail box #0
//	2. wait for the other machine's message to arrive (in our mailbox #0)
//	3. send an acknowledgment for the other machine's message
//	4. wait for an acknowledgement from the other machine to our 
//	    original message
void
MailTest(int farAddr)
{
    PacketHeader outPktHdr, inPktHdr;
    MailHeader outMailHdr, inMailHdr;
    const char *data = "Hello there!";
    const char *ack = "Got it!";
    char buffer[MaxMailSize];

    int mess=0;

    while (mess <10)
    {   
            // construct packet, mail header for original message
        // To: destination machine, mailbox 0
        // From: our machine, reply to: mailbox 1
        outPktHdr.to = farAddr;	
        	
        outMailHdr.to = 0;
        outMailHdr.from = 1;
        outMailHdr.length = strlen(data) + 1;
        // Send the first message
        postOffice->Send(outPktHdr, outMailHdr, data); 

        // Wait for the first message from the other machine
        postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);
        printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
        fflush(stdout);

        // Send acknowledgement to the other machine (using "reply to" mailbox
        // in the message that just arrived
        outPktHdr.to = inPktHdr.from;
        outMailHdr.to = inMailHdr.from;
        outMailHdr.length = strlen(ack) + 1;
        postOffice->Send(outPktHdr, outMailHdr, ack); 

        // Wait for the ack from the other machine to the first message we sent.
        postOffice->Receive(1, &inPktHdr, &inMailHdr, buffer);
        printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
        fflush(stdout);

        mess++;
    }

    // Then we're done!
    interrupt->Halt();
}


void
RingTest(int nbMachine)
{
    PacketHeader outPktHdr, inPktHdr;
    MailHeader outMailHdr, inMailHdr;
    const char *data = "Hello there!";
    // const char *ack = "Got it!";
    char buffer[MaxMailSize];

    // construct packet, mail header for original message
    // To: destination machine, mailbox 0
    // From: our machine, reply to: mailbox 1

    // sends to machine + 1 or to machine 0 if it's the last machine
    if(postOffice->GetAddress() < nbMachine - 1){
        outPktHdr.to = postOffice->GetAddress() + 1;
    }else if(postOffice->GetAddress() == nbMachine - 1){
        outPktHdr.to = 0;
    }	
    
    outMailHdr.to = 0;
    outMailHdr.from = 1;
    outMailHdr.length = strlen(data) + 1;

    // Send the first message
    postOffice->Send(outPktHdr, outMailHdr, data); 

    // Wait for the first message from the other machine
    postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);
    printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
    fflush(stdout);

    // Then we're done!
    interrupt->Halt();
}


static void FillPattern(char *buf, int n, int seed) {
    for (int i = 0; i < n; i++) buf[i] = (char)((i + seed) % 251);
}

void ReliableTest(int farAddr) {
    const int N = 10;

    // ---- simple startup handshake ----
    // Machine 1 announces READY.
    // Machine 0 waits for READY before sending anything.
    if (postOffice->GetAddress() == 1) {
        const char *ready = "READY";
        bool ok = reliablePost->SendFixed(farAddr, 0, ready, strlen(ready) + 1);
        if (!ok) {
            printf("[ReliableTest] failed to send READY\n");
            fflush(stdout);
            // Don't halt here; just return so the other side doesn't explode.
            return;
        }
    } else if (postOffice->GetAddress() == 0) {
        char buf[64];
        int sm, sb;
        reliablePost->ReceiveFixed(buf, sizeof(buf), &sm, &sb);
        printf("[ReliableTest] startup sync: got \"%s\" from %d,%d\n", buf, sm, sb);
        fflush(stdout);
    }

    // ---- Part II: fixed-size ping/pong ----
    if (postOffice->GetAddress() == 0) {
        for (int i = 0; i < N; i++) {
            char msg[64];
            sprintf(msg, "FIXED ping %d from %d", i, postOffice->GetAddress());

            bool ok = reliablePost->SendFixed(farAddr, 0, msg, strlen(msg) + 1);
            if (!ok) {
                printf("[ReliableTest] SendFixed failed at i=%d (continuing)\n", i);
                fflush(stdout);
                return; // return instead of Halt to avoid crashing peer
            }

            char reply[128];
            int srcM, srcB;
            int got = reliablePost->ReceiveFixed(reply, sizeof(reply), &srcM, &srcB);
            printf("[ReliableTest] Got fixed reply (%d bytes) from %d,%d: \"%s\"\n",
                   got, srcM, srcB, reply);
            fflush(stdout);
        }

        // ---- Part III: variable-size send + verify ack ----
        int bigLen = (MaxMailSize * 5) + 123;
        char *big = new char[bigLen];
        FillPattern(big, bigLen, 7);

        printf("[ReliableTest] Sending var message of %d bytes...\n", bigLen);
        fflush(stdout);

        bool ok2 = reliablePost->SendVar(farAddr, 0, big, bigLen);
        delete [] big;

        if (!ok2) {
            printf("[ReliableTest] SendVar failed (continuing)\n");
            fflush(stdout);
            return;
        }

        char ack[64];
        int srcM2, srcB2;
        reliablePost->ReceiveFixed(ack, sizeof(ack), &srcM2, &srcB2);
        printf("[ReliableTest] Got var ACK from %d,%d: \"%s\"\n", srcM2, srcB2, ack);
        fflush(stdout);

        printf("[ReliableTest] Machine 0 done.\n");
        fflush(stdout);
        interrupt->Halt();

    } else {
        // Machine 1: receive fixed pings and reply
        for (int i = 0; i < N; i++) {
            char in[128];
            int srcM, srcB;
            reliablePost->ReceiveFixed(in, sizeof(in), &srcM, &srcB);

            printf("[ReliableTest] Got fixed msg from %d,%d: \"%s\"\n", srcM, srcB, in);
            fflush(stdout);

            char reply[64];
            sprintf(reply, "FIXED pong %d from %d", i, postOffice->GetAddress());

            bool ok = reliablePost->SendFixed(srcM, 0, reply, strlen(reply) + 1);
            if (!ok) {
                printf("[ReliableTest] Reply SendFixed failed at i=%d\n", i);
                fflush(stdout);
                return;
            }
        }

        // Receive var message and verify content
        char *data = 0;
        int len = 0;
        int srcM3, srcB3;
        reliablePost->ReceiveVar(&data, &len, &srcM3, &srcB3);

        printf("[ReliableTest] Got var message len=%d from %d,%d. Verifying...\n",
               len, srcM3, srcB3);
        fflush(stdout);

        bool good = true;
        for (int i = 0; i < len; i++) {
            char expected = (char)((i + 7) % 251);
            if (data[i] != expected) {
                printf("[ReliableTest] Var verify FAILED at i=%d expected=%d got=%d\n",
                       i, (int)(unsigned char)expected, (int)(unsigned char)data[i]);
                good = false;
                break;
            }
        }
        delete [] data;

        if (!good) return;

        const char *okmsg = "VAR OK";
        reliablePost->SendFixed(srcM3, 0, okmsg, strlen(okmsg) + 1);

        printf("[ReliableTest] Machine 1 done.\n");
        fflush(stdout);
        interrupt->Halt();
    }
}

