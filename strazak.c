#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define HRED "\e[0;91m"
#define REDB "\e[41m"

key_t klucz = 21370; //ftok("pp", 127);
int id_sem;

//to ponizej niepotrzebne chyba
//void signal_handler(int sig) {
//    if (sig == SIGUSR1) {
//        printf("\tStrazakakakak\n");
//    }
//}

//to ponizej nie potrebne chyba
//void wyslij_sygnal(int pid, int sig) {
//    if (kill(pid, sig) == 0) {
//        printf("Wysłano sygnał %d do procesu %d\n", sig, pid);
//    } else {
//        perror("Nie udało się wysłać sygnału");
//    }
//}

int main(int argc, char* argv[]) {
    if (argc != 3) {
		perror("Zle uruchomiony strazak");
        exit(EXIT_FAILURE);
	}
    pid_t pid_menedzer = (pid_t)atoi(argv[1]);
//printf("Strazak: Pid menedzera: %d\n", pid_menedzer);
    pid_t pid_kierownik = (pid_t)atoi(argv[2]);
//printf("Strazak: Pid kierownika: %d\n", pid_kierownik);

    id_sem = semget(klucz, 4, 0600 | IPC_CREAT); //semaforki
    if (id_sem == -1) {
        perror("blad tworzenia semaforow");
        exit(EXIT_FAILURE);
    }

//    join_sem();
//    setbuf(stdout, NULL);

    //struct sigaction sa;
    //sa.sa_handler = signal_handler;
    //sigemptyset(&sa.sa_mask);
    //sa.sa_flags = 0;

    //if (sigaction(SIGUSR1, &sa, NULL) == -1) { //SIGTERM
    //    perror("blad ustawienia sygnalu");
    //    exit(EXIT_FAILURE);
    //}

    srand(time(NULL));
    sleep(3000);
//czekanie na pozar sllep();
    printf(HRED REDB "\tStraz pozarna: POZAR!!!" ANSI_COLOR_RESET);
    printf("\n");

//wyslanie sygnalu do menedzera
   	if (kill(pid_menedzer, SIGINT) == -1) {
        perror("Nie udalo sie wyslac SIGUSR1 do menedzera");
        exit(EXIT_FAILURE);
    }
//wyslanie sygnalu do sklepu
    if (kill(pid_kierownik, SIGINT) == -1) {
        perror("Nie udalo sie wyslac SIGUSR1 do kierownika");
        exit(EXIT_FAILURE);
    }
    char command[256];
    snprintf(command, sizeof(command), "pgrep %s", "kierownik");
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Nie udało się otworzyć strumienia");
        exit(EXIT_FAILURE);
    }
    char pid_str[16];
    while (fgets(pid_str, sizeof(pid_str), fp)) {
        pid_t pid = (pid_t)strtol(pid_str, NULL, 10); //konwersja pid na liczbe
        if (kill(pid, SIGINT) == -1) {
            perror("Nie udało się wysłać sygnału");
            exit(EXIT_FAILURE);
        }
    }
    pclose(fp);
    printf("\tok\n");

//wyslanie sygnalow do klientow
    FILE *file = fopen("ksiega_gosci.txt", "r");
    if (file == NULL) {
        perror("Nie można otworzyc pliku");
        exit(EXIT_FAILURE);
    }
    int liczba_klientow = 30 - semctl(id_sem, 0, GETVAL);
    long positions[liczba_klientow]; 
    int count = 0;  
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        long pos = ftell(file);
        positions[count % liczba_klientow] = pos - strlen(line) - 1; 
        count++;
        //positions[count % liczba_klientow] = ftell(file);
        //count++;
    }
    int lines_to_read = count < liczba_klientow ? count : liczba_klientow;
    int start = count > liczba_klientow ? count % liczba_klientow : 0;
    for (int i = 0; i < lines_to_read; i++) {
        int idx = (start + i) % liczba_klientow;
        fseek(file, positions[idx], SEEK_SET);
        if (fgets(line, sizeof(line), file)) {
            pid_t value = (pid_t)strtol(line, NULL, 10);
            //printf("%d -%d\n", value, liczba_klientow);
//wyslij sygnal
            if (kill(value, SIGINT) == -1) {
                perror("Nie udalo sie wyslac SIGUSR1 do klienta");
                exit(EXIT_FAILURE);
            }
        }
    }
    fclose(file);


//wyslanie sygnalu do sklepu

}
