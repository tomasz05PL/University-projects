#ifndef LIBS_H
#define LIBS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <errno.h>

#define KEYFILE "keyfile"
#define KEY_ID 65

union my_semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

int sem_p(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = -1;
    sb.sem_flg = SEM_UNDO;

    if(semop(semid, &sb, 1) == -1) {
        perror("sem_p");
        return -1;
    }
    return 0;
}

int sem_v(int semid, int sem_num) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = 1;
    sb.sem_flg = SEM_UNDO;

    if(semop(semid, &sb, 1) == -1) {
        perror("sem_v");
        return -1;
    }
    return 0;
}

#endif