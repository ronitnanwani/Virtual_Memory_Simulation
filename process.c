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


typedef struct message1
{
    long type;
    int pid;
} message1;

typedef struct message3
{
    long type;
    int pageorframe;
    int pid;
} message3;

int main(int argc, char *argv[])
{   
    // printf("In main\n");

    if (argc != 4)
    {
        printf("Usage: %s <Reference String> <MessageQueue id 1> <MessageQueue id 3>\n", argv[0]);
        exit(1);
    }

    int msgid1 = atoi(argv[2]);
    int msgid3 = atoi(argv[3]);

    char *refstr;
    strcpy(refstr, argv[1]);

    printf("\t\t\t\tProcess with pid = %d has started\n",getpid());

    int pid = getpid();

    message1 msg1;
    msg1.type = 1;
    msg1.pid = pid;

    // send pid to ready queue
    msgsnd(msgid1, (void *)&msg1, sizeof(message1), 0);

    message1 msgr;
    //receive message of type pid from the scheduler indicating process to proceed further
    msgrcv(msgid1,(void*)&msgr,sizeof(message1),pid,0); 

    // fprintf(stderr,"Reference string : %s\n",refstr);
    int previ = 0;
    int i = 0;
    while (refstr[i] != '\0')
    {
        message3 msg3;
        msg3.type = 1;
        msg3.pid = pid;
        int j = 0;
        previ = i;
        while (refstr[i] != '.' && refstr[i] != '\0')
        {   
            j = j * 10 + (refstr[i] - '0');
            i++;
        }
        i++;
        msg3.pageorframe = j;
        // printf("Message sent from here = %d\n",j);
        // printf("i after = %d\n",i);

        //send message to queue 3 of type 1
        msgsnd(msgid3, (void *)&msg3, sizeof(message3), 0);

        // wait for the mmu to allocate the frame
        // printf("\tProcess waiting for mmu to allocate frame - before sleep\n");
        // sleep(2);
        // printf("\tProcess waiting for mmu to allocate frame - after sleep\n");

        //receive message from queue 3 of type pid
        msgrcv(msgid3, (void *)&msg3, sizeof(message3), pid, 0);


        if (msg3.pageorframe == -1)
        {
            fprintf(stderr,"\t\t\t\tProcess %d: ", pid);
            fprintf(stderr,"Page Fault\nWaiting for page to be loaded\n");

            // scheduler will communicate via message queue to indicate that this process is scheduled...till then wait
            msgrcv(msgid1,(void*)&msgr,sizeof(message1),pid,0);
            i = previ;      //update i
            continue;
        }
        else if (msg3.pageorframe == -2)
        {
            fprintf(stderr,"\t\t\t\tProcess %d: ", pid);
            fprintf(stderr,"Illegal Page Number\nTerminating\n");
            exit(1);
        }
        else
        {
            printf("\t\t\t\tProcess %d: ", pid);
            printf("Frame %d allocated\n", msg3.pageorframe);
        }
    }

    //send page = -9 in the end
    printf("\t\t\t\tProcess %d: ", pid);
    printf("Got all frames properly\n");
    message3 msg3;
    msg3.type = 1;
    msg3.pid = pid;
    msg3.pageorframe = -9;

    msgsnd(msgid3, (void *)&msg3, sizeof(message3), 0);
    printf("\t\t\t\tTerminating\n");

    return 0;
}
