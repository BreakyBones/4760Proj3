// Kanaan Sullivan Project 3 for Operating Systems worker.c

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>

// Shared Memory Key

#define SHMKEY 2563849

// Structure of a clock
struct Clock {
    int seconds;
    int nanoSeconds;
};

// Structure for the message buffer
typedef struct msgbuffer {
    long mtype;
    int intData;
};

int main(int argc, char **argv) {
    struct msgbuffer msg;
    int msqid = 0; // Msq Queue ID
    key_t key;
    msg.mtype = 1;
    msg.intData;


    // Genereate Message Queue SHM Key
    if ((key = ftok("msgq.txt" , 1)) == -1) {
        perror("Worker.c: Error in generating message queue key\n");
        exit(1);
    }

// Create Message Queue
    if ((msqid = msgget(key , 0666)) == -1) {
        perror("worker.c: error in creating message queue\n");
        end(1);
    }



}

