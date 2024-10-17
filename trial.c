/* Detailed Outline for a Multi-Threaded Network Server in C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 256

/* Node Structure for Shared Linked List */
typedef struct Node {
    char data[BUFFER_SIZE];
    struct Node *next;
    struct Node *book_next;
    struct Node *next_frequent_search;
} Node;

/* Shared Data Structure */
typedef struct SharedData {
    Node *head;
    pthread_mutex_t lock;
} SharedData;

/* Shared Data Structure Initialization */
SharedData sharedList = {NULL, PTHREAD_MUTEX_INITIALIZER};

/* Error Handling Function */
void error(const char *msg) {
    perror(msg);
    exit(1);
}

/* Function to Add Data to Shared Linked List */
void add_to_shared_list(const char *data) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    if (new_node == NULL) {
        perror("Failed to allocate memory for new node");
        return;
    }
    strcpy(new_node->data, data);
    new_node->next = NULL;
    new_node->book_next = NULL;
    new_node->next_frequent_search = NULL;

    pthread_mutex_lock(&sharedList.lock);
    if (sharedList.head == NULL) {
        sharedList.head = new_node;
    } else {
        Node *current = sharedList.head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
    pthread_mutex_unlock(&sharedList.lock);

    printf("Added node: %s\n", data);
}

/* Client Handler Thread Function */
void *client_thread(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    while (1) {
        int n = read(client_sock, buffer, BUFFER_SIZE - 1);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue; // No data available at the moment
            } else {
                perror("ERROR reading from socket");
                break;
            }
        } else if (n == 0) {
            // Client has disconnected
            close(client_sock);
            return NULL;
        }

        add_to_shared_list(buffer);
        bzero(buffer, BUFFER_SIZE);
    }

    close(client_sock);
    return NULL;
}

/* Server Class Functions */
void start_server(int port) {
    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); // Set socket to non-blocking

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd, MAX_CLIENTS);
    printf("Server listening on port %d\n", port);

    clilen = sizeof(cli_addr);
    while (1) {
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue; // No pending connections
            } else {
                perror("ERROR on accept");
            }
        } else {
            // Allocate memory for client socket descriptor to pass to thread
            int *newsock_ptr = (int *)malloc(sizeof(int));
            *newsock_ptr = newsockfd;

            pthread_t thread;
            if (pthread_create(&thread, NULL, client_thread, newsock_ptr) != 0) {
                perror("ERROR creating thread");
                close(newsockfd);
                free(newsock_ptr);
            }
            pthread_detach(thread); // Automatically free resources when thread completes
        }
    }

    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    int port = atoi(argv[1]);
    start_server(port);
    return 0;
}