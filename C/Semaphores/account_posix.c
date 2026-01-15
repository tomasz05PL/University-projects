#include "posix_libs.h"

sem_t* sem;

int account = 0;

void simulate_heavy_work(int min_ms, int max_ms) {
    long duration_ms = min_ms + rand() % (max_ms - min_ms + 1);

    struct timespec ts;
    ts.tv_sec = duration_ms / 1000;         
    ts.tv_nsec = (duration_ms % 1000) * 1000000;

    full_nanosleep(&ts);
}

void* perform_account_op(void* arg) {
    int* args = (int*)arg;
    int value = args[0], n = args[1];

    for(int i = 0; i < n; ++i) {
        if(sem_wait(sem) == -1) {
            perror("sem_wait");
            free(arg);
            return (void*)(long)-1;
        } 

        int tmp = account;
        tmp += value;
        simulate_heavy_work(10,300);
        account = tmp;

        if(sem_post(sem) == -1) {
            perror("sem_post");
            free(arg);
            return (void*)(long)-1;
        }
    }

    free(arg);

    return (void*)0;
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    if(argc != 6) {
        fprintf(stderr, "Usage: %s <thread_n> <in> <out> <n_in> <n_out>", argv[0]);
        exit(1);
    }

    int args[5] = {0};
    for(int i = 0; i < 5; ++i) {
        args[i] = atoi(argv[i+1]);
    }

    sem_unlink(SEM_NAME);

    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);
    if(sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    pthread_t* threads_in = NULL;
    pthread_t* threads_out = NULL;
    int in_created = 0, out_created = 0, creat_flag = 0;
    
    threads_in = malloc(sizeof(pthread_t) * args[0]);
    if(threads_in == NULL) {
        perror("malloc");
        goto cleanup;
    }

    threads_out = malloc(sizeof(pthread_t) * args[0]);
    if(threads_out == NULL) {
        perror("malloc");
        goto cleanup;
    }

    for(int i = 0; i < args[0]; ++i) {
        int *args_in = malloc(sizeof(int) * 3);
        if(args_in == NULL) {
            perror("malloc");
            goto cleanup;
        }

        args_in[0] = args[1];
        args_in[1] = args[3];
        if(pthread_create(&threads_in[i], NULL, perform_account_op, args_in) != 0) {
            perror("pthread_create");
            creat_flag = 1;
            free(args_in);
            goto cleanup;
        }
        ++in_created;

        int *args_out = malloc(sizeof(int) * 2);
        if(args_out == NULL) {
            perror("malloc");
            goto cleanup;
        }

        args_out[0] = -args[2];
        args_out[1] = args[4];
        if(pthread_create(&threads_out[i], NULL, perform_account_op, args_out) != 0) {
            perror("pthread_create");
            creat_flag = 1;
            free(args_out);
            goto cleanup;
        }
        ++out_created;
    }

    void* ret_value;
    long status;
    int err_flag = 0;

    for(int i = 0; i < args[0]; ++i) {
        pthread_join(threads_in[i], &ret_value);
        status = (long)ret_value;
        if(status == -1) {
            printf("Something went wrong in thread IN %d\n", i);
            err_flag = 1;
        }

        pthread_join(threads_out[i], &ret_value);
        status = (long)ret_value;
        if(status == -1) {
            printf("Something went wrong in thread IN %d\n", i);
            err_flag = 1;
        }
    }

    int expected = (args[0] * args[1] * args[3]) - (args[0] * args[2] * args[4]);
    printf("Finish.\n");
    printf("Expected: %d\n", expected);
    printf("Real account value:  %d\n", account);
    printf("Error flag: %d\n", err_flag);

    if(expected == account) {
        printf(">> SUCCESS <<\n");
    }
    else {
        printf(">> FAILURE <<\nCheck Error flag\n");
    }

    cleanup:
    if(creat_flag == 1) {
        for(int i = 0; i < in_created; ++i) {
            pthread_cancel(threads_in[i]);
        }
        for(int i = 0; i < out_created; ++i) {
            pthread_cancel(threads_out[i]);
        }

        void* trash;
        for(int i = 0; i < in_created; ++i) {
            pthread_join(threads_in[i], &trash);
        }
        for(int i = 0; i < out_created; ++i) {
            pthread_join(threads_out[i], &trash);
        }
    }
    free(threads_in);
    free(threads_out);
    if(sem_close(sem) == -1) {
        perror("sem_close");
    }
    if(sem_unlink(SEM_NAME) == -1) {
        perror("unlink");
        exit(1);
    }
    return 0;
}