#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

int add = 0; /* place to add next element */
int rem = 0; /* place to remove next element */

struct emsg
{
    long int mtype;
    int empty;
} myemsg;

struct fmsg
{
    long int mtype;
    int full;
} myfmsg;

typedef unsigned short ushort;

/* arg for semctl system calls. */
union Semun {
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    ushort *array;         /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};

void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

int main()
{
    int buffsz;
    printf("Enter buffer size: ");
    scanf("%d", &buffsz);

    int shmid = shmget(120, sizeof(int) * buffsz, IPC_CREAT | 0644);
    if (shmid == -1)
    {
        perror("Error in creating shm in producer");
        exit(-1);
    }
    int shmid2 = shmget(121, sizeof(int), IPC_CREAT | 0644);
    if (shmid2 == -1)
    {
        perror("Error in creating shm in producer");
        exit(-1);
    }
    int shmid3 = shmget(122, sizeof(int), IPC_CREAT | 0644);
    if (shmid3 == -1)
    {
        perror("Error in creating shm3 in producer");
        exit(-1);
    }
    int *buff = (int *)shmat(shmid, (void *)0, 0);
    int *num = (int *)shmat(shmid2, (void *)0, 0);
    
    

    union Semun semun1, semun2;

    int sem1 = semget(110, 1, 0666 | IPC_CREAT);
    semun1.val = 1;
    if (semctl(sem1, 0, SETVAL, semun1) == -1)
    {
        fprintf(stderr, "Error in semctl");
        exit(-1);
    }
    int sem2 = semget(111, 1, 0666 | IPC_CREAT);
    int sem3 = semget(106, 1, 0666 | IPC_CREAT);
    semun2.val = 0;
    if (semctl(sem2, 0, SETVAL, semun2) == -1)
    {
        fprintf(stderr, "Error in semctl");
        exit(-1);
    }
    if (semctl(sem3, 0, SETVAL, semun2) == -1)
    {
        fprintf(stderr, "Error in semctl");
        exit(-1);
    }
    myemsg.empty = 1;
    myfmsg.full = 0;

    int msgqid1 = msgget(614, IPC_CREAT | 0644);
    int msgqid2 = msgget(615, IPC_CREAT | 0644);

    int *sz = (int *)shmat(shmid3, (void *)0, 0);
    up(sem3);
    *sz = buffsz;
    shmdt(sz);

    up(sem2);
    for (int i = 1; i <= 20; i++)
    {
        down(sem1);

        if (*num > buffsz)
        {
            shmdt(buff);
            exit(0);
        }

        if (myfmsg.full)
        {
            printf("Buffer is now full, producer will wait for the consumer to consume an item\n");
            int rec_val = msgrcv(msgqid2, &myfmsg, sizeof(myfmsg.full), 9, !IPC_NOWAIT);
            if (rec_val == -1)
                fprintf(stderr, "Error while receiveing not full from consumer\n");
        }

        /* if executing here, buffer not full so add element */
        buff[add] = i;
        printf("Producer produced item (%d) in position (%d)\n ", add, i);
        add = (add + 1) % buffsz;
        printf("Prooducer index is now (%d)\n", add);
        (*num)++;
        printf("The count of elements in the buffer is (%d) \n", *num);
        if (*num == buffsz)
            myfmsg.full = 1;

        if (myemsg.empty)
        {
            myemsg.empty = 0;
            myemsg.mtype = 7;
            printf("Producer is waking up the consumer\n");
            int send_val = msgsnd(msgqid1, &myemsg, sizeof(myemsg.empty), !IPC_NOWAIT);
            if (send_val == -1)
                fprintf(stderr, "Error while sending not empty to consumer\n");
        }
        up(sem1);
        printf("producer: inserted %d\n", i);
        fflush(stdout);
    }
    printf("producer quiting\n");
    fflush(stdout);
    shmctl(shmid, IPC_RMID, (struct shmid_ds *)0);
    // shmctl(shmid2, IPC_RMID, (struct shmid_ds *)0);
    // shmctl(shmid3, IPC_RMID, (struct shmid_ds *)0);
    return 0;
}