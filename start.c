//#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>

#define MAX_AKTYWNYCH_KLIENTOW 20 //klienci w sklepie i klienci czekajacy przed sklepem, ktorzy sobie jeszcze nie poszli

volatile sig_atomic_t pozar = 0;

key_t klucz = 21370; //ftok("pp", 127);
int id_sem; 
int id_kolejki;

//usun semfory i kolejki
void zakoncz_program() {
    //sleep(1);
    printf("\n\t KONCZE PROGRAM...\n");
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
    printf("\t\tKONIEC PROGRAMU\n");
    exit(0);
}

void pozar_alarm(int sig) {
    printf("men\n");
    if (sig == SIGUSR1) {
        printf("\tJestem sobie menedzer - pozar\n");
    }
    semctl(id_sem, 2, GETVAL) == 0; //zablokuj sklep - powiedz ochroniarzowi, by nikogo nie wpuszczal
    //while (wait(NULL) > 0);
    //zakoncz_program();
    //printf("");
}

int main() {
    id_sem = semget(klucz, 4, 0600 | IPC_CREAT); //semaforki
    if (id_sem == -1) {
        perror("blad tworzenia semaforow");
        exit(EXIT_FAILURE);
    }
    id_kolejki = msgget(klucz, 0600 | IPC_CREAT);
    if(id_kolejki == -1) {
		printf("Blad tworzenia kolejki komunikatow.\n");
		exit(EXIT_FAILURE);
	}

    struct sigaction sa;
    sa.sa_handler = pozar_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;


    pid_t kierownik_id = fork();
    if (kierownik_id == -1) {
        perror("nieudany fork dla kierownika");
        exit(EXIT_FAILURE);
    } else if (kierownik_id == 0) { //wlacz kierownika sklepu
        printf("WLACZONY KIEROWNIK\n");
        execlp("./kierownik", "kierownik", NULL);
        perror("nie udany execlp dla kierownika");
        exit(EXIT_FAILURE);
    }

    //sleep(1);
    pid_t menedzer_id = getpid();
    pid_t strazak_id = fork();
    if (strazak_id == -1) {
        perror("nieudany fork dla strazaka");
        exit(EXIT_FAILURE);
    } else if (strazak_id == 0) { //wlacz strazaka
        char menedzer_id_str[20]; // Bufor na ciąg znaków, wystarczająco duży, aby pomieścić PID
        char kierownik_id_str[20];
        snprintf(menedzer_id_str, sizeof(menedzer_id_str), "%d", menedzer_id);
        snprintf(kierownik_id_str, sizeof(kierownik_id_str), "%d", kierownik_id);
        printf("WLACZONY STRAZAK\n");
        execlp("./strazak", "strazak", menedzer_id_str, kierownik_id_str, NULL);
        perror("nie udany execlp dla strazaka");
        exit(EXIT_FAILURE);
    }
//V1
/*    sleep(1);
    srand(time(NULL));
    int aktywni_klienci;
    for (aktywni_klienci = 0; aktywni_klienci < MAX_AKTYWNYCH_KLIENTOW; aktywni_klienci ++){
        if (semctl(id_sem, 2, GETVAL) == 1 ) { //sklep otwarty
printf("\tAKTYWNI KLIENCI = %d\n", aktywni_klienci);
            pid_t klient_id = fork();
            if (klient_id == -1) {
                perror("nieudany fork dla klienta");
                exit(EXIT_FAILURE);
            } else if (klient_id == 0) { 
                //printf("WLACZONY KLIENT\n");
                //aktywni_klienci ++;
                execlp("./klient", "klient", NULL);
                perror("nie udany execlp dla klienta");
                exit(EXIT_FAILURE);
            }
            while (waitpid(-1, NULL, WNOHANG) > 0); //zabiecie zombie???
            sleep(2); //produkuj klientwo co 2 sekundy
            //sleep((rand() % 11) + 1); //od 1 do 11
        }
    }*/
//V2
    sleep(1);
    srand(time(NULL));
    int aktywni_klienci = 0;
    while ( aktywni_klienci < MAX_AKTYWNYCH_KLIENTOW ) { //+czy koniec dnia? //&& !pozar
        if (semctl(id_sem, 2, GETVAL) == 1 && !pozar ) { //czy sklep otwarty
            pid_t klient_id = fork();
            if (klient_id == -1) {
                perror("nieudany fork dla klienta");
                exit(EXIT_FAILURE);
            } else if (klient_id == 0) { 
                //printf("WLACZONY KLIENT\n");
                aktywni_klienci ++;
                execlp("./klient", "klient", NULL);
                perror("nie udany execlp dla klienta");
                exit(EXIT_FAILURE);
            }
            while (waitpid(-1, NULL, WNOHANG) > 0) aktywni_klienci--; //zabicie zombie
            sleep(2); ////produkuj klientow co ilestam sekund

            //powiedz, ze koniec dnia
        }
    }

    printf("m\n");
    while (wait(NULL) > 0); //rodzic czeka na ukonczenie wszystkich potomkow - by poprawnie bylo

    while (waitpid(-1, NULL, WNOHANG) > 0);
    //zbieranie zobmiakow wersja 2
    //while (waitpid(-1, NULL, WNOHANG) > 0)
	//	active_clients--;

    return 0;
}
