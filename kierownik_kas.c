//kierownik kas
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
#include <sys/prctl.h> //prctl


#define DZIEN 30 //czas trwania jednego dnia

int N; //liczba czynnych kas
int K; //liczba klientow w sklepie
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
    bufor_sem.sem_flg = SEM_UNDO;
    if (semop(id_sem, &bufor_sem, 1) == -1) {
        perror("nie mozna podniesc semafora");
        exit(EXIT_FAILURE);
    }
}

void pisz_raport(const char *tresc) {
    FILE *file = fopen("raport.txt", "a"); //plik raport
    if (file == NULL) {
        perror("Nie można otworzyć pliku");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s", tresc); //zawartosc do pliku
    fclose(file);
}

//otwieranie po nocy
void otworz_sklep(void) {
    if (semctl(id_sem, 0, SETVAL, 30) == -1) { //semafor ochroniarz 1 (odpowiada za to, by sklep sie nie przepelnil)
        perror("blad ustawienia semafora 0");
        exit(EXIT_FAILURE);
    }
    if (semctl(id_sem, 1, SETVAL, 1) == -1) { //manager sklepu (blokada raportu - otwarty)
        perror("blad ustawienia semafora 1");
        exit(EXIT_FAILURE);
    }
    if (semctl(id_sem, 2, SETVAL, 1) == -1) { //semafor ochroniarz 2 (blokada sklepu - otworz)
        perror("blad ustawienia semafora 2");
        exit(EXIT_FAILURE);
    }
    /*sem_p(1); //zamknij blokade raportu
    FILE *file = fopen("raport_dzienny.txt", "w"); //plik raport (nadpisz)
    if (file == NULL) {
        perror("problem otwarcia pliku");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s", "---------RAPORT-DZIENNY--------\n"); //zawartosc do pliku
    fclose(file);
    sem_v(1); //otworz blokade raportu */
    printf("\tSklep otwarty\n");
}

//zamykanie nocne
void zamknij_sklep(void) {
    sem_p(2); //ochroniarz pilnuje wejscia, by nikt nie wszedl
    while(1) {
        sleep(1);
        if(semctl(id_sem, 0, GETVAL) == 30) { //czy wszyscy klienci wyszli
            printf("Nikogo nie ma w sklepie\n");
            //semctl(id_sem, 0, SETVAL, 0);
            printf("Nikogo nie ma na kasach\n");
            break;
        }
    }
    //dopisz do raport miesieczny...
    printf("przeliczanie liczby klientow... \n");
    //przelicz klientow
    int potok1[2]; // potok2[2]; // cat ksiega_gosci.txt |wc -c | write()
 	if (pipe(potok1) == -1) { //|| pipe(potok2) == -1){
 		perror("pipe error"); 
 		exit(EXIT_FAILURE);
 	}


    pid_t a = fork();
 	if(a == -1) { //podliczanie klientow
 			perror("fork error");
 			exit(EXIT_FAILURE);
    } else if (a == 0) {
 			close(potok1[0]);
 			dup2(potok1[1], 1);
            sleep(1);
 			execlp("cat","cat", "ksiega_gosci.txt",NULL);
            //ten exec zapisac do pliku
 			perror("execlp error");
 			exit(EXIT_FAILURE);
    } else {
        pid_t b = fork();
        if (b == -1) { 
            perror("fork error");
 		    exit(EXIT_FAILURE);
        } else if (a == 0) {
		    close(potok1[1]);
 		    dup2(potok1[0], 0);
            sleep(1);
            printf("\nPrzyszlo dzis tyle klientow: ");
 		    execlp("wc", "wc", "-c", NULL);
 		    perror("execlp error");
		    exit(EXIT_FAILURE);
        }
        printf("\tSklep zamkniety\n");
    }
 			
}

void zakoncz_program() {
    printf("\n\t KONCZE PROGRAM \n");
    if (semctl(id_sem, 0, IPC_RMID) == -1) {
        perror("blad usunania semaforow");
        exit(EXIT_FAILURE);
    }
    if (fork() == 0) {
        execlp("cat", "raport_dzienny.txt", NULL);
        perror("nie udany execlp");
        exit(EXIT_FAILURE);
    }
}


int main(){
    id_sem = semget(klucz, 3, 0600 | IPC_CREAT); //semaforki
    if (id_sem == -1) {
        perror("blad tworzenia semfarow");
        exit(EXIT_FAILURE);
    }

    otworz_sklep();
    int ilosc_klientow = 30 - semctl(id_sem, 0, GETVAL);
    printf("Ilosc klientow: %d\n", ilosc_klientow);



    sleep(5);
    ilosc_klientow = 30 - semctl(id_sem, 0, GETVAL);
    printf("Ilosc klientow: %d\n", ilosc_klientow);

    zamknij_sklep();
    zakoncz_program();

    return 0;
}
