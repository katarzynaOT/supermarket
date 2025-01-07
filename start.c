//#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


int main() {
    if (fork() == 0) {
        execlp("./kierownik", "./kierownik", NULL);
        perror("nie udany execlp");
        exit(EXIT_FAILURE);
    }

    sleep(2);
    if (fork() == 0) {
        execlp("./klient", "./klient", NULL);
        perror("nie udany execlp");
        exit(EXIT_FAILURE);
    }

    wait(NULL);

    return 0;
}
