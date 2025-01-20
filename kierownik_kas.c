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
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/prctl.h> //prctl
#include <fcntl.h>


#define MAX_TEXT 20 //ilosc znakow do zapisu do kolejki
#define DZIEN 30 //czas trwania jednego dnia
#define MAX_KASY 10 //maksymalna liczba aktywnych kas

int N; //liczba czynnych kas
int K; //liczba klientow w sklepie
key_t klucz = 21370; //ftok("pp", 127);

int id_sem; 
int id_kolejki;
struct msg_buffer {
	long mtype; //numer kasy
	char mtext[MAX_TEXT]; //kto podchodzi do kasy
};

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

//otwieranie po nocy - wlacz semafory, wyczysc ksiege gosciS
void otworz_sklep(void) {
    if (semctl(id_sem, 0, SETVAL, 30) == -1) { //semafor ochroniarz 1 (odpowiada za to, by sklep sie nie przepelnil)
        perror("blad ustawienia semafora 0");
        exit(EXIT_FAILURE);
    }
    if (semctl(id_sem, 1, SETVAL, 1) == -1) { //manager sklepu (blokada ksiegi_gosci - otwarty)
        perror("blad ustawienia semafora 1");
        exit(EXIT_FAILURE);
    }
    if (semctl(id_sem, 2, SETVAL, 1) == -1) { //semafor ochroniarz 2 (blokada sklepu - otworz)
        perror("blad ustawienia semafora 2");
        exit(EXIT_FAILURE);
    }
    if (semctl(id_sem, 3, SETVAL, 2) == -1) { // ilosc aktywnych kas (na poczatku 2)
        perror("blad ustawienia semafora 3");
        exit(EXIT_FAILURE);
    }
    sem_p(1); //wyczysc ksiege gosci
    FILE *file = fopen("ksiega_gosci.txt", "w"); //plik ksiega_gosci (wyczysc)
    if (file == NULL) {
        perror("problem otwarcia pliku");
        exit(EXIT_FAILURE);
    }
    //fprintf(file, "%s", "---------RAPORT-DZIENNY--------\n"); //zawartosc do pliku
    fclose(file);
    sem_v(1); //otworz blokade 
    printf("\t\tSklep otwarty\n");
}

//zamykanie nocne - zamknij sklep, policz klientow z dnia
void zamknij_sklep(void) {
    printf("\tZamknij sklep\n");
    sem_p(2); //ochroniarz pilnuje wejscia, by nikt nie wszedl
    while(1) {
        sleep(1);
        if(semctl(id_sem, 0, GETVAL) == 30) { //czy wszyscy klienci wyszli
            printf("\nNikogo nie ma w sklepie\n");
            //semctl(id_sem, 0, SETVAL, 0);
            printf("Nikogo nie ma na kasach\n");
            break;
        }
    }

    //dopisz do raport miesieczny...
    printf("przeliczanie liczby klientow... \n");

    //przelicz klientow
    int potok1[2]; // cat ksiega_gosci.txt |wc -c 
 	if (pipe(potok1) == -1){
 		perror("pipe error"); 
 		exit(EXIT_FAILURE);
 	}

    pid_t a = fork();
 	if(a == -1) { 
 		perror("fork error");
 		exit(EXIT_FAILURE);
    } else if (a == 0) {
 		close(potok1[0]);
 		dup2(potok1[1], 1);
        //sleep(1);
 		execlp("cat","cat", "ksiega_gosci.txt",NULL);
		perror("execlp 'cat' error");
 		exit(EXIT_FAILURE);
    } else {
	    close(potok1[1]);
 	    dup2(potok1[0], 0);
        close(potok1[0]);
        sleep(1);
            //printf("\n\tLiczba klientow: ");
 		execlp("wc", "wc", "-c", NULL);
 		perror("execlp 'wc' error");
		exit(EXIT_FAILURE);
    }
 			
}

//usun semfory i kolejki
void zakoncz_program() {
    printf("\n\t KONCZE PROGRAM \n");
    if (semctl(id_sem, 0, IPC_RMID) == -1) {
        perror("blad usunania semaforow");
        exit(EXIT_FAILURE);
    }
    if (msgctl(id_kolejki, IPC_RMID, NULL) == -1) {
        perror("blad usuniecia kolejki");
        exit(EXIT_FAILURE);
    }
    /*if (fork() == 0) {
        execlp("cat", "raport_dzienny.txt", NULL);
        perror("nie udany execlp");
        exit(EXIT_FAILURE);
    }*/
}

void *obsluga_kasy(void* arg) {
    int msgid = *(int*)arg; //id kolejki komunikatow
    struct msg_buffer message;
    printf("\tWatek %d jest na kasie\n", getpid());

    // Pobieranie numeru kasy na podstawie ID wątku
    pthread_t tid = pthread_self();
    int id_kasy = tid % MAX_KASY + 1; //cos nie tak

    printf("Kasa %d: Gotowa do pracy.\n", id_kasy);


    while (1) {
        // Odczytanie komunikatu
        //printf("\tKierownik odczytał komunikat: %s\n", message.mtext);
        if (msgrcv(msgid, &message, sizeof(message.mtext) - sizeof(long), id_kasy, 0) == -1) {
            perror("blad odczytania komuinkatu");
            //exit(EXIT_FAILURE);
            continue;
        }
        printf("Kasa %d obsluguje klienta %s\n", id_kasy, message.mtext);

        printf("\tP: %s\n", message.mtext);

        /*char lokalna_kopia[MAX_TEXT];
        strncpy(lokalna_kopia, message.mtext, MAX_TEXT);
        lokalna_kopia[MAX_TEXT - 1] = '\0';  // Zapewnienie, że tekst jest zakończony '\0'

        printf("Kierownik odczytał komunikat od procesu %s\n", lokalna_kopia);*/

        // Usuwanie komunikatu po odczycie jest automatyczne w System V
        sleep(8);  // Symulacja czasu przetwarzania
    }

    return NULL;

}

int main(){
    id_sem = semget(klucz, 4, 0600 | IPC_CREAT); //semaforki
    if (id_sem == -1) {
        perror("blad tworzenia semfarow");
        exit(EXIT_FAILURE);
    }
    id_kolejki = msgget(klucz, 0600 | IPC_CREAT);
    if(id_kolejki == -1) {
		printf("Blad tworzenia kolejki komunikatow.\n");
		exit(EXIT_FAILURE);
	}

    otworz_sklep();
    int ilosc_klientow = 30 - semctl(id_sem, 0, GETVAL);
    printf("Ilosc klientow: %d\n", ilosc_klientow);




/*
    // Tworzenie wątku do obsługi kolejki
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, obsluga_kasy, &id_kolejki) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    // Oczekiwanie na zakończenie wątku (teoretycznie nieskończone)
    pthread_join(thread_id, NULL);
*/

    // Tworzenie wątków dla kas
    pthread_t kasy[MAX_KASY];
    for (int i = 0; i < MAX_KASY; i++) {
        if (pthread_create(&kasy[i], NULL, obsluga_kasy, &id_kolejki) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    // Czekanie na zakończenie wątków (teoretycznie nieskończone)
    for (int i = 0; i < MAX_KASY; i++) {
        pthread_join(kasy[i], NULL);
    }





    otworz_sklep();
    ilosc_klientow = 30 - semctl(id_sem, 0, GETVAL);
    printf("Ilosc klientow: %d\n", ilosc_klientow);


    //sleep(5);
    ilosc_klientow = 30 - semctl(id_sem, 0, GETVAL);
    printf("Ilosc klientow: %d\n", ilosc_klientow);

    zamknij_sklep();
    zakoncz_program();

    return 0;
}
