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
#define ANSI_COLOR_WHITE   "\x1B[37m"

#define MAX_TEXT 20 //ilosc znakow do zapisu do kolejki
#define DZIEN 100 //czas trwania jednego dnia
#define MAX_KASY 10 //maksymalna liczba aktywnych kas
#define KLIENCI_NA_KASE 5 //liczba klientow na kase - zmienna

int N; //liczba czynnych kas
int K; //liczba klientow w sklepie
key_t klucz = 21370; //ftok("pp", 127);

volatile sig_atomic_t pozar = 0; //do powiadomienia sklepu o pozarze

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
    if (semctl(id_sem, 0, SETVAL, 30) == -1) { //semafor ochroniarz 1 - ilosc klientow (odpowiada za to, by sklep sie nie przepelnil)
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
    if (semctl(id_sem, 3, SETVAL, 2) == -1) { //ilosc aktywnych kas (na poczatku 2)
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
    printf("\tDecyzja o zamknieciu sklepu\n");
    sem_p(2); //ochroniarz pilnuje wejscia, by nikt nie wszedl
    while(1) { //czekam na obsluge pozostalych klietnow
        sleep(1);
        if(semctl(id_sem, 0, GETVAL) == 30) { //czy wszyscy klienci wyszli
            printf("\nNikogo nie ma w sklepie\n");
            //semctl(id_sem, 0, SETVAL, 0);
            //printf("Nikogo nie ma na kasach\n");
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
        pid_t b = fork();
        if(b == -1) {
            perror("fork error");
            exit(EXIT_FAILURE);
        } else if (b == 0) {
            close(potok1[1]);
 	        dup2(potok1[0], 0);
            close(potok1[0]);
            sleep(1);
 		    execlp("wc", "wc", "-w", NULL);
 	    	perror("execlp 'wc' error");
		    exit(EXIT_FAILURE);
        }
    }
 			
}

//usun semfory i kolejki
void zakoncz_program() {
    //sleep(1);
    printf("\n\t KONCZE PROGRAM... %d\n", getpid());
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
    printf("\t\tKONIEC\n");
    exit(0);
}

//pozar
void pozar_alarm(int sig) {
    if (sig == SIGUSR1) {
        pozar = 1;
        printf("\tPracownik %d sklepu wie, ze jest pozar\n", getpid());
        sem_p(3); //opusc swoje stanowisko kasowwe
        while(1) { //czekam na ewakuacje klientow
            sleep(1);
            //printf("Pracownik %d czeka na wyjscie klietnow\n", getpid());
            if(semctl(id_sem, 0, GETVAL) == 30) { //czy wszyscy klienci wyszli
                printf("\nNikogo nie ma w sklepie -> zamknij kasy\n");
                semctl(id_sem, 0, SETVAL, 0);
                //printf("Nikogo nie ma na kasach\n");
                break;
            }
        }
        printf("\tPracownik (z kasy) %d ucieka ze sklepu.\n", getpid());
        zakoncz_program();
        exit(0); //uciekaj jak nie ma juz klientow w sklepie
    }

}


//komentarze zostac i w raporcie je rozszerzyc jeszzce
//min 4 testy
//interfejs, github, kod podzielony na procedury
//powinno dzialac bez sleep
//obsluga bledu - perror
//obsluga wyjatkow - przechwycenie bledu-wyjatku i robienie cso z nim dalej


void *obsluga_kasy(void* arg) {
    struct dane_kasy* dane = (struct dane_kasy*)arg; // Rzutowanie wskaźnika void* na struct dane_kasy*
    int msg_id = dane->id_msg; //id kolejki
    int id_kasy = dane->id_nowej_kasy;

    struct msg_buffer message;
//int liczba_klientow = 30 - semctl(id_sem, 0, GETVAL);
//int liczba_kas = semctl(id_sem, 3, GETVAL);
//int wzor = (liczba_kas - 1) * KLIENCI_NA_KASE;
//printf(ANSI_COLOR_WHITE "\t\tIV. kl = %d, ka = %d, wz = %d\n", liczba_klientow, liczba_kas, wzor);
    printf(ANSI_COLOR_WHITE "\tWatek %ld jest na kasie %d\n", pthread_self(), id_kasy);
    sleep(2);

    while ( semctl(id_sem, 3, GETVAL) <= id_kasy && !pozar) { //dopoki kasa otwarta i nie ma pozaru (i ktos czeka w kolejce - to do)
//obsluga kllienta na kasie
        if (msgrcv(msg_id, &message, sizeof(message.mtext) - sizeof(long), id_kasy, 0) == -1) {
            perror("blad odczytania komuinkatu");
            exit(EXIT_FAILURE);
        }
        printf(ANSI_COLOR_MAGENTA "Kasa %d obsluguje klienta %s\n" ANSI_COLOR_RESET, id_kasy, message.mtext);       
        sleep((rand() % 11) + 1); ///czas obslugi jednego klienta (od 1 do 11)
        printf("d2 = %d\n", pozar);
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

    struct sigaction sa; //sygnal
    sa.sa_handler = pozar_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("blad ustawienia sygnalu");
        exit(EXIT_FAILURE);
    }
    
    if (fork() == 0) {
        sleep(DZIEN);
        zamknij_sklep();
        zakoncz_program();
        exit(0);
    }

    pthread_t kasy[MAX_KASY]; //pracownicy, ktorzy moga pracowac na kasach - istnieja, ale nie wszyscy obsluguja kase (niektorzy maja przerwy czy cos)

    otworz_sklep();
    int ilosc_klientow = 30 - semctl(id_sem, 0, GETVAL);
    printf("Ilosc klientow: %d\n", ilosc_klientow);
    printf("Mamy %d otwartych kas\n", semctl(id_sem, 3, GETVAL));

    for (int i = 0; i < 2; i++) { //otworzenie dwoch poczatkowych kas, przy otwarciu sklepu
        struct dane_kasy* dane = malloc(sizeof(struct dane_kasy));
        if (dane == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        dane->id_msg = id_kolejki;
        dane->id_nowej_kasy = i;
//int liczba_klientow = 30 - semctl(id_sem, 0, GETVAL);
//int liczba_kas = semctl(id_sem, 3, GETVAL);
//int wzor = (liczba_kas - 1) * KLIENCI_NA_KASE;
//printf(ANSI_COLOR_WHITE "\t\tII. kl = %d, ka = %d, wz = %d\n", liczba_klientow, liczba_kas, wzor);
        if (pthread_create(&kasy[i], NULL, obsluga_kasy, dane) != 0) { //pracownik idze na nowa kase (0,1)
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    sleep(2);


//ile kas potrzebujemy - sprawdzamy dopoki sklep otwarty 
    while (semctl(id_sem, 2, GETVAL) != 0 && !pozar) { //dopoki sklep otwarty && nie ma pozaru
        int liczba_klientow = 30 - semctl(id_sem, 0, GETVAL);
        int liczba_kas = semctl(id_sem, 3, GETVAL);
        int wzor = (liczba_kas - 1) * KLIENCI_NA_KASE;
//printf(ANSI_COLOR_WHITE "\t\tI. kl = %d, ka = %d, wz = %d\n", liczba_klientow, liczba_kas, wzor);
//zwiekszenie liczby kas
        if ( (liczba_klientow > wzor) && (liczba_kas < 10) ) { 
//printf(ANSI_COLOR_WHITE "\tZwieksz kasy, pp = %d, p = %d\n", (liczba_kas - 1) * KLIENCI_NA_KASE, (liczba_klientow > ((liczba_kas - 1) * KLIENCI_NA_KASE)));
sleep(1);
            struct dane_kasy* dane = malloc(sizeof(struct dane_kasy));
            if (dane == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            dane->id_msg = id_kolejki;
            dane->id_nowej_kasy = liczba_kas;
//printf(ANSI_COLOR_WHITE "\t\tIII. kl = %d, ka = %d, wz = %d\n", liczba_klientow, liczba_kas, wzor);
            if (pthread_create(&kasy[liczba_kas], NULL, obsluga_kasy, dane) != 0) { //pracownik idze na nowa kase
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            sem_v(3); //otworz nowa kase
//zmniejszenie liczby kas
        } else if ( (liczba_klientow < wzor) && (liczba_kas > 2) ) { 
//printf(ANSI_COLOR_WHITE "\tZmniejsz kasy, pp = %d, p = %d\n", (liczba_kas - 1) * KLIENCI_NA_KASE, (liczba_klientow > ((liczba_kas - 1) * KLIENCI_NA_KASE)));
sleep(1);            
            sem_p(3); //zamykamy kase
            pthread_t pracownik;

        }
        sleep(1);
    }

//obslugiwanie kas po zamknieciu sklepu - czekanie na zakonczenie watkow
    int liczba_kas = semctl(id_sem, 3, GETVAL);
    for (int i = 0; i < liczba_kas; i++) {
        pthread_join(kasy[i], NULL);
    }
    
    ilosc_klientow = 30 - semctl(id_sem, 0, GETVAL);
    printf("Ilosc klientow: %d\n", ilosc_klientow);

    //czy potrzebne?
    while (wait(NULL) > 0); //rodzic czeka na ukonczenie wszystkich potomkow

    return 0;
}
