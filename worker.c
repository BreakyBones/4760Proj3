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
} msgbuffer;

int main(int argc, char **argv) {
    msgbuffer msg;
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
        exit(1);
    }

    int lengthSeconds = atoi(argv[1]);
    int lengthNano = atoi(argv[2]);

    int shmid = shmget(SHMKEY, sizeof(struct Clock), 0666);
    if (shmid == -1) {
        perror("worker.c: Error in shmget\n");
        exit(1);
    }
    struct Clock* clockPointer;
    clockPointer = (struct Clock*)shmat(shmid, 0 ,0);
    if (clockPointer == (struct Clock*)-1) {
        perror("worker.c: Error in shmat\n");
        exit(1);
    }

    int StopTimeS = clockPointer->seconds + lengthSeconds;
    int StopTimeN = clockPointer->nanoSeconds + lengthNano;
    int secondTracker = clockPointer->seconds;
    printf("Worker PID:%d PPID:%d Called with oss: TermTimeS: %d TermTimeNano: %d\n--Received message\n", getpid() , getppid(), StopTimeS , StopTimeN);

    while(1) {
        if (msgrcv(msqid, &msg, sizeof(msgbuffer), getpid(), 0) == -1) {
            perror("worker.c: failed to recieve message from oss\n");
            exit(1);
        }

        if(clockPointer->seconds >= secondTracker + 1) {
            printf("WORKER PID:%d SysClock:%u SysClockNano:%u TermTime:%d TermTimeNano%d\n--%d seconds have passed since starting\n" , getpid(), getppid(), clockPointer->seconds, clockPointer->nanoSeconds, StopTimeN, ((int)clockPointer->seconds - StopTimeS));
            secondTracker = clockPointer->seconds;
        }

        msg.mtype = getppid();

        if(clockPointer->seconds >= StopTimeS) {
            msg.mtype = 0;
            if (msgsnd(msqid , &msg, sizeof(msgbuffer)-sizeof(long) , 0) == -1) {
                perror("worker.c: msgsnd to oss failed\n");
                exit(1);
            }
            break;
        } else if (clockPointer->seconds == StopTimeS && clockPointer->nanoSeconds >= StopTimeN) {
            msg.intData = 0;

            if (msgsnd(msqid , &msg, sizeof(msgbuffer)-sizeof(long) , 0) == -1) {
                perror("worker.c: msgsnd to oss failed\n");
                exit(1);
            }
            break;
        }

        msg.intData = 1;
        if (msgsnd(msqid, &msg, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
            perror("worker.c msgsnd to oss failed\n");
            exit(1);
        }
    }

    printf("WORKER PID%d PPID%d SysClockS:%u SysClockNano:%u TermTimeS:%d TermTimeNano:%d\n--Terminating\n" , getpid(), getppid(), clockPointer->seconds, clockPointer->nanoSeconds, StopTimeS, StopTimeN);

    return(EXIT_SUCCESS);

}

