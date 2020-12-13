#include "server.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include "linkedList.h"
#include "helpers.h"
#include <semaphore.h>
#include "debug.h"


int listen_fd;


// Globals

unsigned int JOBS = 4;
char LOG_FILE[200];



List_t USER_LIST;
pthread_mutex_t USER_MUTEX;

List_t ROOM_LIST;
pthread_mutex_t ROOM_MUTEX;

List_t JOB_LIST;
pthread_mutex_t JOB_MUTEX;
sem_t JOB_SEM;




void sigint_handler(int sig) {
    printf("shutting down server\n");
    close(listen_fd);
    exit(0);
}

int server_init(int server_port) {
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(EXIT_FAILURE);
    } else
        printf("Socket successfully created\n");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed\n");
        exit(EXIT_FAILURE);
    } else
        printf("Socket successfully binded\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) {
        printf("Listen failed\n");
        exit(EXIT_FAILURE);
    } else
        printf("Server listening on port: %d.. Waiting for connection\n", server_port);

    return sockfd;
}

void clientDisconnected(User * user, int client_fd, bool shouldMutex) {
    // Client suddenly disconnected, fail gracefully
    debug("%s", "socket disconnected");

    petr_header h={0,LOGOUT};
    
    // Initialize jobh.msg_type = LOGOUT;
    JobProcess * job = malloc(sizeof(JobProcess));
    job->user = user;
    job->header = h;
    sprintf(job->message, "%s", "");
    debug("%s", "before logout");

    logout(job);
    debug("%s", "freee");
    free(job);
    debug("%s", "after free");


    pthread_cancel(pthread_self());
    debug("%s", "");
}

//Function running in thread
void *process_client(void *clientfd_ptr) {
    int client_fd = *(int *)clientfd_ptr;
    free(clientfd_ptr);
    int received_size;
    fd_set read_fds;

    User * user = NULL;

    char* buffer=(char*)malloc(BUFFER_SIZE);

    int retval;
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
                
        retval = select(client_fd + 1, &read_fds, NULL, NULL, NULL);
        if (retval != 1 && !FD_ISSET(client_fd, &read_fds)) {
            printf("Error with select() function\n");
            free(buffer);
            clientDisconnected(user, client_fd, false);
            break;
        }
        
    
       // Read header from client socket
       debug("%s\n", "About to start reading header");
       petr_header header={0,0};

       if (rd_msgheader(client_fd, &header) < 0) {
           //ERROR 
            printf("Receiving failed\n");   
            free(buffer);
            clientDisconnected(user, client_fd, false);
            break;
       } 

       if ((!header.msg_type)&&(!header.msg_len)) {
            free(buffer);
            clientDisconnected(user,client_fd, false);
            break;           
       }

        // Read message from socket
        if (header.msg_len > 0) {
            debug("%s\n", "About to start reading body");
            received_size = read(client_fd, buffer, header.msg_len);
            if (received_size <= 0) {
                printf("Receiving failed\n");   
                free(buffer);
                clientDisconnected(user, client_fd, true);
                break;
            } 
            debug("buffer: %s\n", buffer);
        }

        debug("headerType: %d headerLen: %d\n", header.msg_type, header.msg_len);
              
        // Handle login
        if (header.msg_type == LOGIN) {
            //Logging in
            if (header.msg_len <= 1) {
                // Case username is empty
                header.msg_type = ESERV;
                header.msg_len = 0;
                wr_msg(client_fd, &header, NULL);

                printf("Username is empty\n");   
                break;
            }
            if (isValidUsername(buffer, &USER_LIST)) {
                // Create new user
                user = malloc(sizeof(User));
                user->fd = client_fd;
                sprintf(user->username, "%s", buffer);
                

                pthread_mutex_lock(&USER_MUTEX);
                insertRear(&USER_LIST, user);
                pthread_mutex_unlock(&USER_MUTEX);

                header.msg_type = OK;
                header.msg_len = 0;
                wr_msg(client_fd, &header, NULL);
                continue;

            } else {
                // Username already exists
                header.msg_type = EUSREXISTS;
                header.msg_len = 0;
                wr_msg(client_fd, &header, NULL);

                printf("Username exists\n"); 
                break;
            }
        } 
        

        // Initialize job
        debug("%s\n", "About to start initializing job");
        JobProcess * job = malloc(sizeof(JobProcess));
        job->user = user;
        job->header = header;
        sprintf(job->message, "%s", buffer);

        if (header.msg_type == LOGOUT) {
            logout(job);
            debug("About to free\n");
            free(job);
            debug("About to free\n");
            free(buffer);
            pthread_cancel(pthread_self());
            break;
        }

        // Add job to Queue
        debug("%s\n", "About to start adding job to queue");
        pthread_mutex_lock(&JOB_MUTEX);

        insertRear(&JOB_LIST, job);
        debug("%s\n", "");
        sem_post(&JOB_SEM);
        debug("%s\n", "");

        pthread_mutex_unlock(&JOB_MUTEX);

        // UNLOCK buffer
        debug("%s\n", "");
    }
    debug("About to free\n");
    free(buffer);
    // Close the socket at the end
    printf("Close current client connection\n");
    close(client_fd);
    return NULL;
}

void run_server(int server_port) {
    listen_fd = server_init(server_port); // Initiate server and start listening on specified port
    int client_fd;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    pthread_t tid;

    while (1) {
        // Wait and Accept the connection from client
        printf("Wait for new client connection\n");
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(listen_fd, (SA *)&client_addr, (socklen_t*)&client_addr_len);
        if (*client_fd < 0) {
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        } else {
            printf("Client connetion accepted\n");
            pthread_create(&tid, NULL, process_client, (void *)client_fd);
        }
    }
    // never runs
    close(listen_fd);
    return;
}



int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    initializeLists();
    int opt;

    unsigned int port = 0;

    while ((opt = getopt(argc, argv, "hj:")) != -1) {
        switch (opt) {
        case 'h':
            printf("Server Application Usage: %s [-h] [-j <number of jobs>] <port number> <audit filename>\n", argv[0]);
            exit(EXIT_SUCCESS);
            break;
        case 'j':
            JOBS = atoi(optarg);
            break;
        default: /* '?' */
            printf("Server Application Usage: %s [-h] [-j <number of jobs>] <port number> <audit filename>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (argc>=3)
        {
            port = atoi(argv[argc-2]);
            sprintf(LOG_FILE,"%s",argv[argc-1]);
        }
    else
    {
        printf("Server Application Usage: %s [-h] [-j <number of jobs>] <port number> <audit filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    

    if (port == 0) {
        fprintf(stderr, "ERROR: Port number for server to listen is not given\n");
        printf("Server Application Usage: %s [-h] [-j <number of jobs>] <port number> <audit filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //printf("jobs = %d, port = %d, file = %s\n",JOBS, port, LOG_FILE);
    spawnJobs();
    run_server(port);

    return 0;
}
