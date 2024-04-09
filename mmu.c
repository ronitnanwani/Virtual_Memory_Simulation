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
#include <limits.h>

typedef struct SM
{
    long pid;         // process id
    int mi;          // number of required pages
    int pagetable[25][3]; // page table
    int totalpagefaults;
    int totalillegalaccess;
} SM;

typedef struct message2
{
    long type;
    int pid;
} message2;

typedef struct message3
{
    long type;
    int pageorframe;
    int pid;
} message3;



int main(int argc, char *argv[])
{   
    int timestamp = 0;

    if (argc != 5)
    {
        printf("Usage: %s <Message Queue 2 ID> <Message Queue 3 ID> <Shared Memory 1 ID> <Shared Memory 2 ID>\n", argv[0]);
        exit(1);
    }

    int msgid2 = atoi(argv[1]);
    int msgid3 = atoi(argv[2]);
    int shmid1 = atoi(argv[3]);
    int shmid2 = atoi(argv[4]);

    FILE* file = fopen("result.txt","w");

    // printf("%d %d %d %d\n",msgid2,msgid3,shmid1,shmid2);

    message2 msg2;
    message3 msg3;
    message3 msgs;

    SM* sm = (SM *)shmat(shmid1, NULL, 0);
    int *sm2 = (int *)shmat(shmid2, NULL, 0);
    // sleep(1);
    // printf("Yaha se - %d\n",sm[0].mi);
    // printf("Here1\n");
    // printf("Here2\n");

    while (1)
    {
        // printf("Waiting\n");

        // receive message of type 1
        msgrcv(msgid3, (void *)&msg3, sizeof(message3), 1, 0);
        // printf("Waitcompleted\n");

        timestamp++;

        printf("Global Ordering - (Timestamp %d, Process %d, Page %d)\n", timestamp, msg3.pid, msg3.pageorframe);
        fprintf(file,"Global Ordering - (Timestamp %d, Process %d, Page %d)\n", timestamp, msg3.pid, msg3.pageorframe);
        fflush(file);
        
        // check if the requested page is in the page table of the process with that pid
        int i = 0;
        // printf("PID = %ld\n",sm[0].pid);
        while (sm[i].pid != msg3.pid)
        {
            i++;
        }

        int page = msg3.pageorframe;
        // fprintf(stderr,"Page Received by mmu = %d\n",page);
        // printf("Hello %d\n",sm[i].pagetable[0][0]);
        // printf("i=%d\n",i);
        // printf("Hi\n");
        // printf("%d\n",(sm[i].pagetable[page][0] == -1));
        if (page >= sm[i].mi)
        {
            // printf("In else if 2\n");
            // illegal page number
            // ask process to kill themselves
            msgs.pageorframe = -2;
            msgs.type = sm[i].pid;

            msgsnd(msgid3, (void *)&msgs, sizeof(message3), 0);

            // update total illegal access
            sm[i].totalillegalaccess++;

            printf("Invalid Page Reference - (Process %d, Page %d)\n", i + 1, page);
            fprintf(file,"Invalid Page Reference - (Process %d, Page %d)\n", i + 1, page);
            fflush(file);

            // free the frames
            for (int j = 0; j < sm[i].mi; j++)
            {
                if (sm[i].pagetable[j][0] != -1)
                {
                    sm2[sm[i].pagetable[j][0]] = 1;
                }
                sm[i].pagetable[j][0] = -1;
                sm[i].pagetable[j][1] = 0;
                sm[i].pagetable[j][2] = INT_MAX;
            }
            msg2.type = 2;
            msg2.pid = msg3.pid;
            msgsnd(msgid2, (void *)&msg2, sizeof(message2), 0);
        }
        else if (page == -9)
        {   
            // printf("m_i = %d\n",sm[i].mi);
            // process is done
            // free the frames
            for (int j = 0; j < sm[i].mi; j++)
            {   
                // printf("j = %d\n",j);
                // printf("Hi1\n");
                // printf("%d\n",sm1[i].pagetable[j][0]);
                // printf("Hi2\n");
                if (sm[i].pagetable[j][0] != -1)
                {
                    sm2[sm[i].pagetable[j][0]] = 1;
                }
                sm[i].pagetable[j][0] = -1;
                sm[i].pagetable[j][1] = 0;
                sm[i].pagetable[j][2] = INT_MAX;
            }

            // printf("Sending to scheduler from mmu\n");
            msg2.type = 2;
            msg2.pid = msg3.pid;
            msgsnd(msgid2, (void *)&msg2, sizeof(message2), 0);
        }
        else if(sm[i].pagetable[page][0] == -1)
        {   
            // printf("In Else\n");
            // page fault
            // ask process to wait
            sm[i].totalpagefaults++;
            msgs.pageorframe = -1;
            msgs.type = sm[i].pid;

            msgsnd(msgid3, (void *)&msgs, sizeof(message3), 0);

            printf("Page fault sequence - (Process %d, Page %d)\n", i + 1, page);
            fprintf(file,"Page fault sequence - (Process %d, Page %d)\n", i + 1, page);
            fflush(file);

            // Page Fault Handler (PFH)
            // check if there is a free frame in sm2
            int j = 0;
            // printf("Hi0\n");
            // printf("Here %d\n",sm2[1]);
            while (sm2[j] != -1)
            {   
                // printf("%d\n",j);
                if (sm2[j] == 1)
                {
                    sm2[j] = 0;
                    break;
                }
                j++;
            }
            // printf("Hi\n");
            // printf("j = %d\n",j);


            if (sm2[j] == -1)
            {
                // printf("No free frame\n");
                // no free frame
                // find the page with the least timestamp
                int min = INT_MAX;
                int minpage = -1;
                for (int k = 0; k < sm[i].mi; k++)
                {
                    if (sm[i].pagetable[k][2] < min)
                    {
                        min = sm[i].pagetable[k][2];
                        minpage = k;
                    }
                }

                
                if(minpage!=-1){
                    sm[i].pagetable[page][0] = sm[i].pagetable[minpage][0];
                    sm[i].pagetable[minpage][0] = -1;
                    sm[i].pagetable[page][1] = 1;
                    sm[i].pagetable[page][2] = timestamp;
                    sm[i].pagetable[minpage][2] = INT_MAX;
                }


                msg2.type = 1;
                msg2.pid = msg3.pid;
                msgsnd(msgid2, (void *)&msg2, sizeof(message2), 0);
            }

            else
            {   
                // printf("free frame found\n");
                // free frame found
                sm[i].pagetable[page][0] = j;
                sm[i].pagetable[page][1] = 1;
                sm[i].pagetable[page][2] = timestamp;

                msg2.type = 1;
                msg2.pid = msg3.pid;
                msgsnd(msgid2, (void *)&msg2, sizeof(message2), 0);
            }
        }
        else
        {
            // page there in memory and valid, return frame number
            // printf("In else if 1\n");
            sm[i].pagetable[page][2] = timestamp;
            msgs.pageorframe = sm[i].pagetable[page][0];
            msgs.type = sm[i].pid;
            msgsnd(msgid3, (void *)&msgs, sizeof(message3), 0);
        }
    }
}