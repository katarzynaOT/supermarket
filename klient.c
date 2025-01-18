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
            perror("K:nie mozna opuscic semafora");
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

void *idz_do_sklepu(void*arg) {
    if (semctl(id_sem, 2, GETVAL) == 0) { //sklep zamkniety, czekaj
        printf("Klient %d czeka przed sklepem, bo sklep zamkniety \t %d\n", getpid(), semctl(id_sem, 3, GETVAL));
        while (semctl(id_sem, 3, GETVAL) == 0) sleep(0.1); //czekaj przed sklepem az otworza
        printf("Klient %d przeczekal\n", getpid());
    }   
    if (semctl(id_sem, 2, GETVAL) == 1) sem_p(0); //wejdz jak otwarty sklep
}

void zapisz_sie(const char *tresc) {
    FILE *file = fopen("ksiega_gosci.txt", "a"); //plik raport
    if (file == NULL) {
        perror("Nie można otworzyć pliku");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s", tresc); //zawartosc do pliku
    fclose(file);
}


int main(){
    sleep(2);
    id_sem = semget(klucz, 3, 0600 | IPC_CREAT); //semaforki
    if (id_sem == -1) {
        perror("blad tworzenia semaforow");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 4; i ++) { //tworzenie klientow
        pid_t pid = fork();
        if (pid == -1) { 
            perror("fork error");
            exit(EXIT_FAILURE);
        } else if (pid > 0) { //rodzic
            sleep(2);
            //while (wait(NULL) > 0); //rodzic czeka na ukonczenie wszystkich potomkow
        } 
    }

    //klient w sklepie  
    /*if (semctl(id_sem, 2, GETVAL) == 0 || semctl(id_sem, 0, GETVAL) == 0) { //sklep zamkniety albo przepelniony, czekaj
        printf("Klient %d czeka przed sklepem\n", getpid());
        while (semctl(id_sem, 2, GETVAL) == 0) sleep(0.1); //czekaj przed sklepem az otworza
        printf("Klient %d przeczekal\n", getpid());
    }*/  
 

    //if (semctl(id_sem, 2, GETVAL) == 1 && semctl(id_sem, 0, GETVAL) > 0) { //wejdz jak otwarty sklep i nie przelniony
        sem_p(0); //wejdz do sklepu
        printf("Klient %d wchodzi do sklepu. \t %d\\30 \n", getpid(), 30-semctl(id_sem, 0, GETVAL));
        sleep(4); //badz w sklepie
            
        //zapisz sie w ksiedze gosci
        sem_p(1);
        zapisz_sie("i");
        sem_v(1);

        sem_v(0);
        printf("Klient %d wychodzi ze sklepu.\t %d\\30 \n", getpid(), 30-semctl(id_sem, 0, GETVAL));
        
    //} 
        

        
    

    return 0;
}
