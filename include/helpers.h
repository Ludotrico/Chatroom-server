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


extern FILE * LOG_FILE;


#define logText(S, ...)                                                       \
    fprintf(LOG_FILE, "%s:%s:%d " S, __FILE__,  __FUNCTION__, __LINE__, ##__VA_ARGS__)




void initializeLists();
void spawnJobs();

bool isValidUsername(char *, List_t *);
void * jobProcess();
void logout(JobProcess *);
void createRoom(JobProcess *);
void deleteRoom(JobProcess *);
void listUsersInRooms(JobProcess *);
void joinRoom(JobProcess *);
void leaveRoom(JobProcess *);
void sendMessageToRoom(JobProcess *);
void sendMessageToUser(JobProcess *);
void listUsers(JobProcess *);

int isUserInRoom(List_t* users, char* name);
char* printDatetime();





