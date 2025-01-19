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


key_t klucz = 21370; //ftok("pp", 127);
int id_sem; 
int id_kolejki;

struct msg_buffer{
	long mtype;
	long ktype;
	char mtext[MAX_TEXT];
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
        perror("Nie można otworzyć pliku");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s", tresc); //zawartosc do pliku
    fclose(file);
}

/*void pozar_alarm(int sig) {
    if (sig == SIGUSR1) {
        printf("Pozar! Klient %d ucieka", getpid());
        exit(0);
    }
}

void sighandler(int signum, siginfo_t *info, void *ptr)
{
        printf("Sygnal SIGINT!\n");
        return;
}*/

int main(int argc, char *argv[]) {
    //sleep(2);
    id_sem = semget(klucz, 3, 0600 | IPC_CREAT); //semaforki
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



    /*struct sigaction sa; //sygnal
    sa.sa_handler = pozar_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("blad ustawienia sygnalu");
        exit(EXIT_FAILURE);
    }*/
    



    if (semctl(id_sem, 2, GETVAL) == 0 ) { //sklep zamkniety, zabij sie (idz do innego sklepu)
        printf("\tSklep zamkniety. %d idzie sie zabic\n", getpid());
        raise(SIGTERM);
    }
    if (semctl(id_sem, 0, GETVAL) == 0) { //sklep przepelniony
        int co_zrobi = (rand() % 2) + 1;
        if (co_zrobi == 1) { //czekaj przed sklepem az bedzie miejsce, by wejsc do srodka
            printf("Sklep przepelniony. Klient %d postanawia czekac przed sklepem\n", getpid());
            while (semctl(id_sem, 2, GETVAL) == 0) sleep(1); 
            printf("Klient %d przeczekal.\n", getpid());
        } else { //nie warto czekac, zabij sie (idz gdzie indziej)
            printf("Sklep przepelniony. Klient %d postanawia pojsc gdzie indziej", getpid());
            raise(SIGTERM);
        }
    }


    if (semctl(id_sem, 2, GETVAL) == 1 && semctl(id_sem, 0, GETVAL) > 0) { //wejdz jak otwarty sklep i nie przelniony
        sem_p(0); //wejdz do sklepu
        printf("Klient %d wchodzi do sklepu. \t %d\\30 \n", getpid(), 30-semctl(id_sem, 0, GETVAL));
        srand(time(NULL));
        sleep((rand() % 11) + 1); //badz w sklepie
            
        //zapisz sie w ksiedze gosci
        sem_p(1);
        zapisz_sie("i");
        sem_v(1);

        //idz do kasy
        struct msg_buffer message;
        message.mtype = 1;  // Typ komunikatu
        //message.mtext = getpid();
        snprintf(message.mtext, MAX_TEXT, "%d", getpid());


        // Wysłanie komunikatu
        if (msgsnd(id_kolejki, &message, strlen(message.mtext) + 1, 0) == -1) {
            perror("blad wpisanie sie do kolejki");
            exit(EXIT_FAILURE);
        }

        printf("Wiadomość wysłana: %s\n", message.mtext);




        sem_v(0);
        printf("Klient %d wychodzi ze sklepu.\t %d\\30 \n", getpid(), 30-semctl(id_sem, 0, GETVAL));
        
    } 
        

        
    

    return 0;
}
