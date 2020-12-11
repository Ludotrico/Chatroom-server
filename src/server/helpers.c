#include "server.h"
#include "protocol.h"
#include "helpers.h"

#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include "linkedList.h"
#include "debug.h"


pthread_mutex_t LOG_MUTEX;
int LOG_COUNT = 0;

extern char LOG_FILE[200];
extern int JOBS;

extern List_t USER_LIST;
extern pthread_mutex_t USER_MUTEX;

extern List_t ROOM_LIST;
extern pthread_mutex_t ROOM_MUTEX;

extern List_t JOB_LIST;
extern pthread_mutex_t JOB_MUTEX;
extern sem_t JOB_SEM;



void initializeLists() {
    USER_LIST.head = NULL;
    USER_LIST.length = 0;

    ROOM_LIST.head = NULL;
    ROOM_LIST.length = 0;

    JOB_LIST.head = NULL;
    JOB_LIST.length = 0;
}

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
    pthread_mutex_lock(&USER_MUTEX);
    debug("nameToCheckFor %s\n", nameToCheckFor);
    
    node_t * curr = userNameList->head;

    while (curr != NULL){
        debug("curr username %s\n", ((User*)curr->value)->username);
        if (strcmp(nameToCheckFor, ((User*)curr->value)->username) == 0){
            pthread_mutex_unlock(&USER_MUTEX);
            return false;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&USER_MUTEX);
    return true;
}

bool isValidRoom(char * roomToCheckFor, List_t *roomList){
    // return true if not found in list
    // return false if found in list
    pthread_mutex_lock(&ROOM_MUTEX);
    debug("room to check for %s\n", roomToCheckFor);
    
    node_t * curr = roomList->head;

    while (curr != NULL){
        debug("curr roomName %s\n", ((ChatRoom*)curr->value)->roomName);
        if (strcmp(roomToCheckFor, ((ChatRoom*)curr->value)->roomName) == 0){
            pthread_mutex_unlock(&ROOM_MUTEX);
            return false;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&ROOM_MUTEX);
    return true;
}


void spawnJobs() {
    int i;
    pthread_t tid;
    for (i = 0; i < JOBS; i++) {
        pthread_create(&tid, NULL, jobProcess, NULL);
    }
}

void * jobProcess() {
    JobProcess * currentJob = NULL;

    while (1) {
        sem_wait(&JOB_SEM);
        pthread_mutex_lock(&JOB_MUTEX);

        currentJob = removeFront(&JOB_LIST);

        pthread_mutex_unlock(&JOB_MUTEX);
        debug("job message: %s type: %d\n", currentJob->message, currentJob->header.msg_type);
        // Start processing current job
        if (currentJob->header.msg_type == LOGOUT) {
            // 10
            
        } else if (currentJob->header.msg_type == RMCREATE) {
            // 2
            createRoom(currentJob);

        } else if (currentJob->header.msg_type == RMDELETE) {
            // 6
        } else if (currentJob->header.msg_type == RMLIST) {
            // 7
        } else if (currentJob->header.msg_type == RMJOIN) {
            // 3
            
        } else if (currentJob->header.msg_type == RMLEAVE) {
            // 4
            
        } else if (currentJob->header.msg_type == RMSEND) {
            // 5
            
        } else if (currentJob->header.msg_type == USRSEND) {
            // 8
        } else if (currentJob->header.msg_type == USRLIST) {
            // 9
        }

        free(currentJob);
    }

    return NULL;
}

void createRoom(JobProcess* job){
    debug("%s\n", " ");
    if (isValidRoom(job->message, &ROOM_LIST)) {
        debug("%s\n", " ");
        // Room name is unique, initialize newRoom
        ChatRoom * newRoom = malloc(sizeof(ChatRoom));
        sprintf(newRoom->roomName, "%s", job->message);
        newRoom->creator = job->user;
        newRoom->users = NULL;
        debug("%s\n", " ");

        // LOCK room mutex and add to room list
        pthread_mutex_lock(&ROOM_MUTEX);
        insertFront(&ROOM_LIST, newRoom);
        pthread_mutex_unlock(&ROOM_MUTEX);

        debug("%s\n", " ");
        // Send OK back to client
        job->header.msg_type = OK;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);
        debug("room created\n");
        
    } else {
        debug("%s\n", " ");
        // Room name already exists
        job->header.msg_type = ERMEXISTS;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("Room already exists\n"); 
    }
}