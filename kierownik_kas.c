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

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define MAX_TEXT 20 //ilosc znakow do zapisu do kolejki
#define DZIEN 30 //czas trwania jednego dnia
#define MAX_KASY 10 //maksymalna liczba aktywnych kas
#define KLIENCI_NA_KASE 5 //liczba klientow na kase - zmienna

int N; //liczba czynnych kas
int K; //liczba klientow w sklepie
key_t klucz = 21370; //ftok("pp", 127);

int id_sem; 
int id_kolejki;
struct msg_buffer {
	long mtype; //numer kasy
	char mtext[MAX_TEXT]; //kto podchodzi do kasy
};
struct dane_kasy {
    int id_msg;
    int id_nowej_kasy;
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
    struct dane_kasy* dane = (struct dane_kasy*)arg; // Rzutowanie wskaźnika void* na struct dane_kasy*
    int msg_id = dane->id_msg;
    int id_kasy = dane->id_nowej_kasy;

    struct msg_buffer message;
    printf("\tWatek %ld jest na kasie %d", pthread_self(), id_kasy);

    while ( semctl(id_sem, 3, GETVAL) == id_kasy ) { //dopoki kasa otwarta i ktos czeka w kolejce
//obsluga kllienta na kasie
        if (msgrcv(msg_id, &message, sizeof(message.mtext) - sizeof(long), id_kasy, 0) == -1) {
            perror("blad odczytania komuinkatu");
            exit(EXIT_FAILURE);
            //continue;
        }
        printf(ANSI_COLOR_MAGENTA "Kasa %d obsluguje klienta %s\n" ANSI_COLOR_RESET, id_kasy, message.mtext);       
        sleep((rand() % 11) + 1); ///czas obslugi jednego klienta (od 1 do 11)
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




    pthread_t kasy[MAX_KASY]; //pracownicy, ktorzy moga pracowac na kasach - istnieja, ale nie wszyscy obsluguja kase (niektorzy maja przerwy czy cos)
//ile kas potrzebujemy
    while (1) {
        int liczba_klientow = 30 - semctl(id_sem, 1, GETVAL);
        int liczba_kas = semctl(id_sem, 3, GETVAL);
        if ( liczba_klientow >= ((liczba_kas - 1) * KLIENCI_NA_KASE) && liczba_kas < 10 ) { //zwiekszenie liczby kas
            struct dane_kasy* dane = malloc(sizeof(struct dane_kasy));
            if (dane == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            dane->id_msg = id_kolejki;
            dane->id_nowej_kasy = liczba_kas + 1;
            if (pthread_create(&kasy[liczba_kas+1], NULL, obsluga_kasy, dane) != 0) { //pracownik idze na nowa kase
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            sem_v(3); //otworz nowa kase
        } else if ( liczba_klientow < ((liczba_kas - 1) * KLIENCI_NA_KASE) && liczba_kas > 2 ) { //zmniejszenie liczby kas
            sem_p(3); //zamykamy kase
            pthread_t pracownik;

        }

    }




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
