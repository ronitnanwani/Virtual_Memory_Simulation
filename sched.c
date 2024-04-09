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
#include <time.h>

struct sembuf waitop = {0, -1, 0};
struct sembuf signalop = {0, 1, 0};

typedef struct message1
{
    long type;
    int pid;
} message1;

typedef struct message2
{
    long type;
    int pid;
} message2;

int main(int argc, char *argv[])
{

    if (argc != 4)
    {
        printf("Usage: %s <Message Queue 1 ID> <Message Queue 2 ID> <Number of Processes>\n", argv[0]);
        exit(1);
    }

    int msgid1 = atoi(argv[1]);
    int msgid2 = atoi(argv[2]);
    int k = atoi(argv[3]);

    int pid = getpid();

    message1 msg1;
    message2 msg2;

    while (k > 0)
    {
        // receive type 1 message from the ready queue
        msgrcv(msgid1, (void *)&msg1, sizeof(message1), 1, 0);

        printf("\t\tScheduling process %d\n", msg1.pid);

        
        message1 msgs;
        msgs.type = msg1.pid;
        msgs.pid = msg1.pid;     //Not needed

        //signal process of pid to start
        msgsnd(msgid1,(void*)(&msgs),sizeof(message1),0);

        // wait for mmu
        msgrcv(msgid2, (void *)&msg2, sizeof(message2), 0, 0);

        // check the type of message
        if (msg2.type == 1)
        {
            printf("\t\tProcess %d added to end of queue\n", msg2.pid);
            msg1.pid = msg2.pid;
            msg1.type = 1;
            msgsnd(msgid1, (void *)&msg1, sizeof(message1), 0);
        }
        else if (msg2.type == 2)
        {
            printf("\t\tProcess %d terminated\n", msg2.pid);
            k--;
        }
    }

    printf("\t\tScheduler terminated\n");

    key_t key = ftok("master.c",16);
    int semid = semget(key,1,0666);

    semop(semid,&signalop,1);
    
    return 0;
}