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


// Parent and child agree on common key
#define SHMKEY  55861349

// Create the clock structure to attach later to shared memory
struct Clock {
    int seconds;
    int nanoSeconds;
};


// Creation of the Message Buffer Structure
typedef struct msgbuffer {
    long mtype;
    int intData;
} msgbuffer;



int main(int argc, char ** argv) {
    //declare variables for message queue
    msgbuffer buf;
    int msqid = 0; // messageQueueID
    key_t key;
    buf.mtype = 1;
    buf.intData;

    // Get a key for the message queue
    if ((key = ftok("msgq.txt", 1)) == -1){
        perror("worker.c: ftok error\n");
        exit(1);
    }

    // Creation of the message queue
    if ((msqid = msgget(key, 0666)) == -1) {
        perror("worker.c: error in msgget\n");
        exit(1);
    }


    // Check the number of commands, this should have everything from the checking in oss.c but it pays to double check
    if(argc !=  3) {
        printf("Usage: ./worker [Must be 2 arguments]\n");
        return EXIT_FAILURE;
    }


    //change argv[1] to an integer
    int inputSeconds = atoi(argv[1]);
    int inputNanoSeconds = atoi(argv[2]);

    int shmid = shmget(SHMKEY, sizeof(struct Clock), 0666);
    if (shmid == -1) {
        perror("worker.c: Error in shmget\n");
        exit(1);
    }

    struct Clock *clockPointer;
    // Attach to the shared memory segment
    clockPointer = (struct Clock *)shmat(shmid, 0, 0);
    if (clockPointer == (struct Clock *)-1) {
        perror("worker.c: Error in shmat\n");
        exit(1);
    }

    // Get termination time from stop times
    int sStopTime = clockPointer->seconds + inputSeconds;
    int nStopTime = clockPointer->nanoSeconds + inputNanoSeconds;


    // Startup print to console
    printf("Worker PID:%d PPID:%d Called with oss: TermTimeS: %d TermTimeNano: %d\n--Received message\n", getpid(), getppid(), sStopTime , nStopTime);


    int sStartTime = clockPointer->seconds;
    int sNanoTime = clockPointer->nanoSeconds;
    int copySecond = sStartTime;

    while(1) {
        // When message is received
        if ( msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) {
            perror("worker.c: failed to receive message from oss\n");
            exit(1);
        }

        if(clockPointer->seconds >= copySecond + 1) {
            printf("WORKER PID:%d PPID:%d SysClockS:%u SysClockNano:%u TermTimeS:%d TermTimeNano:%d\n--%d seconds have passed since starting\n", getpid(), getppid(), clockPointer->seconds, clockPointer->nanoSeconds, sStopTime, nStopTime, (int)clockPointer->seconds - sStartTime);
            copySecond = clockPointer->seconds;
        }

        buf.mtype = getppid();

        if(clockPointer->seconds >= sStopTime) {
            buf.intData = 0;
            //message send
            if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                perror("worker.c: msgsnd to oss failed\n");
                exit(1);
            }
            break;
        } else if (clockPointer->seconds == sStopTime && clockPointer->nanoSeconds >= nStopTime) {
            buf.intData = 0;
            // Send Message
            if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                perror("worker.c: msgsnd to oss failed\n");
                exit(1);
            }
            break;
        }

        buf.intData = 1;
        // Send Message
        if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
            perror("worker.c: msgsnd to oss failed\n");
            exit(1);
        }


    }

    // On termination, display this
    printf("WORKER PID:%d PPID:%d SysClockS:%u SysClockNano:%u TermTimeS:%d TermTimeNano:%d\n--Terminating\n", getpid(), getppid(), clockPointer->seconds, clockPointer->nanoSeconds, sStopTime, nStopTime);


    return EXIT_SUCCESS;
}