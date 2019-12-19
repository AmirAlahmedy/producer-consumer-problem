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
int num = 0; /* number elements in buffer */

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

    int sem3 = semget(106, 1, 0666 | IPC_CREAT);
    int shmid3 = shmget(122, sizeof(int), IPC_CREAT | 0644);
    if (shmid3 == -1)
    {
        perror("Error in creating shm3 in consumer");
        exit(-1);
    }
    int *sz = (int *)shmat(shmid3, (void *)0, 0);
    down(sem3);
    buffsz = (*sz);
    shmdt(sz);

    int shmid = shmget(120, sizeof(int) * buffsz, IPC_CREAT | 0644);
    if (shmid == -1)
    {
        fprintf(stderr, "Unable to create shm consumer\n");
        exit(1);
    }
    int shmid2 = shmget(121, sizeof(int), IPC_CREAT | 0644);
    if (shmid2 == -1)
    {
        perror("Error in creating shm2 in consumer");
        exit(-1);
    }
    int *buff = (int *)shmat(shmid, (void *)0, 0);
    int *num = (int *)shmat(shmid2, (void *)0, 0);

    union Semun semun;

    int sem1 = semget(110, 1, 0666 | IPC_CREAT);
    int sem2 = semget(111, 1, 0666 | IPC_CREAT);
    

    int msgqid1 = msgget(614, IPC_CREAT | 0644);
    int msgqid2 = msgget(615, IPC_CREAT | 0644);

    myemsg.empty = 1;

    down(sem2);
    int i;
    while (1)
    {
        down(sem1);

        if (myemsg.empty)
        {
            printf("Buffer is empty, consumer is waiting for an item to be produced\n");
            up(sem1);
            int rec_val = msgrcv(msgqid1, &myemsg, sizeof(myemsg.empty), 7, !IPC_NOWAIT);
            down(sem1);
            if (rec_val == -1)
                fprintf(stderr, "Error while receiveing not empty from producer\n");
        }

        if (*num < 0)
        {
            shmdt(buff);
            exit(1); /* underflow */
        }

        /* if executing here, buffer not empty so remove element */
        i = buff[rem];
        printf("Item (%d) at position (%d), was consumed\n", i, rem);
        buff[rem] = 0;
        rem = (rem + 1) % buffsz;
        printf("Consumer index is now (%d)\n", rem);
        printf("The count of elements in the buffer is (%d)\n", *num);
        (*num)--;
        for (int i = 0; i < buffsz; i++)
            printf("%d ", buff[i]);
        printf("\n");

        if (*num == 0)
            myemsg.empty = 1;

        if (*num == buffsz - 1)
        {
            myfmsg.full = 0;
            myfmsg.mtype = 9;
            printf("Consumer is waking up the Producer\n");
            int send_val = msgsnd(msgqid2, &myfmsg, sizeof(myfmsg.full), !IPC_NOWAIT);
            if (send_val == -1)
                fprintf(stderr, "Error while sending not full to producer\n");
        }

        up(sem1);

        printf("Consume value %d\n", i);
        fflush(stdout);
        sleep(1);
    }

    return 0;
}