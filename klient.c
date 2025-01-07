//klient
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>


key_t klucz = 21370; //ftok("pp", 127);
int id_sem; 

static void sem_p(int nr) { //opuszczenie semafora
    struct sembuf bufor_sem;
    bufor_sem.sem_num = nr;
    bufor_sem.sem_op = -1;
    bufor_sem.sem_flg = 0;
    if (semop(id_sem, &bufor_sem, 1) == -1) {
        if (errno == EINTR) sem_p(nr); //jesli semop nieudany przez to, ze byl wyslany sygnal
        else {
            perror("nie mozna opuscic semafora");
            exit(EXIT_FAILURE);
        }
    }
}

static void sem_v(int nr) { //podniesienie semafora
    struct sembuf bufor_sem;
    bufor_sem.sem_num = nr;
    bufor_sem.sem_op = 1;
    bufor_sem.sem_flg = 0; //SEM_UNDO
    if (semop(id_sem, &bufor_sem, 1) == -1) {
        perror("nie mozna podniesc semafora");
        exit(EXIT_FAILURE);
    }
}

int main(){
    sleep(2);
    id_sem = semget(klucz, 4, 0600 | IPC_CREAT); //semaforki
    if (id_sem == -1) {
        perror("blad tworzenia semaforow");
        exit(EXIT_FAILURE);
    }

    //tworzenie klientow
    for (int i = 0; i < 3; i ++) {
        pid_t pid = fork();
        if (pid == -1) { 
            perror("fork error");
            exit(EXIT_FAILURE);
        } else if (pid > 0) { //rodzic
            sleep(2);
        } 

        //klient w sklepie
        sem_p(0);
        printf("Klient %d wchodzi do sklepu. \t %d\\30 \n", getpid(), 30-semctl(id_sem, 0, GETVAL));
        sleep(5);
        sem_v(0);
        printf("Klient %d wychodzi ze sklepu.\t %d\\30 \n", getpid(), 30-semctl(id_sem, 0, GETVAL));
        
    }

    return 0;
}
