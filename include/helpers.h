#include "linkedList.h"
#include "protocol.h"



typedef struct user {
    char username[BUFFER_SIZE];
    int fd;
} User;


typedef struct Room {
    char roomName[BUFFER_SIZE];
    User * creator;
    node_t * users;
} ChatRoom;

typedef struct Job {
    petr_header header;
    char message[BUFFER_SIZE];
} JobProcess;




void logText(char *);


