#include "linkedList.h"
#include "protocol.h"



typedef struct user {
    char username[BUFFER_SIZE];
    int fd;
} User;


typedef struct Room {
    char roomName[BUFFER_SIZE];
    User * creator;
    List_t * users;
} ChatRoom;

typedef struct Job {
    petr_header header;
    char message[BUFFER_SIZE];
    User * user;
} JobProcess;


void initializeLists();
void spawnJobs();
void logText(char *);
bool isValidUsername(char *, List_t *);
void * jobProcess();
void logout(JobProcess *);
void createRoom(JobProcess *);
void deleteRoom(JobProcess *);
void listUsersInRoom(JobProcess *);
void joinRoom(JobProcess *);
void leaveRoom(JobProcess *);
void sendMessageToRoom(JobProcess *);
void sendMessageToUser(JobProcess *);
void listUsers(JobProcess *);

int isUserInRoom(List_t* users, char* name);



