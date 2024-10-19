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
#define MAX_BOOKS 100         // Maximum number of books

// Linked list node structure
typedef struct Node {
    struct Node* next;               // Points to the next node in the global list
    struct Node* book_next;          // Points to the next node in the same book
    char* data;
    struct Node* next_frequent_search; // Points to the next node containing the search term
} Node;

// Book structure
typedef struct Book {
    char title[50];                  // Title of the book
    int occurrences;                 // Total occurrences of the search term in the book
    Node *frequent_search_head;      // Head of the frequent search linked list
} Book;

// Global variables
Book books[MAX_BOOKS];               // Array of books
Node *global_list_head = NULL;       // Global list for all books
int book_count = 0;                  // Number of books processed
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for thread safety
char *search_term;  // Global variable for search term


// Function prototypes
void error(const char *msg);
void add_node_to_global_list(Node *new_node);
void add_node_to_book_list(char *data, Node **book_head, const char *search_term, Book *book);
void write_book_to_file(Node *book_head, int book_number);
void free_list(Node *book_head);
void remove_bom(char* buffer);
void *handle_client(void *newsockfd_ptr);
void free_global_list(Node* book_head);
void set_nonblocking(int sockfd);
void accumulate_line(char *buffer, char *line_buffer, int *line_pos, Node **book_head, const char *search_term, Book *book);
void print_sorted_books();
void *analysis_thread_func(void *arg);

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t thread_id, analysis_thread_id;  // Thread identifiers

    // Validate command-line arguments
    if (argc != 5 || strcmp(argv[1], "-l") != 0 || strcmp(argv[3], "-p") != 0) {
        fprintf(stderr, "ERROR: Invalid arguments\nUsage: ./server5 -l <port> -p <search_term>\n");
        exit(1);
    }

    // Extract port number and search term from the command line
    portno = atoi(argv[2]);        // Extract port number from the -l flag
    search_term = argv[4];         // Extract search term from the -p flag

    printf("Starting server on port %d with search term: %s\n", portno, search_term);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    // Initialize server address structure
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);  // Correctly use the extracted port number here

    // Bind the socket to the port
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    // Start listening for incoming connections
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    // Create the analysis thread to periodically print results
    pthread_create(&analysis_thread_id, NULL, analysis_thread_func, NULL);
    pthread_detach(analysis_thread_id);

    // Accept connections and create client threads
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");

        // Set the accepted socket to non-blocking mode
        set_nonblocking(newsockfd);

        // Allocate memory for socket pointer and connection order
        int *params = malloc(3 * sizeof(int));  // Allocate extra space for socket and connection order
        if (params == NULL) {
            error("ERROR allocating memory for socket pointer and order");
        }
        params[0] = newsockfd;         // First element is the socket
        params[1] = ++book_count;      // Second element is the connection order

        // Create a new thread for each client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)params) < 0) {
            error("ERROR creating thread");
        }

        pthread_detach(thread_id);  // Detach the thread to avoid memory leaks
    }

    // Close the socket and free global list
    free_global_list(global_list_head);
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

// void *handle_client(void *params) {
//     int *int_params = (int *)params;
//     int newsockfd = int_params[0];  // Extract socket
//     int connection_order = int_params[1];  // Extract connection order
//     char *filename = (char *)(int_params + 2); // Extract filename (full path)
//     free(params);  // Free the allocated memory

    


//     char buffer[BUFFER_SIZE];
//     char line_buffer[LINE_BUFFER_SIZE] = {0};  // Buffer to accumulate a full line
//     int line_pos = 0;  // Position within the line buffer
//     int n;
//     Node *book_head = NULL;  // Each thread has its own book-specific list
//     Book *current_book = &books[connection_order - 1];  // Reference to the current book

//     // char *dot = strrchr(filename, '.');  // Find the last dot
//     // if (dot != NULL) {
//     //     *dot = '\0';  // Remove the file extension by terminating the string early
//     // }
//     // strncpy(current_book->title, filename, sizeof(current_book->title) - 1);
//     // current_book->title[sizeof(current_book->title) - 1] = '\0';  // Ensure null-termination



//     n = read(newsockfd, buffer, BUFFER_SIZE - 1);
//     if (n > 0) {
//         buffer[n] = '\0';  // Null-terminate the string

//         // Extract the title from the first line
//         char *newline = strchr(buffer, '\n');
//         if (newline) *newline = '\0';  // Remove newline character from the title line
//         strncpy(current_book->title, buffer, sizeof(current_book->title) - 1);
//         current_book->title[sizeof(current_book->title) - 1] = '\0';  // Ensure null-termination
//     }

//     // Non-blocking read loop
//     while (1) {
//         n = read(newsockfd, buffer, BUFFER_SIZE - 1);

//         if (n > 0) {
//             buffer[n] = '\0';  // Null-terminate the string

//             // Remove BOM if present
//             remove_bom(buffer);

//             // Accumulate line data until we encounter a newline
//             accumulate_line(buffer, line_buffer, &line_pos, &book_head, search_term, current_book);
//         } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
//             usleep(1000);  // Sleep briefly before trying again
//             continue;
//         } else {
//             break;  // Connection closed or error
//         }
//     }

//     if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
//         error("ERROR reading from socket");
//     }

//     // Add the entire book list to the global list
//     pthread_mutex_lock(&list_mutex);  // Lock the mutex before modifying the global list
//     add_node_to_global_list(book_head);
//     pthread_mutex_unlock(&list_mutex);  // Unlock the mutex

//     // Write the book-specific list to a file, using connection order for file naming
//     write_book_to_file(book_head, connection_order);

//     // Close the socket
//     close(newsockfd);

//     return NULL;
// }


void *handle_client(void *params) {
    int *int_params = (int *)params;
    int newsockfd = int_params[0];  // Extract socket
    int connection_order = int_params[1];  // Extract connection order
    char *filename = (char *)(int_params + 2); // Extract filename
    free(params);  // Free the allocated memory

    char buffer[BUFFER_SIZE];
    char line_buffer[LINE_BUFFER_SIZE] = {0};  // Buffer to accumulate a full line
    int line_pos = 0;  // Position within the line buffer
    int n;
    Node *book_head = NULL;  // Each thread has its own book-specific list
    Book *current_book = &books[connection_order - 1];  // Reference to the current book
    int first_line = 1; //added

    // Process the content lines (after the filename)
    while (1) {
        n = read(newsockfd, buffer, BUFFER_SIZE - 1);

        if (n > 0) {
            buffer[n] = '\0';  // Null-terminate the string

            // Remove BOM if present
            remove_bom(buffer);

            // Accumulate line data until we encounter a newline
            accumulate_line(buffer, line_buffer, &line_pos, &book_head, search_term, current_book);
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
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


void accumulate_line(char *buffer, char *line_buffer, int *line_pos, Node **book_head, const char *search_term, Book *book) {
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '\n') {
            // Newline encountered, complete the line
            line_buffer[*line_pos] = '\0';  // Null-terminate the line
            strcat(line_buffer, "\n");  // Add the newline character to the line
            add_node_to_book_list(line_buffer, book_head, search_term, book);  // Pass the search term and current book
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
                add_node_to_book_list(line_buffer, book_head, search_term, book);  // Pass the search term and current book
            }
        }
    }
}

void add_node_to_book_list(char *data, Node **book_head, const char *search_term, Book *book) {
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
    new_node->next_frequent_search = NULL;

    // Add to the book-specific list (this tracks all nodes in the book)
    if (*book_head == NULL) {
        *book_head = new_node;
    } else {
        Node *book_temp = *book_head;
        while (book_temp->book_next != NULL) {
            book_temp = book_temp->book_next;
        }
        book_temp->book_next = new_node;
    }

    // Count occurrences of the search pattern in this line
    const char *temp_str = data;
    int found_occurrences = 0;
    while ((temp_str = strstr(temp_str, search_term)) != NULL) {
        found_occurrences++;
        temp_str++;  // Move past the found pattern to continue searching
    }

    // Update the book's total occurrences
    book->occurrences += found_occurrences;

    // If the line contains the search pattern, add it to the frequent search list
    if (found_occurrences > 0) {
        if (book->frequent_search_head == NULL) {
            book->frequent_search_head = new_node;
        } else {
            Node *temp = book->frequent_search_head;
            while (temp->next_frequent_search != NULL) {
                temp = temp->next_frequent_search;
            }
            temp->next_frequent_search = new_node;
        }
    }

    printf("Added node: %s", new_node->data);
}

void print_sorted_books() {
    printf("Books sorted by occurrences:\n");

    int printed_books[book_count];  // Array to track which books have already been printed
    memset(printed_books, 0, sizeof(printed_books));  // Initialize it to zero (not printed)

    for (int i = 0; i < book_count; i++) {
        int max_index = -1;
        int max_occurrences = -1;

        // Find the book with the maximum occurrences that hasn't been printed yet
        for (int j = 0; j < book_count; j++) {
            if (!printed_books[j] && books[j].occurrences > max_occurrences) {
                max_occurrences = books[j].occurrences;
                max_index = j;
            }
        }

        if (max_index != -1) {
            // Mark this book as printed
            printed_books[max_index] = 1;

            // Derive the filename from the connection order/book number
            // char filename[20];
            // sprintf(filename, "book_%02d.txt", max_index + 1);  // e.g., "book_01.txt", "book_02.txt"

            // Print the book title and occurrences
            printf("Book Title: %s (Occurrences: %d)\n", books[max_index].title, books[max_index].occurrences);
        }
    }
}


void *analysis_thread_func(void *arg) {
    while (1) {
        sleep(5);  // Wait for 5 seconds (or whatever interval is needed)

        pthread_mutex_lock(&list_mutex);  // Lock the list to ensure only one thread prints
        print_sorted_books();             // Print the sorted results by occurrences
        pthread_mutex_unlock(&list_mutex);  // Unlock the list after printing
    }
    return NULL;
}

// Function to add a book (list of nodes) to the global shared list
void add_node_to_global_list(Node *book_head) {
    if (book_head == NULL) {
        return;  // No book to add
    }

    // Add the book list to the global list (track all books)
    if (global_list_head == NULL) {
        global_list_head = book_head;
    } else {
        Node *temp = global_list_head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = book_head;  // Append book to the global list
    }
}

void free_global_list(Node *book_head) {
    Node *temp;
    while (book_head != NULL) {
        temp = book_head;
        book_head = book_head->next;
        free(temp->data);  // Free the duplicated string
        free(temp);        // Free the node
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

// Function to handle errors
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Function to remove BOM (Byte Order Mark) from UTF-8 text
void remove_bom(char* buffer) {
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};  // UTF-8 BOM
    if (memcmp(buffer, bom, 3) == 0) {
        memmove(buffer, buffer + 3, strlen(buffer) - 2);  // Remove BOM
    }
}
