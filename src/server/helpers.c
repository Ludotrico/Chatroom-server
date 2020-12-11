#include "server.h"
#include "protocol.h"
#include "helpers.h"

#include <string.h>
#include <pthread.h>
#include <signal.h>


pthread_mutex_t LOG_MUTEX;
int LOG_COUNT = 0;

extern char LOG_FILE[200];



void logText(char * text) {
    pthread_mutex_lock(&LOG_MUTEX);


    //Safe to write to log file and to change log count
    FILE * f = fopen(LOG_FILE, (LOG_COUNT == 0) ? "w" : "a");
    fprintf(f,"%s\n", text);

    LOG_COUNT++;
    fclose(f);


    pthread_mutex_unlock(&LOG_MUTEX);
}

bool isValidUsername(char * nameToCheckFor, List_t *userNameList){
    // return true if not found in list
    // return false if found in list
    printf("nameToCheckFor %s\n", nameToCheckFor);
    
    node_t * curr = userNameList->head;

    while (curr != NULL){
        printf("curr username %s\n", ((User*)curr->value)->username);
        if (strcmp(nameToCheckFor, ((User*)curr->value)->username) == 0){
            return false;
        }
        curr = curr->next;
    }
    return true;
}