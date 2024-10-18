#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>   // For non-blocking I/O
#include <errno.h>   // For error handling

#define BUFFER_SIZE 1024  // Increased buffer size for long lines
#define LINE_BUFFER_SIZE 2048  // Buffer size to accumulate a full line

// Linked list node structure
typedef struct Node {
    struct Node* next;  // Points to the next node in the global list
    struct Node* book_next;  // Points to the next node in the same book
    char* data;
    struct Node* next_frequent_search;
} Node;

// Global variables
Node *head = NULL;  // Global list for all books
int book_counter = 1;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void error(char *msg);
void add_node_to_global_list(Node *new_node);
void add_node_to_book_list(char *data, Node **book_head);
void write_book_to_file(Node *book_head, int book_number);
void free_list(Node *book_head);
void remove_bom(char* buffer);
void *handle_client(void *newsockfd_ptr);
void free_global_list(Node* book_head);
void set_nonblocking(int sockfd);
void accumulate_line(char *buffer, char *line_buffer, int *line_pos, Node **book_head);

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t thread_id;  // Thread identifier

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

    // Accept connections and create threads
    // Modify the thread creation logic to pass connection order
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");

        // Set the accepted socket to non-blocking mode
        set_nonblocking(newsockfd);

        // Allocate memory for socket pointer and connection order
        int *params = malloc(2 * sizeof(int)); // Array to hold both newsockfd and connection order
        if (params == NULL) {
            error("ERROR allocating memory for socket pointer and order");
        }
        params[0] = newsockfd;         // First element is the socket
        params[1] = book_counter++;    // Second element is the connection order

        if (pthread_create(&thread_id, NULL, handle_client, (void *)params) < 0) {
            error("ERROR creating thread");
        }

        // Detach the thread to avoid memory leaks
        pthread_detach(thread_id);
    }


    // Close the socket and free global list
    free_global_list(head);
    close(sockfd);
    return 0;
}

// Set a socket to non-blocking mode
void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        error("ERROR getting socket flags");
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        error("ERROR setting non-blocking mode");
    }
}

void *handle_client(void *params) {
    int *int_params = (int *)params;
    int newsockfd = int_params[0];  // Extract socket
    int connection_order = int_params[1];  // Extract connection order
    free(params);  // Free the allocated memory

    char buffer[BUFFER_SIZE];
    char line_buffer[LINE_BUFFER_SIZE] = {0};  // Buffer to accumulate a full line
    int line_pos = 0;  // Position within the line buffer
    int n;
    Node *book_head = NULL;  // Each thread has its own book-specific list

    // Non-blocking read loop
    while (1) {
        n = read(newsockfd, buffer, BUFFER_SIZE - 1);

        if (n > 0) {
            buffer[n] = '\0';  // Null-terminate the string

            // Remove BOM if present
            remove_bom(buffer);

            // Accumulate line data until we encounter a newline
            accumulate_line(buffer, line_buffer, &line_pos, &book_head);
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // No data available, continue non-blocking behavior
            usleep(1000);  // Sleep briefly before trying again
            continue;
        } else {
            break;  // Connection closed or error
        }
    }

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        error("ERROR reading from socket");
    }

    // Add the entire book list to the global list
    pthread_mutex_lock(&list_mutex);  // Lock the mutex before modifying the global list
    add_node_to_global_list(book_head);
    pthread_mutex_unlock(&list_mutex);  // Unlock the mutex

    // Write the book-specific list to a file, using connection order for file naming
    write_book_to_file(book_head, connection_order);

    // Close the socket
    close(newsockfd);

    return NULL;
}


// Function to accumulate data until a newline is encountered
void accumulate_line(char *buffer, char *line_buffer, int *line_pos, Node **book_head) {
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '\n') {
            // Newline encountered, complete the line
            line_buffer[*line_pos] = '\0';  // Null-terminate the line
            strcat(line_buffer, "\n");  // Add the newline character to the line
            add_node_to_book_list(line_buffer, book_head);
            *line_pos = 0;  // Reset the line position for the next line
        } else {
            // Add character to the line buffer
            line_buffer[*line_pos] = buffer[i];
            (*line_pos)++;
            // Avoid overflow
            if (*line_pos >= LINE_BUFFER_SIZE - 1) {
                line_buffer[LINE_BUFFER_SIZE - 1] = '\0';  // Null-terminate in case of overflow
                strcat(line_buffer, "\n");  // Add the newline character
                *line_pos = 0;
                add_node_to_book_list(line_buffer, book_head);  // Add what we've accumulated so far
            }
        }
    }
}


// Function to remove BOM (Byte Order Mark) from UTF-8 text
void remove_bom(char* buffer) {
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};  // UTF-8 BOM
    if (memcmp(buffer, bom, 3) == 0) {
        memmove(buffer, buffer + 3, strlen(buffer) - 2);  // Remove BOM
    }
}

// Function to add a book (list of nodes) to the global shared list
void add_node_to_global_list(Node *book_head) {
    if (book_head == NULL) {
        return;  // No book to add
    }

    // Add the book list to the global list (track all books)
    if (head == NULL) {
        head = book_head;
    } else {
        Node *temp = head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = book_head;  // Append book to the global list
    }
}

// Function to write the current book to a file
void write_book_to_file(Node *book_head, int connection_order) {
    char filename[20];
    sprintf(filename, "book_%02d.txt", connection_order);  // Use connection order for filename
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        error("ERROR opening file");
        return;
    }

    Node *temp = book_head;

    // Iterate through each node in the linked list (each node is a line)
    while (temp != NULL) {
        char *line = temp->data;  // Get the data (the line) from the node
        int i = 0;

        // Write each character one by one
        while (line[i] != '\0') {
            if (line[i] == '\n') {
                // If we encounter a newline, write it to the file and start a new line
                fputc('\n', file);
            } else {
                // Otherwise, write the character to the file
                fputc(line[i], file);
            }
            i++;  // Move to the next character
        }

        // After finishing one node, move to the next node in the list
        temp = temp->book_next;
    }

    fclose(file);
    printf("Data written to file: %s\n", filename);
}


// Function to add a node to the book-specific list
void add_node_to_book_list(char *data, Node **book_head) {
    // Create a new node
    Node *new_node = (Node *)malloc(sizeof(Node));
    if (new_node == NULL) {
        error("ERROR allocating memory for new node");
    }
    new_node->data = strdup(data);  // Duplicate the line of text
    if (new_node->data == NULL) {
        free(new_node);
        error("ERROR duplicating string");
    }
    new_node->next = NULL;
    new_node->book_next = NULL;

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

// Function to free the global linked list
void free_global_list(Node *book_head) {
    Node *temp;
    while (book_head != NULL) {
        temp = book_head;
        book_head = book_head->next;
        free(temp->data);  // Free the duplicated string
        free(temp);        // Free the node
    }
}

// Function to handle errors
void error(char *msg) {
    perror(msg);
    exit(1);
}
