#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <signal.h>

#define PROB 0.1

struct sembuf waitop = {0,-1,0};
struct sembuf signalop = {0,1,0};

int msgid1,msgid2,msgid3,shmid1,shmid2,semid;//ids of ipc entities
int pidscheduler;//scheduler pid
int pidmmu;//mmu pid

typedef struct SM
{
    long pid;         // process id
    int mi;          // number of required pages
    int pagetable[25][3]; // page table
    int totalpagefaults;//total page faults by a process
    int totalillegalaccess;//total illegal faults by a process. Can be either 0 or 1.
} SM;

SM *sm;//Shared memory for info about all the processes
int *sm2;//Shared memory for free frames

void sighand(int signum)
{
    if(signum == SIGINT)
    {
        // kill scheduler,mmu and all the processes
        kill(pidscheduler,SIGINT);
        kill(pidmmu,SIGINT);
        shmdt(sm);
        shmdt(sm2);
        shmctl(shmid1, IPC_RMID, NULL);
        shmctl(shmid2, IPC_RMID, NULL);

        semctl(semid, 0, IPC_RMID, 0);

        msgctl(msgid1, IPC_RMID, NULL);
        msgctl(msgid2, IPC_RMID, NULL);
        msgctl(msgid3, IPC_RMID, NULL);
        exit(1);
    }
}

int main(){
    signal(SIGINT,sighand);
    srand(time(NULL));


    int k;
    printf("Enter the number of processes: ");
    scanf("%d", &k);
    int m;
    printf("Enter the Virtual Address Space size (max value can be 25): ");
    scanf("%d", &m);
    int f;
    printf("Enter the Physical Address Space size: ");
    scanf("%d", &f);

    key_t key = ftok("master.c", 101);
    shmid1 = shmget(key, k * sizeof(SM), IPC_CREAT | 0666);
    sm = (SM *)shmat(shmid1, NULL, 0);


    key = ftok("master.c", 102);
    shmid2 = shmget(key, (f + 1) * sizeof(int), IPC_CREAT | 0666);
    sm2 = (int *)shmat(shmid2, NULL, 0);

//initialising the shared memory
    for (int i = 0; i < k; i++)
    {
        sm[i].totalpagefaults = 0;
        sm[i].totalillegalaccess = 0;
    }

    //initialising free frames.
    for (int i = 0; i < f; i++)
    {
        sm2[i] = 1;
        // printf("CHECk\n");
    }
    sm2[f] = -1;

    key = ftok("master.c",16);
    semid = semget(key,1,IPC_CREAT | 0666);
    semctl(semid,0,SETVAL,0);
    
     // Message Queue 1 between the scheduler and the process
    key = ftok("master.c", 108);
    msgid1 = msgget(key, IPC_CREAT | 0666);

    // Message Queue 2 between scheduler and mmu
    key = ftok("master.c", 109);
    msgid2 = msgget(key, IPC_CREAT | 0666);

    // Message Queue 3 between process and mmu
    key = ftok("master.c", 110);
    msgid3 = msgget(key, IPC_CREAT | 0666);

    // convert shmids to string
    char shmid1str[10], shmid2str[10];
    sprintf(shmid1str, "%d", shmid1);
    sprintf(shmid2str, "%d", shmid2);
    // convert msgid to string
    char msgid1str[10], msgid2str[10], msgid3str[10];
    sprintf(msgid1str, "%d", msgid1);
    sprintf(msgid2str, "%d", msgid2);
    sprintf(msgid3str, "%d", msgid3);


    // processes as string
    char strk[10];
    sprintf(strk, "%d", k);    

    int refi[k][10*m+1];//max length of refernce seqeunce is 10*m for each process can be
    char refstr[k][1000]; //if m = 25(max value) each string can have max length = 3*(25*10) = 750

    for(int i=0;i<k;i++){
        memset(refstr[i],'\0',sizeof(refstr[i]));
    }

    // initialize the Processes
    for (int i = 0; i < k; i++)
    {
        // generate random number of pages between 1 to m
        sm[i].mi = rand() % m + 1;

        for (int j = 0; j < m; j++)
        {
            sm[i].pagetable[j][0] = -1;      // no frame allocated for this page
            sm[i].pagetable[j][1] = 0;       // invalid
            if(j<sm[i].mi){
                sm[i].pagetable[j][1] = 1;       // valid
            }
            sm[i].pagetable[j][2] = INT_MAX; // timestamp
        }

        int x = rand() % (8 * sm[i].mi + 1) + 2 * sm[i].mi;

        for (int j = 0; j < x; j++)
        {
            refi[i][j] = rand() % sm[i].mi;
            if ((double)rand() / RAND_MAX < PROB)
            {
                refi[i][j] = rand() % m;
            }
        }

        for (int j = 0; j < x; j++)
        {
            char temp[12] = {'\0'};
            sprintf(temp, "%d.", refi[i][j]);
            strcat(refstr[i], temp);
        }
    }

    pidscheduler = fork();
    if (pidscheduler == 0)
    {
        execlp("./sched", "./sched", msgid1str, msgid2str, strk, NULL);
    }

    pidmmu = fork();
    if (pidmmu == 0)
    {
        execlp("xterm", "xterm", "-T", "Memory Management Unit", "-e", "./mmu", msgid2str, msgid3str, shmid1str, shmid2str, NULL);
        // execlp("./mmu","./mmu", msgid2str, msgid3str, shmid1str, shmid2str, NULL);
    }

    // create Processes
    for (int i = 0; i < k; i++)
    {
        usleep(250000);
        int pid = fork();
        if (pid != 0)
        {
            sm[i].pid = pid;
        }
        else
        {
            sm[i].pid = getpid();
            execlp("./process", "./process", refstr[i], msgid1str, msgid3str, NULL);
        }
    }

    semop(semid,&waitop,1);

    int total_illegal = 0;
    int total_fault = 0;

    FILE *file = fopen("result.txt", "a+");

    for(int i=0;i<k;i++){
        fprintf(file,"Page faults in process P%d is %d\n",i+1,sm[i].totalpagefaults);
        fflush(file);
        total_fault+=sm[i].totalpagefaults;
        if(sm[i].totalillegalaccess){
            fprintf(file,"Process P%d made an illegal access\n",i+1);
            fflush(file);
            total_illegal++;
        }
        fprintf(file,"#############################\n");
        fflush(file);
    }

    fprintf(file,"Total Page faults is %d\n",total_fault);
    fprintf(file,"Total illegal accesses is %d\n",total_illegal);
    fflush(file);

    fclose(file);


    // terminate Memory Management Unit
    kill(pidmmu, SIGINT);

    shmdt(sm);
    shmctl(shmid1, IPC_RMID, NULL);

    shmdt(sm2);
    shmctl(shmid2, IPC_RMID, NULL);

    // remove semaphores
    semctl(semid, 0, IPC_RMID, 0);

    // remove message queues
    msgctl(msgid1, IPC_RMID, NULL);
    msgctl(msgid2, IPC_RMID, NULL);
    msgctl(msgid3, IPC_RMID, NULL);

    return 0;

}