#include "server.h"
#include "protocol.h"
#include "helpers.h"

#include <pthread.h>
#include <signal.h>


sem_t LOG_MUTEX;
int LOG_COUNT = 0;


void initializeMutexes() {
    if (sem_init(&LOG_MUTEX, 0, 1) != 0 ) {
        //ERROR 
    }


}

void log(char * text) {
    if (sem_wait(&LOG_MUTEX) != 0) {
        //ERROR
    }

    //Safe to write to log file and to change log count
    FILE * f = fopen("log.txt", (LOG_COUNT == 0) ? "w" : "a");
    fprintf(f,"%s\n", text);
    
    LOG_COUNT++;
    fclose(f);


    if (sem_post(&LOG_MUTEX) != 0) {
        //ERROR
    }

    
    


}
