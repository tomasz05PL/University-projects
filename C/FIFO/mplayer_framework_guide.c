#include "libs.h"

volatile int end_flag = 0;

void clean_flag() {
    end_flag = 1;
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, clean_flag);
    signal(SIGINT, clean_flag);

    int exit_code = 0;

    char buf = {0};
    char command[COMM_BUF] = {0};
    int comm_size = 0;
    
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <fifo_name>\n", argv[0]);
        exit(0);
    }

    int fifo_fd = open(argv[1], O_WRONLY);
    if(fifo_fd < 0) {
        if(errno == ENOENT) {
            if(mkfifo(argv[1], S_IRUSR | S_IWUSR) != 0) {
                perror("fifo");
                exit(1);
            } 
            fifo_fd = open(argv[1], O_WRONLY);
            if(fifo_fd < 0) {
                perror("open");
                exit(1);
            }
        }
        else {
            perror("open");
            exit(1);
        }
    }
    while(!end_flag) {
        printf("Podaj instrukcję: ");
        buf = getchar();
        int ch;
        switch(buf) {
            case 'j':
                strcpy(command, "seek 10\n\0");
                break;
            
            case 'l':
                strcpy(command, "seek -10\n\0"); 
                break;

            case 'k':
                strcpy(command, "pause\n\0");
                break;

            case 'q':
                strcpy(command, "quit\n\0");
                break;

            case 's':
                strcpy(command, "stop\n\0");
                break;

            case '>':
                strcpy(command, "seek 60\n\0");
                break;
            
            case '<':
                strcpy(command, "seek -60\n\0");
                break;
            
            case 'm':
                strcpy(command, "mute\n\0");
                break;

            case '+':
                strcpy(command, "af volume=1\n\0");
                break;

            case '-':
                strcpy(command, "af volume=-1\n\0");
                break;

            case '0':
                strcpy(command, "seek 0 1\n\0");
                break;

            case 'h':
                printf("Instrukcja programu\n");
                printf("j/l - przewin o 10 sekund do tylu/przodu\n");
                printf("</> - przewin o 60 sekund do tylu/przody\n");
                printf("0   - przewin na poczatek\n");
                printf("k   - zatrzymaj/wznow\n");
                printf("s   - zatrzymaj odtwarzanie (bez zatrzymywnia programu)\n");
                printf("q   - wyjdz z programu\n");
                printf("+/- - podglosnij/scisz o 1 dB\n");
                printf("m   - wycisz\n");
                break;    
            
            default:
                while((ch = getchar()) != '\n' && ch != EOF);
            continue;
        }
        comm_size = strlen(command);
        while((ch = getchar()) != '\n' && ch != EOF);

        if(write(fifo_fd, command, comm_size) != comm_size) {
            exit_code = 1;
            goto cleanup;
        }
        if(buf =='q') {
            goto cleanup;
        }
    }

    cleanup:
    if(close(fifo_fd) == -1) {
        perror("close");
        exit(1);
    }
    if(unlink(argv[1]) != 0) {
        perror("unlink");
        exit(1);
    }

    return exit_code;
}