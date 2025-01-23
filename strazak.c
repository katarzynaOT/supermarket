#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

key_t klucz = 21370; //ftok("pp", 127);
int id_sem;

void signal_handler(int sig) {
    if (sig == SIGTERM) {
        printf("\tStrazak\n");
    }
}

void wyslij_sygnal(int pid, int sig) {
    if (kill(pid, sig) == 0) {
        printf("Wysłano sygnał %d do procesu %d\n", sig, pid);
    } else {
        perror("Nie udało się wysłać sygnału");
    }
}


int main() {
    id_sem = semget(klucz, 3, 0600 | IPC_CREAT); //semaforki
    if (id_sem == -1) {
        perror("blad tworzenia semaforow");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    sleep(10);
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("blad ustawienia sygnalu");
        exit(EXIT_FAILURE);
    }

    //wysylanie sygnalu
    pid_t pid;
   	if (kill(pid, SIGUSR1) == -1) {
        perror("Nie udalo sie wyslac SIGUSR1 do pid");
        exit(EXIT_FAILURE);
    }



}
