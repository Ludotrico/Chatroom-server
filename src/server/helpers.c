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
    debug("nameToCheckFor %s\n", nameToCheckFor);
    
    node_t * curr = userNameList->head;

    while (curr != NULL){
        debug("curr username %s\n", ((User*)curr->value)->username);
        if (strcmp(nameToCheckFor, ((User*)curr->value)->username) == 0){
            return false;
        }
        curr = curr->next;
    }
    return true;
}

User* getUser(char * nameToCheckFor, List_t *userNameList){
    // return true if not found in list
    // return false if found in list
    debug("nameToCheckFor %s\n", nameToCheckFor);
    
    node_t * curr = userNameList->head;

    while (curr != NULL){
        debug("curr username %s\n", ((User*)curr->value)->username);
        if (strcmp(nameToCheckFor, ((User*)curr->value)->username) == 0){
            return ((User*) curr->value);
        }
        curr = curr->next;
    }
    return NULL;
}

int getUserIndex(char * nameToCheckFor, List_t *userNameList){
    // return true if not found in list
    // return false if found in list
    debug("nameToCheckFor %s\n", nameToCheckFor);
    
    node_t * curr = userNameList->head;
    int i = 0;

    while (curr != NULL){
        debug("curr username %s\n", ((User*)curr->value)->username);
        if (strcmp(nameToCheckFor, ((User*)curr->value)->username) == 0){
            return i;
        }
        i++;
        curr = curr->next;
    }
    return -1;
}


ChatRoom * isValidRoom(char * roomToCheckFor, List_t *roomList){
    // return true if not found in list
    // return false if found in list
    debug("room to check for %s\n", roomToCheckFor);
    
    node_t * curr = roomList->head;

    while (curr != NULL){
        debug("curr roomName %s\n", ((ChatRoom*)curr->value)->roomName);
        if (strcmp(roomToCheckFor, ((ChatRoom*)curr->value)->roomName) == 0){
            return ((ChatRoom*)curr->value);
        }
        curr = curr->next;
    }
    return NULL;
}

int getRoomIndex(char * roomToCheckFor, List_t *roomList){
    // return true if not found in list
    // return false if found in list
    debug("room to check for %s\n", roomToCheckFor);
    
    node_t * curr = roomList->head;
    int index = 0;

    while (curr != NULL){
        debug("curr roomName %s\n", ((ChatRoom*)curr->value)->roomName);
        if (strcmp(roomToCheckFor, ((ChatRoom*)curr->value)->roomName) == 0){
            return index;
        }
        index++;
        curr = curr->next;
    }
    return -1;
}


int isUserInRoom(List_t* users, char* name){
    // returning int of index on success
    // returning -1 upon failure
    
    debug("user to look for: %s\n", name);

    node_t * curr = users->head;
    int index = 0;

    while (curr != NULL){
        debug("curr userName %s\n", ((User*)curr->value)->username);
        if (strcmp(name, ((User*)curr->value)->username) == 0){
            return index;
        }
        index++;
        curr = curr->next;
    }

    return -1;
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
            logout(currentJob);
        } else if (currentJob->header.msg_type == RMCREATE) {
            // 2
            createRoom(currentJob);

        } else if (currentJob->header.msg_type == RMDELETE) {
            // 6
            deleteRoom(currentJob);
        } else if (currentJob->header.msg_type == RMLIST) {
            // 7
            debug("%s", " ");
            listUsersInRooms(currentJob);
        } else if (currentJob->header.msg_type == RMJOIN) {
            // 3
            joinRoom(currentJob);
            
        } else if (currentJob->header.msg_type == RMLEAVE) {
            // 4
            leaveRoom(currentJob);
            
        } else if (currentJob->header.msg_type == RMSEND) {
            // 5
            sendMessageToRoom(currentJob);
            
        } else if (currentJob->header.msg_type == USRSEND) {
            // 8
            sendMessageToUser(currentJob);
        } else if (currentJob->header.msg_type == USRLIST) {
            // 9
            listUsers(currentJob);
        }
        debug("About to free job\n");
        free(currentJob);
    }

    return NULL;
}

void createRoom(JobProcess* job){
    debug("%s\n", " ");
    pthread_mutex_lock(&ROOM_MUTEX);
    pthread_mutex_lock(&USER_MUTEX);
    ChatRoom * room = isValidRoom(job->message, &ROOM_LIST); 

    if (!room) {
        debug("%s\n", " ");
        // Room name is unique, initialize newRoom
        ChatRoom * newRoom = malloc(sizeof(ChatRoom));

        sprintf(newRoom->roomName, "%s", job->message);
        newRoom->creator = job->user;

        // Initialize room users
        List_t * joinedUsers = malloc(sizeof(List_t));
        joinedUsers->head = NULL;
        joinedUsers->length = 0;

        // Inser creator in room users
        insertFront(joinedUsers, job->user);

        newRoom->users = joinedUsers;
        debug("%s\n", " ");

        insertFront(&ROOM_LIST, newRoom);

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
    pthread_mutex_unlock(&USER_MUTEX);
    pthread_mutex_unlock(&ROOM_MUTEX);
}


void joinRoom(JobProcess * job) {
    pthread_mutex_lock(&ROOM_MUTEX);
    pthread_mutex_lock(&USER_MUTEX);
    ChatRoom * room = isValidRoom(job->message, &ROOM_LIST);

    if (!room) {
        job->header.msg_type = ERMNOTFOUND;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("%s\n", " ");
    } else {
        // Room name exists

        // If user is in room just return OK
        if (isUserInRoom(room->users, job->user->username) != -1) {
            // Send OK to client
            job->header.msg_type = OK;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            pthread_mutex_unlock(&USER_MUTEX);
            pthread_mutex_unlock(&ROOM_MUTEX);
            return;
        }
               
        //Check if it is full
        if (room->users->length >= 5) {
            // Room is full
            job->header.msg_type = ERMFULL;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            debug("Room is full!\n"); 
        } else {
            // Add user to room
           
            insertFront(room->users, job->user);
            
            // Send OK to clinet
            job->header.msg_type = OK;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            debug("User added to room\n");
        }

    }
    pthread_mutex_unlock(&USER_MUTEX);
    pthread_mutex_unlock(&ROOM_MUTEX);
}

void leaveRoom(JobProcess * job) {
    pthread_mutex_lock(&ROOM_MUTEX);
    pthread_mutex_lock(&USER_MUTEX);
    ChatRoom * room = isValidRoom(job->message, &ROOM_LIST);
    
    if (!room) {
        job->header.msg_type = ERMNOTFOUND;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("Room does not exist\n");
    } else {
        // Room found

        // Check if user is creator of room
        if (room->creator->fd == job->user->fd) {
            job->header.msg_type = ERMDENIED;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            debug("Creator of room cannot leave!\n");
            pthread_mutex_unlock(&USER_MUTEX);
            pthread_mutex_unlock(&ROOM_MUTEX);
            return;
        }


        // Check if user is in the room
        int userIndex = isUserInRoom(room->users, &(job->user->username[0]));
        if (userIndex != -1) {
            // User is in room

            // Remove user 
            
            removeByIndex(room->users, userIndex);
            

            // Send client success
            job->header.msg_type = OK;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            debug("User successfull removed from room\n");
            
        } else {
            // User is not in room
            job->header.msg_type = OK;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            debug("User is not in room!\n");
        }
    }
    pthread_mutex_unlock(&USER_MUTEX);
    pthread_mutex_unlock(&ROOM_MUTEX);
}


void sendMessageToRoom(JobProcess * job) {
    char * tmp = strchr(job->message, '\r');
    *tmp = '\0';
    char * roomName = job->message;
    tmp += 2;
    char * message = tmp;

    char buffer[BUFFER_SIZE*3];
    bzero(buffer, BUFFER_SIZE*3);

    pthread_mutex_lock(&ROOM_MUTEX);
    pthread_mutex_lock(&USER_MUTEX);

    int totalBytes = strlen(roomName) + strlen(job->user->username) + strlen(message) + 4;
    int bytes = sprintf(buffer, "%s\r\n%s\r\n%s", roomName, job->user->username, message);
    debug("buffer: %s, bytes: %d\n", buffer, bytes+1);


    debug("roomname: %s, message: %s\n", roomName, message);

    ChatRoom * room = isValidRoom(job->message, &ROOM_LIST);

    if (!room) {
        job->header.msg_type = ERMNOTFOUND;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("Room does not exist!\n");
    } else {
        // Room exists

        // Check if user is in room
        if (isUserInRoom(room->users, job->user->username) != -1) {
            debug("%s\n", " ");
            // User is in the room
            // Send message to all users in the room
            node_t * currentUser = room->users->head;

            while (currentUser != NULL) {
                debug("%s\n", " ");
                User * user = (User *) currentUser->value;
                if (user->fd != job->user->fd) {
                   // Send message to user
                   // roomname<\r\n>sender<\r\n>message
                    job->header.msg_type = RMRECV;
                    job->header.msg_len = bytes + 1;

                    wr_msg(user->fd, &(job->header), buffer);

                    debug("Sent chatRoom message to %s\n", user->username);
                }
                currentUser = currentUser->next;
            }

            // Send OK back to sender
            debug("%s\n", " ");
            job->header.msg_type = OK;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            debug("Message sent to all users in room\n");


        } else {
            // User is not in the room
            debug("%s\n", " ");
            job->header.msg_type = ERMDENIED;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            debug("User is not in room!\n");
        }
    }
    pthread_mutex_unlock(&USER_MUTEX);
    pthread_mutex_unlock(&ROOM_MUTEX);

}

void deleteRoom(JobProcess * job) {
    pthread_mutex_lock(&ROOM_MUTEX);
    pthread_mutex_lock(&USER_MUTEX);
    ChatRoom * room = isValidRoom(job->message, &ROOM_LIST);
    
    if (!room) {
        job->header.msg_type = ERMNOTFOUND;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("Room does not exist!\n");
    } else {
        if (room->creator->fd != job->user->fd) {
            // ERROR not the creator of room

            job->header.msg_type = ERMDENIED;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            debug("User cannot delete room!\n");
        } else {

            node_t * currentUser = room->users->head;

            while (currentUser != NULL) {
                debug("%s\n", " ");
                User * user = (User *) currentUser->value;
                if (user->fd != job->user->fd) {
                    // Send RMClosed to user

                    job->header.msg_type = RMCLOSED;
                    job->header.msg_len = strlen(room->roomName) + 1;

                    wr_msg(user->fd, &(job->header), room->roomName);

                    debug("Sent RMClosed to %s\n", user->username);
                }

                currentUser = currentUser->next;
            }

            // Send OK back to sender
            debug("%s\n", " ");
            job->header.msg_type = OK;
            job->header.msg_len = 0;
            wr_msg(job->user->fd, &(job->header), NULL);

            // Delete the room
            int roomIndex = getRoomIndex(job->message, &ROOM_LIST);            
            removeByIndex(&ROOM_LIST, roomIndex);

            debug("%s\n", "Room cleanup");
            deleteList(room->users);
            free(room->users);
            free(room);

            debug("%s\n", "Room closed");


        }
    }
    pthread_mutex_unlock(&USER_MUTEX);
    pthread_mutex_unlock(&ROOM_MUTEX);
}

void listUsersInRooms(JobProcess * job)  {
    debug("%s", " ");
    pthread_mutex_lock(&ROOM_MUTEX);
    pthread_mutex_lock(&USER_MUTEX);

    if (ROOM_LIST.length == 0) {
        // Send RMLIST back to sender
        debug("%s\n", " ");
        job->header.msg_type = RMLIST;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("No rooms on server\n");
        pthread_mutex_unlock(&USER_MUTEX);
        pthread_mutex_unlock(&ROOM_MUTEX);
        return;
    }
    node_t * currentRoom = ROOM_LIST.head;
    char buffer[BUFFER_SIZE*3];
    char * tmp = &buffer[0];

    while (currentRoom != NULL) {
        ChatRoom * room = (ChatRoom *) currentRoom->value;
        sprintf(tmp, "%s: ", room->roomName);
        tmp += strlen(room->roomName) + 2;

        node_t * currentUser = room->users->head;
        while (currentUser != NULL) {
            User * user = (User *) currentUser->value;
            sprintf(tmp, "%s,", user->username);
            tmp += strlen(user->username) + 1;

            currentUser = currentUser->next;
        }
        tmp--;
        *tmp = '\n';
        tmp++;

        currentRoom = currentRoom->next;
    }

    debug("buffer size: %d buffer: %s", (int) ((tmp+1) - (&buffer[0])), buffer);

    // Send RMLIST back to sender
    job->header.msg_type = RMLIST;
    job->header.msg_len = ((tmp+1) - (&buffer[0]));
    wr_msg(job->user->fd, &(job->header), buffer);

    debug("Rmlist sent\n");

    /*
    Room1: user1, user2
    Room2: user3, user4

    Room1:user1,user2\nRoom2:user3,user4\n\0
    */

    pthread_mutex_unlock(&USER_MUTEX);
    pthread_mutex_unlock(&ROOM_MUTEX);
}

void sendMessageToUser(JobProcess * job) {
    char * tmp = strchr(job->message, '\r');
    *tmp = '\0';
    char * username = job->message;
    tmp += 2;
    char * message = tmp;

    char buffer[BUFFER_SIZE*3];
    bzero(buffer, BUFFER_SIZE*3);

    int bytes = sprintf(buffer, "%s\r\n%s", job->user->username, message);
    debug("buffer: %s, bytes: %d\n", buffer, bytes+1);

    pthread_mutex_lock(&USER_MUTEX);
    User * recipient = getUser(username, &USER_LIST);
    
    if (recipient) {
        //User exists
        // Send message to recipient 
        job->header.msg_type = USRRECV;
        job->header.msg_len = bytes + 1;
        wr_msg(recipient->fd, &(job->header), buffer);

        // Send OK by to client
        job->header.msg_type = OK;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("DM sent\n");
    } else {
        //ERROR user does not exist
        job->header.msg_type = EUSRNOTFOUND;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("User not found\n");
    }
    pthread_mutex_unlock(&USER_MUTEX);

}

void listUsers(JobProcess * job) {
    pthread_mutex_lock(&USER_MUTEX);
    if (USER_LIST.length == 1) {
        // ONly one user on server
        job->header.msg_type = USRLIST;
        job->header.msg_len = 0;
        wr_msg(job->user->fd, &(job->header), NULL);

        debug("Only one user one server\n");
    } else {
        char buffer[BUFFER_SIZE*3];
        char * tmp = &buffer[0];

        node_t * currentUser = USER_LIST.head;
        while (currentUser != NULL) {
            User * user = (User *) currentUser->value;
            if (user->fd != job->user->fd) {
                sprintf(tmp, "%s\n", user->username);
                tmp += strlen(user->username) + 1;
            }

            currentUser = currentUser->next;
        }
        tmp--;
        *tmp = '\n';
        tmp++;


        debug("buffer size: %d buffer: %s", (int) ((tmp+1) - (&buffer[0])), buffer);

        // Send RMLIST back to sender
        job->header.msg_type = USRLIST;
        job->header.msg_len = (tmp+1) - (&buffer[0]);
        wr_msg(job->user->fd, &(job->header), buffer);

        debug("User list sent\n");
    
    }

    pthread_mutex_unlock(&USER_MUTEX);
}


void logout(JobProcess * job) {
    // Find all chat rooms that were created by user
    debug("%s\n", "");
    pthread_mutex_lock(&ROOM_MUTEX);
    pthread_mutex_lock(&USER_MUTEX);

    node_t * currentRoom = ROOM_LIST.head;
    int i = 0;
    while (currentRoom != NULL) {
        debug("%s\n", "");
        ChatRoom * room = (ChatRoom *) currentRoom->value;
        currentRoom = currentRoom->next;
        if (room->creator->fd == job->user->fd) {
            // Delete Room and send RMCLOSED to users within
            removeByIndex(&ROOM_LIST, i);

            node_t * currentUser = room->users->head;

            while (currentUser != NULL) {
                debug("%s\n", " ");
                User * user = (User *) currentUser->value;
                if (user->fd != job->user->fd) {
                    // Send RMClosed to user

                    job->header.msg_type = RMCLOSED;
                    job->header.msg_len = strlen(room->roomName) + 1;

                    wr_msg(user->fd, &(job->header), room->roomName);

                    debug("Sent RMClosed to %s\n", user->username);
                }

                currentUser = currentUser->next;
            }   
        } else {
            i++;
        }
        
    }
 


    // Send OK by to client
    job->header.msg_type = OK;
    job->header.msg_len = 0;
    wr_msg(job->user->fd, &(job->header), NULL);

    close(job->user->fd);

    // Remove user from USER_LIST
    int userIndex = getUserIndex(job->user->username, &USER_LIST);
    removeByIndex(&USER_LIST, userIndex);
    free(job->user);

    debug("Logout success\n");

    pthread_mutex_unlock(&USER_MUTEX);
    pthread_mutex_unlock(&ROOM_MUTEX);
}