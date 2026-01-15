#include "libs.h"

// shm and sem init
int init(int* shm_id, int* sem_id_a) {
    // key creation
    key_t key = ftok(KEYFILE, KEY_ID);
    if(key == -1) {
        perror("ftok");
        return -1;
    }

    // shm create
    *shm_id = shmget(key, 2*sizeof(int), IPC_CREAT | 0666);
    if(*shm_id == -1) {
        perror("shmget");
        return -1;
    }

    // ataching memory
    void* ptr = shmat(*shm_id, NULL, 0);
    if(ptr == (void*)-1) {
        perror("shmat");
        shmctl(*shm_id, IPC_RMID, NULL);
        return -1;
    }

    // clearing the memory
    int *account_ptr = (int*)ptr;
    account_ptr[0] = 0;
    account_ptr[1] = 0;

    // sem create
    *sem_id_a = semget(key, 2, IPC_CREAT | 0666);
    if(*sem_id_a == -1) {
        perror("semget");
        shmdt(ptr);
        shmctl(*shm_id, IPC_RMID, NULL);
        return -1;
    }

    // setting sem 0 to 0
    union my_semun arg;
    arg.val = 0;
    if(semctl(*sem_id_a, 0, SETVAL, arg) == -1) {
        perror("semctl");
        semctl(*sem_id_a, 0, IPC_RMID);
        shmdt(ptr);                
        shmctl(*shm_id, IPC_RMID, NULL);
        return -1;
    }

    // setting sem 1 to 0
    if(semctl(*sem_id_a, 1, SETVAL, arg) == -1) {
        perror("semctl");
        semctl(*sem_id_a, 0, IPC_RMID);
        shmdt(ptr);                
        shmctl(*shm_id, IPC_RMID, NULL);
        return -1;
    }

    // detaching memory
    if(shmdt(ptr) == -1) {
        perror("shmdt");
        semctl(*sem_id_a, 0, IPC_RMID);
        shmctl(*shm_id, IPC_RMID, NULL);
        return -1;
    }

    return 1;
}

// connecting
int connect_bank(int* shm_id, int* sem_id, int** account_ptr) {
    // key creation
    key_t key = ftok(KEYFILE, KEY_ID);
    if(key == -1) {
        perror("ftok");
        return -1;
    }

    // connnecting to existing shm
    *shm_id = shmget(key, 2*sizeof(int), 0666);
    if(*shm_id == -1) {
        perror("shmget");
        return -1;
    }

    // attaching memory
    void* ptr = shmat(*shm_id, NULL, 0);
    if(ptr == (void*)-1) {
        perror("shmat");
        return -1;
    }

    // casting memory on int
    *account_ptr = (int*)ptr;

    // connecitng to semaphore
    *sem_id = semget(key, 2, 0666);
    if(*sem_id == -1) {
        perror("semget");
        shmdt(ptr);
        return -1;
    }

    return 1;
}

int cleanup(int shm_id, int sem_id, int *accounts) {
    int ret_val = 1;
    if (sem_id != -1) {
        if (semctl(sem_id, 0, IPC_RMID) == -1) {
            if (errno != EINVAL && errno != EIDRM) {
                perror("Warning: semctl cleanup failed");
                ret_val = -1;
            }
        } else {
            printf("Semafory usunięte.\n");
        }
    }

    if (accounts != NULL && accounts != (void*)-1) {
        if (shmdt(accounts) == -1) {
            perror("Warning: shmdt failed");
            ret_val = -1;
        } else {
            printf("Pamięć odłączona.\n");
        }
    }

    if (shm_id != -1) {
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            if (errno != EINVAL && errno != EIDRM) {
                perror("Warning: shmctl cleanup failed");
                ret_val = -1;
            }
        } else {
            printf("Pamięć usunięta z systemu.\n");
        }
    }

    return ret_val;
}

int main(int argc, char* argv[]) {
    int shm_id, sem_id;
    if(argc < 2) {
        fprintf(stderr, "Użycie: %s <typ_operacji> <?konto> <?ilość_operacji> <?wartość_operacji>\n", argv[0]);
        exit(1);
    }
    int role = atoi(argv[1]);
    if((role == 1 || role == 2) && argc != 5) {
        fprintf(stderr, "Niezgodność roli z argumentami!\n");
        exit(1);
    }
    switch(role) {
        case 0:
            if(init(&shm_id, &sem_id) == -1) {
                printf("Nie udało się utworzyć zasobów\n");
                exit(1);
            }
            printf("Utworzenie zasobów przebiegło poprawnie. Naciśnij Enter, aby zwolnić semafory...\n");
            getchar();
            if(sem_v(sem_id, 0) == -1) {
                exit(1);
            }
            if(sem_v(sem_id, 1) == -1) {
                exit(1);
            }
        break;
        case 1: {
            int target = atoi(argv[2]);
            int nops = atoi(argv[3]);
            int val = atoi(argv[4]);
            int *accounts;

            if(connect_bank(&shm_id, &sem_id, &accounts) == -1) {
                printf("Nie udało się uzyskać zasobow\n");
                exit(1);
            }

            for(int i = 0; i < nops; ++i) {
                if(sem_p(sem_id, target) == -1) {
                    exit(1);
                }
                accounts[target] += val;
                if(sem_v(sem_id, target) == -1) {
                    exit(1);
                }
            }

            if(shmdt(accounts) == -1) {
                perror("shmdt");
                exit(1);
            }
        }
        break;
        case 2: {
            int target = atoi(argv[2]);
            int nops = atoi(argv[3]);
            int val = atoi(argv[4]);
            int *accounts;

            if(connect_bank(&shm_id, &sem_id, &accounts) == -1) {
                printf("Nie udało się uzyskać zasobow\n");
                exit(1);
            }

            for(int i = 0; i < nops; ++i) {
                if(sem_p(sem_id, 0) == -1) {
                    exit(1);
                }
                if(sem_p(sem_id, 1) == -1) {
                    exit(1);
                }
                if(target == 0){
                    accounts[0] += val;
                    accounts[1] -= val;
                }
                else {
                    accounts[1] += val;
                    accounts[0] -= val;
                }
                if(sem_v(sem_id, 0) == -1) {
                    exit(1);
                }
                if(sem_v(sem_id, 1) == -1) {
                    exit(1);
                }
            }

            if(shmdt(accounts) == -1) {
                perror("shmdt");
                exit(1);
            }
        }
        break;
        case 3: {
            int *accounts;
            if(connect_bank(&shm_id, &sem_id, &accounts) == -1) {
                printf("Nie udało się uzyskać zasobow. Sprzątanie zakończone niepowodzeniem\n");
                exit(1);
            }
            printf("Końcowe salda: 0 -> %d, 1 -> %d\n", accounts[0], accounts[1]);
            if(cleanup(shm_id, sem_id, accounts) == -1) {
                printf("Sprzątanie zakończone niepowidzeniem\n");
                exit(1);
            }
            printf("Zakończono sprzątanie\n");
        }
        break;
    }

    return 0;
}