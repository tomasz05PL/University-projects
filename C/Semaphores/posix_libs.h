#ifndef POSIX_LIBS
#define POSIX_LIBS

#include <stdio.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#define SEM_NAME "/my_sem"

#include <time.h>
#include <errno.h>

int full_nanosleep(const struct timespec *req) {
    struct timespec rem;
    struct timespec temp_req = *req;

    while (nanosleep(&temp_req, &rem) == -1) {
        if (errno == EINTR) {
            temp_req = rem;
        } else {
            return -1;
        }
    }
    return 0;
}

#endif