#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define BUFFER_SIZE 1024  // Increased buffer size for long lines

// Linked list node structure
typedef struct Node {
    struct Node* next;
    struct Node* book_next;
    struct Node* next_frequent_search;
    char* data;
} Node;

// Global variables
Node *head = NULL;
int book_counter = 1;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void error(char *msg);
void add_node_to_list(char *data, Node **book_head);
void write_book_to_file(Node *book_head, int book_number);
void free_list(Node *book_head);
void remove_bom(char* buffer);


void *handle_client(void *newsockfd_ptr);

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t thread_id;


    // int n;
    // Node *book_head = NULL;

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    // Initialize server address structure
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Bind the socket to an address
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    // Start listening for connections
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    // Accept connection
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0)
        error("ERROR on accept");

    // Read message from client and add to the linked list
    while ((n = read(newsockfd, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[n] = '\0';  // Null-terminate the string

        // Remove BOM if present
        remove_bom(buffer);

        // Add to the linked list
        add_node_to_list(buffer, &book_head);
    }

    // add mutex lock here 
    // Write the linked list (book) to a file when the connection is closed
    write_book_to_file(book_head, book_counter++);
    // add mutex unlock here 

    // add mutex lock here
    // Free the memory used by the linked list
    free_list(book_head);
    // add mutex unlock here 

    // Close the sockets
    close(newsockfd);
    close(sockfd);

    return 0;
}

// Function to remove BOM (Byte Order Mark) from UTF-8 text
void remove_bom(char* buffer) {
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};  // UTF-8 BOM
    if (memcmp(buffer, bom, 3) == 0) {
        memmove(buffer, buffer + 3, strlen(buffer) - 2);  // Remove BOM
    }
}

// Function to write the current book to a file
void write_book_to_file(Node *book_head, int book_number) {
    char filename[20];
    sprintf(filename, "book_%02d.txt", book_number);
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        error("ERROR opening file");
        return;
    }

    Node *temp = book_head;
    while (temp != NULL) {
        fprintf(file, "%s\n", temp->data);  // Write each line to the file
        temp = temp->book_next;
    }

    fclose(file);
    printf("Data written to file: %s\n", filename);
}

// Helper function to remove trailing newlines (\n or \r\n)
void trim_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';  // Remove the newline character
        if (len > 1 && str[len - 2] == '\r') {
            str[len - 2] = '\0';  // Handle \r\n (Windows-style line ending)
        }
    }
}

// Modified function to add a node to the shared list and book-specific list
void add_node_to_list(char *data, Node **book_head) {
    // Remove trailing newline characters from the data
    trim_newline(data);

    // Create a new node
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->data = strdup(data);  // Duplicate the line of text
    new_node->next = NULL;
    new_node->book_next = NULL;
    new_node->next_frequent_search = NULL;

    // Add to the global list
    if (head == NULL) {
        head = new_node;
    } else {
        Node *temp = head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_node;
    }

    // Add to the book-specific list
    if (*book_head == NULL) {
        *book_head = new_node;
    } else {
        Node *book_temp = *book_head;
        while (book_temp->book_next != NULL) {
            book_temp = book_temp->book_next;
        }
        book_temp->book_next = new_node;
    }

    printf("Added node: %s\n", new_node->data);  // Print added line for debugging
}

// Function to free the linked list for the book
void free_list(Node *book_head) {
    Node *temp;
    while (book_head != NULL) {
        temp = book_head;
        book_head = book_head->book_next;
        free(temp->data);  // Free the duplicated string
        free(temp);        // Free the node
    }
}

// Function to handle errors
void error(char *msg) {
    perror(msg);
    exit(1);
}