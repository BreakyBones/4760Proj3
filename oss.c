#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <stdbool.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>

#define SHMKEY 2563849

int shmid, msqid;

// Create the system clock
struct Clock {
    int seconds;
    int nanoseconds;
};

struct Clock *clockPointer;

// Define the PCB Table as per instructions
struct PCB {
    int occupied; // Whether the current slot is occupied
    pid_t pid; // Process ID of the Child
    int startSeconds; // Time in seconds of fork
    int startNano; // Time in nanoseconds of fork
};

// define the message buffer
typedef struct msgbuffer {
    long mtype; // Stores the type of message and sends it to process
    int intData;
} msgbuffer;

void print_usage(const char *progName) {
    printf("Usage for %s: -n <n_value> -s <s_value> -t <t_value> -i <i_value>\n" , progName);
    printf("Options:\n");
    printf("-n: stands for the total number of workers to launch\n");
    printf("-s: Defines how many workers are allowed to run simultaneously\n");
    printf("-t: The time limit to pass to the workers\n");
    printf("-i: How often a worker should be launched (in milliseconds)\n");
    printf("-f: The name of the Logfile to pass OSS output to\n");
}

void generateTime(int maxSeconds, int* seconds, int *nanoseconds) {
    srand(time(NULL));

    // Random seconds between 1 and limit
    *seconds = (rand() % maxSeconds) + 1;

    // Random Nanoseconds
    *nanoseconds = rand() % 1000000000;
};

// Increment Clock
void IncrementClock(struct Clock* clockPointer) {
    clockPointer->nanoseconds += 100000000;

    if (clockPointer->nanoseconds >= 1000000000) {
        clockPointer->seconds++;
        clockPointer->nanoseconds = 0;
    }
}

void PCBDisplay(struct Clock* clockPointer, struct PCB* procTable, int proc) {
    printf("OSS PID: %d SysClockS: %d SysClockNano: %d\n" , getpid(), clockPointer->seconds, clockPointer->nanoseconds);
    printf("Process Table: \n");
    printf("Entry Occupied PID StartS StartN\n");

    for(int i=0; i < proc; i++) {
        printf("%d\t%d\t%d\t%d\t%d\n" , i , procTable[i].occupied , procTable[i].pid , procTable[i].startSeconds , procTable[i].startNano);
    }
}

// Set up the failsafe shutdown
static void myHandler(int s) {
    printf("Got signal %d, terminate!\n" , s);
    exit(1);
}

// Set up myHandler
static int setupinterrupt (void) {
    struct sigaction act;
    act.sa_handler = myHandler;
    act.sa_flags = 0;

    return(sigemptyset(&act.sa_mask) || sigaction(SIGINT , &act, NULL) || sigaction(SIGPROF , &act , NULL));
}

static int setupitimer(void) { /* set ITIMER_PROF for 60-second intervals */
    struct itimerval value;
    value.it_interval.tv_sec = 60;
    value.it_interval.tv_usec = 0;
    value.it_value = value.it_interval;
    return (setitimer(ITIMER_PROF, &value, NULL));
}

void log(const char* logFile, const char* message) {
    FILE* filePointer = fopen(logFile, "a");
    if(filePointer != NULL) {
        fprintf(filePointer, "%s", message);
        fclose(filePointer);
    } else {
        perror("oss.c: Error opening logFile\n");
        exit(1);
    }
}

int main(int argc, char** argv) {
    // set up variables for use
    char opt;
    const char optString[] ="hn:s:t:i:f:";

    int randomSeconds, randomNano;
    int arg_n , arg_s , arg_t , arg_i;
    char* arg_f;



    // Set up Interrupts
    if (setupinterrupt() == -1) {
        perror("Failed to set up handler for SIGPROF");
        return 1;
    }
    if (setupitimer() == -1) {
        perror("Failed to set up ITIMER_PROF interval timer");
        return 1;
    }

    while((opt = getopt(argc , argv, optString)) != -1) {
        switch(opt) {
            case 'h':
                print_usage(argv[0]);
                return(EXIT_SUCCESS);
            case 'n':
                arg_n = atoi(optarg);
                break;
            case 's':
                arg_s = atoi(optarg);
                break;
            case 't':
                arg_t = atoi(optarg);
                break;
            case 'i':
                arg_t = atoi(optarg);
                break;
            case 'f':
                arg_f = optarg;
                break;
            case '?':
                print_usage(argv[0]);
                break;
            default:
                printf("Invalid option %c\n" , optopt);
                print_usage(argv[0]);
                return (EXIT_FAILURE);
        }
    }

    // Check if all argument were provided for use
    if (arg_n <= 0 || arg_s <= 0 || arg_t <= 0 || arg_i <= 0 || arg_f == NULL) {
        printf("All arguments are required\n");
        print_usage(argv[0]);

        return(EXIT_FAILURE);
    }

    // Keep the iterator low to prevent confusion and time lag on OpSyS server
    if (arg_t > 10) {
        printf("Please keep your time limit for workers between 0 and 10 seconds to reduce time strain");

        return (EXIT_FAILURE);
    }
    // Keep the number of simultaneous processes low to reduce lag on OpSys server
    if (arg_s > 20) {
        printf("Please keep the simultaneous number of processes below 20");

        return(EXIT_FAILURE);
    }


    // create array of PCBs for the process table
    struct PCB processTable[arg_n];

    // Initialize the process table
    for(int i = 0; i < arg_n; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = 0;
        processTable[i].startSeconds = 0;
        processTable[i].startNano = 0;
    }

    // Allocate Shared Memory for Simulated System Clock
    shmid = shmget(SHMKEY, sizeof(struct Clock), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("oss.c: Error in creating shared memory ID");
        exit(1);
    }

    // Attach to shared memory
    clockPointer = (struct Clock*)shmat(shmid, 0, 0);
    if (clockPointer == (struct Clock*)-1) {
        perror("oss.c: Error in shmat");
        exit(1);
    }

    clockPointer->seconds = 0;
    clockPointer->nanoseconds = 0;

    msgbuffer msg;
    key_t key;
    system("touching msgq.txt");

    // key for message queue
    if ((key = ftok("msgq.txt" , 1)) == -1) {
        perror("oss.c: ftok error\n");
        exit(1);
    }

    // message queue creation
    if ((msqid = msgget(key , 0666 | IPC_CREAT)) == -1) {
        perror("oss.c error in msgget\n");
        exit(1);
    }

    // initialize worker variables
    int activeWorkers = 0; // active workers
    int endedWorkers = 0; // number of workers ended
    int timeout = 1; // Loop variable

    arg_i *= 1000000;
    int launchTimeS;
    int launchTimeN = clockPointer->nanoseconds + arg_i;
    if (clockPointer->nanoseconds >= 1000000000) {
        launchTimeS = clockPointer->seconds + 1;
        launchTimeN -= 1000000000;
    }


    while(!timeout) {
        IncrementClock(clockPointer);

        if(activeWorkers < arg_s && clockPointer->seconds >= launchTimeS && clockPointer->nanoseconds >= launchTimeN) {
            launchTimeN = clockPointer->nanoseconds + arg_i;
            if (clockPointer->nanoseconds >= 1000000000) {
                launchTimeS = clockPointer->seconds + 1;
                launchTimeN -= 1000000000;
            }

            pid_t workPid = fork();
            if (workPid == 0) {

            }
        }
    }
}