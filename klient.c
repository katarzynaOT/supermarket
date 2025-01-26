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

#define MAX_TEXT 20 //ilosc znakow do zapisu do kolejki

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_WHITE   "\x1B[37m"


key_t klucz = 21370; //ftok("pp", 127);
int id_sem; 
int id_kolejki;

struct msg_buffer{
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
        perror("K:nie mozna podniesc semafora");
        exit(EXIT_FAILURE);
    }
}

void zapisz_sie(const char *tresc) {
    FILE *file = fopen("ksiega_gosci.txt", "a"); //plik raport
    if (file == NULL) {
        perror("Nie można otworzyc pliku");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s", tresc); //zawartosc do pliku
    fclose(file);
}

void pozar_alarm(int sig) {
    if (sig == SIGINT) {
        printf(ANSI_COLOR_RED "\tPozar! Klient %d ucieka\n" ANSI_COLOR_RESET, getpid());
        sem_v(0);
        exit(0); //wyjdz ze sklepu - natychmiast
    }
}

int main(int argc, char *argv[]) {
    //sleep(2);
    id_sem = semget(klucz, 4, 0600 | IPC_CREAT); //semaforki
    if (id_sem == -1) {
        perror("blad tworzenia semaforow");
        exit(EXIT_FAILURE);
    }
    id_kolejki = msgget(klucz, 0600 | IPC_CREAT);
    if(id_kolejki == -1)
	{
		printf("Blad tworzenia kolejki");
		exit(EXIT_FAILURE);
	}

//////////////////////////////////////
    struct sigaction sa; //sygnal
    sa.sa_handler = pozar_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("blad ustawienia sygnalu");
        exit(EXIT_FAILURE);
    }
    
//sprobuj wejsc do sklepu
    if (semctl(id_sem, 2, GETVAL) == 0 ) { //sklep zamkniety, zabij sie (idz do innego sklepu)
        printf("\tSklep zamkniety. %d idzie sie zabic\n", getpid());
        raise(SIGTERM);
    }
    if (semctl(id_sem, 0, GETVAL) == 0) { //sklep przepelniony
//        int co_zrobi = (rand() % 2) + 1;
//        if (co_zrobi == 1) { //czekaj przed sklepem az bedzie miejsce, by wejsc do srodka
//            printf("Sklep przepelniony. Klient %d postanawia czekac przed sklepem\n", getpid());
//            while (semctl(id_sem, 2, GETVAL) == 0) sleep(1); 
//            printf("Klient %d przeczekal.\n", getpid());
//        } else { //nie warto czekac, zabij sie (idz gdzie indziej)
            printf("Sklep przepelniony. Klient %d postanawia pojsc gdzie indziej", getpid());
            raise(SIGTERM);
//        }
    }

//wejdz do sklepu
    if (semctl(id_sem, 2, GETVAL) == 1 && semctl(id_sem, 0, GETVAL) > 0) { //wejdz jak otwarty sklep i nie przelniony - ostateczne sprawdzenie - chyba niepotrezebne
        sem_p(0); //wejdz do sklepu
        printf(ANSI_COLOR_GREEN "Klient %d wchodzi do sklepu. \t\t %d\\30 \n" ANSI_COLOR_RESET, getpid(), 30-semctl(id_sem, 0, GETVAL));
printf(ANSI_COLOR_WHITE "");
        srand(time(NULL));
        sleep((rand() % 20) + 1); //badz w sklepie (od 1 do 11 sekund)
            
//zapisz sie w ksiedze gosci
        sem_p(1);
        pid_t pid = getpid();
        char pid_str[20]; 
        snprintf(pid_str, sizeof(pid_str), "%d\n", pid);//zamiana pid na stringsa
        zapisz_sie(pid_str);
        sem_v(1);

//idz do kasy
        struct msg_buffer message;
//losowanie kasy
        int kasa;
        int max_kasa = semctl(id_sem, 3, GETVAL); //liczba aktywnych kas
        kasa = rand() % max_kasa + 1; //losowanie kasy od 1 do max_kasa
        message.mtype = kasa; //przekazuje nr kasy, do ktorej idze klient
        snprintf(message.mtext, MAX_TEXT, "%d", getpid()); //przkazuje numer PID klienta do komunikatu
//wyslanie komunikatu - wejscie do kolejki
        if (msgsnd(id_kolejki, &message, strlen(message.mtext) + 1, 0) == -1) {
            perror("blad wpisanie sie do kolejki");
            exit(EXIT_FAILURE);
        }
        //printf("\tK: %s\n", message.mtext); //to co ponizej
        printf(ANSI_COLOR_YELLOW "Klient %d wchodzi do kolejki %d\n" ANSI_COLOR_RESET, getpid(), kasa);

//czekanie w kolejce
    //    while (msgrcv(id_kolejki, &message, sizeof(message.mtext), message.mtype, IPC_NOWAIT) != -1) {
    //        printf("Klient %d czeka w kolejce %d...\n", getpid(), kasa);
    //        sleep(1); 
    //    }

//zaplata
        printf(ANSI_COLOR_BLUE "Klient %d placi przy kasie - opuszcza kolejke %d\n" ANSI_COLOR_RESET, getpid(), kasa);

//wyjscie ze sklepu
        sem_v(0);
        printf(ANSI_COLOR_CYAN "Klient %d wychodzi ze sklepu \t\t %d\\30 \n" ANSI_COLOR_RESET, getpid(), 30-semctl(id_sem, 0, GETVAL));
        exit(0); //zakoncz zywot
    
    }
    return 0;
}
