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


// Define Shared Memory Key
#define SHMKEY  55861349


// Define one second and incrementation variable
#define incrementNano 2500000
#define oneSecond 1000000000
int shmid, msqid;

// Setup internal clock
struct Clock {
    int seconds;
    int nanoSeconds;
};

struct Clock *clockPointer;


// create process table
struct PCB {
    int occupied;           // either true or false
    pid_t pid;              // process id of this child
    int startSeconds;       // time when it was forked
    int startNano;          // time when it was forked
};

// define message buffer
typedef struct msgbuffer {
    long mtype;
    int intData;
} msgbuffer;


// help function -------------------------------------------------------------------------------
void print_usage(){
    printf("Usage for OSS: -n <n_value> -s <s_value> -t <t_value> -i <i_value> -f <fileName>\n");
    printf("Options:\n");
    printf("-n: stands for the total number of workers to launch\n");
    printf("-s: Defines how many workers are allowed to run simultaneously\n");
    printf("-t: The time limit to pass to the workers\n");
    printf("-i: How often a worker should be launched (in milliseconds)\n");
    printf("-f: Name of the arg_f the user wishes to use\n");
}


// Generate random numbers with this function
void generateRandomTime(int maxSeconds, int *seconds, int *nanoseconds) {
    srand(time(NULL));

    //Generate random seconds between 1 and T-Argument
    *seconds = (rand() % maxSeconds) + 1;

    //Generate random nanoseconds
    *nanoseconds = rand() % oneSecond;
}


// Function to increment the Clock
void incrementClock(struct Clock* clockPointer) {
    clockPointer->nanoSeconds += incrementNano;

    // Check if nanoseconds have reached 1 second yet, reset Nanoseconds when they do
    if (clockPointer->nanoSeconds >= oneSecond) {
        clockPointer->seconds++;
        clockPointer->nanoSeconds = 0;
    }
}






// function for displaying the process table
void procTableDisplay(struct Clock* clockPointer, struct PCB* procTable, int arg_n){
    printf("OSS PID: %d  SysClockS: %d  SysClockNano: %d\n", getpid(), clockPointer->seconds, clockPointer->nanoSeconds);
    printf("Process Table: \n");
    printf("Entry Occupied  PID   StartS   StartN\n");

    for(int i=0; i < arg_n; i++){
        printf("%d\t %d\t%d\t%d\t%d\n", i, procTable[i].occupied, procTable[i].pid, procTable[i].startSeconds, procTable[i].startNano);
    }
}


bool timeout = false;
// function for signal handle to change timeout---------------------------------------------
void alarmSignalHandler(int signum) {
    printf("Been 60 seconds time to terminate\n");
    timeout = true;
}


// function for ctrl-c signal handler--------------------------------------------------------
void controlHandler(int signum) {
    printf("\nYou hit Ctrl-C. Time to Terminate\n");
    timeout = true;
}


//fucntion to handle logging when message is recieved and message is sent----------------------
void logMessage(const char* arg_f, const char* message) {
    FILE* filePointer = fopen(arg_f, "a"); //open arg_f in append mode
    if (filePointer != NULL) {
        fprintf(filePointer, "%s", message);
        fclose(filePointer);
    } else {
        perror("oss.c: Error opening arg_f\n");
        exit(1);
    }
}


// main function--------------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Declare variables
    signal(SIGALRM, alarmSignalHandler);
    signal(SIGINT, controlHandler);

    alarm(60);

    int arg_n, arg_s, arg_i, arg_t, opt;
    int randomSeconds, randomNanoSeconds;
    char* arg_f;


    // get opt to get command line arguments
    while((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
        switch(opt) {
            case 'h':
                print_usage();
                break;
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
                arg_i = atoi(optarg);
                arg_i *= 100000000;
                break;
            case 'f':
                arg_f = optarg;
                break;
            case '?':
                print_usage();
                return EXIT_FAILURE;
            default:
                break;
        }
    }

    // Check if all argument were provided for use
    if (arg_n <= 0 || arg_s <= 0 || arg_t <= 0 || arg_i <= 0 || arg_f == NULL) {
        printf("All arguments are required\n");
        print_usage();

        return(EXIT_FAILURE);
    }

    //create array of structs for process table with size = number of children
    struct PCB processTable[arg_n];

    //Initalize the process table information for each process to 0
    for(int i = 0; i < arg_n; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = 0;
        processTable[i].startSeconds = 0;
        processTable[i].startNano = 0;
    }

    //Allocate memory for the simulated clock
    shmid = shmget(SHMKEY, sizeof(struct Clock), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("oss.c: Error in shmget");
        exit(1);
    }

    //Attach to the shared memory segment
    clockPointer = (struct Clock *)shmat(shmid, 0, 0);
    if (clockPointer == (struct Clock *)-1) {
        perror("oss.c: Error in shmat");
        exit(1);
    }

    //Initialize the simulated clock to zero
    clockPointer->seconds = 0;
    clockPointer->nanoSeconds = 0;

    //set up message queue and arg_f
    msgbuffer buf;
    key_t key;
    system("touch msgq.txt");

    // get a key for our message queues
    if ((key = ftok("msgq.txt", 1)) == -1) {
        perror("oss.c: ftok error\n");
        exit(1);
    }

    //create our message queue
    if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("oss.c: error in msgget\n");
        exit(1);
    }
    printf("oss.c: message queue is set up\n");


    for(int i=0; i < arg_s; i++) {
        pid_t childPid = fork();

        if (childPid == 0) {
            // Generate the random numbers for the clock
            generateRandomTime(arg_t, &randomSeconds, &randomNanoSeconds);

            // Change the ints to strings
            char randomSecondsBuffer[20], randomNanoSecondsBuffer[20];
            sprintf(randomSecondsBuffer, "%d", randomSeconds);
            sprintf(randomNanoSecondsBuffer, "%d", randomNanoSeconds);

            // Char array to hold information for exec call
            char* args[] = {"./worker", randomSecondsBuffer, randomNanoSecondsBuffer, 0};

            // Execute the worker file with given arguments
            execvp(args[0], args);
        } else {
            // New child was launched, update process table
            for(int i = 0; i < arg_n; i++) {
                if (processTable[i].pid == 0) {
                    processTable[i].occupied = 1;
                    processTable[i].pid = childPid;
                    processTable[i].startSeconds = clockPointer->seconds;
                    processTable[i].startNano = clockPointer->nanoSeconds;
                    break;
                }
            }
        }
    }


    int workers = arg_s; //active number of workers
    int workerNum = 0;
    int termWorker = 0;
    int launchTimerS = clockPointer ->seconds;
    int launchTimerN = clockPointer ->nanoSeconds + arg_i;
    if (launchTimerN >= oneSecond) {
        launchTimerS++;
        launchTimerN = 0;
    }
    while(!timeout) {
        incrementClock(clockPointer);

        struct PCB childP = processTable[workerNum];
        int cPid = childP.pid;

        if(cPid != 0 && childP.occupied == 1) {
            buf.mtype = cPid;

            //message send to each launched worker
            if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                perror("oss.c: msgsnd to worker failed");
                exit(1);
            } else {
                // Change the ints to strings
                char newBuffer[20], buffer1[20], buffer2[20], buffer3[20];
                sprintf(newBuffer, "%d", workerNum);
                sprintf(buffer1, "%d", cPid);
                sprintf(buffer2, "%d", clockPointer->seconds);
                sprintf(buffer3, "%d", clockPointer->nanoSeconds);

                char message[256];
                sprintf(message, "OSS: Sending message to worker %s PID %s at time %s:%s\n", newBuffer, buffer1, buffer2, buffer3);
                printf("%s\n", message);
                logMessage(arg_f, message);
            }

            //message recieve
            msgbuffer rcvbuf;
            if (msgrcv(msqid, &rcvbuf, sizeof(msgbuffer), getpid(), 0) == -1) {
                perror("oss.c: Failed to recieve message\n");
                exit(1);
            } else {
                // Change the ints to strings
                char newBuffer1[20], buffer4[20], buffer5[20], buffer6[20];
                sprintf(newBuffer1, "%d", workerNum);
                sprintf(buffer4, "%d", cPid);
                sprintf(buffer5, "%d", clockPointer->seconds);
                sprintf(buffer6, "%d", clockPointer->nanoSeconds);

                char message2[256];
                sprintf(message2, "OSS: Recieving message from worker %s PID %s at time %s:%s\n", newBuffer1, buffer4, buffer5, buffer6);
                printf("%s\n", message2);
                logMessage(arg_f, message2);
            }

        }

        //see if any workers have terminated
        int status;
        int terminatingPid = waitpid(-1, &status, WNOHANG);

        if (terminatingPid != 0) {
            termWorker++;

            for(int i=0; i < arg_n; i++) {
                if(processTable[i].pid == terminatingPid) {
                    processTable[i].occupied = 0;
                    break;
                }
            }

            if(workers < arg_n && clockPointer->seconds >= launchTimerS) {

                // update launch timer
                int launchTimerN = clockPointer ->nanoSeconds + arg_i;
                if (launchTimerN >= oneSecond) {
                    launchTimerS++;
                    launchTimerN = 0;
                }



                //increment the number of active workers
                workers++;
                //fork to worker
                pid_t childPid = fork();


                if (childPid == 0) {
                    // Generate the random numbers for the clock
                    generateRandomTime(arg_t, &randomSeconds, &randomNanoSeconds);

                    // Change the ints to strings
                    char randomSecondsBuffer[20], randomNanoSecondsBuffer[20];
                    sprintf(randomSecondsBuffer, "%d", randomSeconds);
                    sprintf(randomNanoSecondsBuffer, "%d", randomNanoSeconds);

                    // Char array to hold information for exec call
                    char* args[] = {"./worker", randomSecondsBuffer, randomNanoSecondsBuffer, 0};

                    // Execute the worker file with given arguments
                    execvp(args[0], args);
                } else {
                    // New child was launched, update process table
                    for(int i = 0; i < arg_n; i++) {
                        if (processTable[i].pid == 0) {
                            processTable[i].occupied = 1;
                            processTable[i].pid = childPid;
                            processTable[i].startSeconds = clockPointer->seconds;
                            processTable[i].startNano = clockPointer->nanoSeconds;
                            break;
                        }
                    }
                }
            }
        }

        //display process table every half second
        if ((clockPointer->nanoSeconds % (int)(oneSecond / 2)) == 0) {
            procTableDisplay(clockPointer, processTable, arg_n);

            if(termWorker >= arg_n) {
                break;
            }
        }

        //increment worker number
        workerNum++;
        if(workerNum >= arg_n) {
            workerNum = 0;
        }

    }

    // do clean up
    for(int i=0; i < arg_n; i++) {
        if(processTable[i].occupied == 1) {
            kill(processTable[i].pid, SIGKILL);
        }
    }


    // get rid of message queue
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("oss.c: msgctl to get rid of queue, failed\n");
        exit(1);
    }

    //detach from shared memory
    shmdt(clockPointer);

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("oss.c: shmctl to get rid or shared memory, failed\n");
        exit(1);
    }

    system("rm msgq.txt");


    return EXIT_SUCCESS;

}