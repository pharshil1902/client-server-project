#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <ctype.h>
#include <fcntl.h>

#define PORT 9003
#define BUFFER_SIZE 1024
int isCmdValid = 0;


void validate_command(char *command);

void receive_tar_file(int socket) {
    long file_size;
    if (recv(socket, &file_size, sizeof(file_size), 0) == -1) {
        perror("Error receiving file size");
        return;
    }

    printf("%ld\n", file_size);

    if (file_size == 0) {
        printf("No file to receive.\n");
        return;
    }

    char *buffer = (char *)malloc(file_size);
    if (buffer == NULL) {
        perror("Memory allocation error");
        return;
    }

    ssize_t bytes_received = recv(socket, buffer, file_size, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("Connection closed by the server.\n");
        } else {
            perror("Error receiving file data");
        }
        free(buffer);
        return;
    }

    FILE *file = fopen("received.tar.gz", "wb");
    if (file == NULL) {
        perror("Error opening destination file");
        free(buffer);
        return;
    }

    size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
    if (bytes_written < bytes_received) {
        perror("Error writing to file");
    } else {
        printf("File received and saved as 'received.tar.gz'.\n");
    }

    if (fclose(file) == EOF) {
        perror("Error closing destination file");
    }

    free(buffer);
}


void receive_response(int socket) {
    char response[BUFFER_SIZE];
    ssize_t bytes_received = recv(socket, response, sizeof(response) - 1, 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("Response from server: %s\n", response);
    } else if (bytes_received == 0) {
        printf("Connection closed by the server.\n");
    } else {
        perror("Error receiving response");
    }
}


int main(int argc, char *argv[]) {
    int client_socket;
    struct sockaddr_in server_addr;
    struct hostent *server;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error creating socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t) atoi(argv[2]));//Port number
    printf("%s", argv[1]);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) < 0) {
        fprintf(stderr, " inet_pton() has failed\n");
        exit(2);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddrnano *) &server_addr, sizeof(server_addr)) < 0) {//Connect()
        perror("Error connecting to server");
        close(client_socket);
        exit(3);
    }

    char command[BUFFER_SIZE];
    int is_valid;

    while (1) {
        // Read command from the user
        char cmdArr[1024];
        printf("\nEnter Command:\n");
        fgets(cmdArr, sizeof(cmdArr), stdin);
        // Remove the newline character from the end of the input
        size_t input_length = strlen(cmdArr);
        if (input_length > 0 && cmdArr[input_length - 1] == '\n') {
            cmdArr[input_length - 1] = '\0';
        }

        char tempCmdArr[1024];
        strcpy(tempCmdArr, cmdArr);

        validate_command(tempCmdArr);
        if (isCmdValid == 1) {
            printf("\nisCmdValid :: => :: %d\n", isCmdValid);
            send(client_socket, cmdArr, strlen(cmdArr), 0);
            if (strcmp(cmdArr, "quit") == 0) {
                close(client_socket);
                break;
            }
            printf("Message from the server:\n\n");

            // Receive the flag from the server
            int flag;
            recv(client_socket, &flag, sizeof(int), 0);
            printf("%d\n",flag);

            if (flag == 1) {
                receive_tar_file(client_socket);
            }

            printf("printing response:\n");
            char server_response[1024];
            recv(client_socket, server_response, sizeof(server_response)-1, 0);
            printf("%s", server_response);

        } else {
            printf("\ncommand is not valid\n");
        }
    }
}


int checkInputCmd(char *command) {
    int fileCount = 0;
    char *token = strtok(command, " ");
    while (token != NULL) {
        fileCount++;
        token = strtok(NULL, " ");
    }
    return fileCount;
}

int substrExists(const char *command, const char *substring) {
    int commandLen = strlen(command);       // Length of the command string
    int substringLen = strlen(substring);   // Length of the substring to be found

    // Loop through the command string to check for the presence of the substring
    for (int i = 0; i <= commandLen - substringLen; i++) {
        int j;
        // Compare each character in the command string with the substring
        for (j = 0; j < substringLen; j++) {
            if (command[i + j] != substring[j])
                break; // If characters don't match, break out of the inner loop
        }

        // If the inner loop completes without a break, the entire substring has been found
        if (j == substringLen)
            return 1; // Return 1 to indicate that the substring exists in the command
    }

    // If the loop completes without finding the substring, return 0
    return 0; // Return 0 to indicate that the substring does not exist in the command
}

void validate_fGets(char *cmd) {
    int c = checkInputCmd(cmd);
    printf("%d\n", c);
    if (c <= 5) {
        isCmdValid = 1;
    } else {
        isCmdValid = 0;
    }
}

void validate_tarGets(char *cmd) {
    int size1, size2;
    char flag[5];
    if (sscanf(cmd, "tarfgetz %d %d %4s", &size1, &size2, flag) == 3) {
        if (strcmp(flag, "-u") == 0) {
            if (size1 <= size2) {
                isCmdValid = 1;
            } else {
                isCmdValid = 0;
            }
        } else {
            isCmdValid = 0;
        }
    } else if (sscanf(cmd, "tarfgetz %d %d", &size1, &size2) == 2) {
        if (size1 <= size2 && size1 > 0 && size2 > 0) {
            isCmdValid = 1;
        } else {
            isCmdValid = 0;
        }
    } else {
        isCmdValid = 0;
        printf("Invalid format, usage: tarfgetz size1 size2 <-u>");
    }
}

bool is_valid_date(const char *date) {
    // Check for valid format (YYYY-MM-DD)
    if (strlen(date) != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }

    // Check that the non-dash characters are valid digits
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            continue;  // Skip dashes
        }
        if (date[i] < '0' || date[i] > '9') {
            return false;
        }
    }
    // Additional validation logic can be added here if needed

    return true;
}

void validate_getDirf(char *cmd) {
    char date1[11], date2[11]; // Room for YYYY-MM-DD and null-terminator
    char flag[5];
    if (sscanf(cmd, "getdirf %10s %10s %4s", date1, date2, flag) == 3) {
        if (strcmp(flag, "-u") == 0) {
            if (is_valid_date(date1) && is_valid_date(date2)) {
                if (strcmp(date1, date2) <= 0) {
                    isCmdValid = 1;
                } else {
                    isCmdValid = 0;
                }
            } else {
                isCmdValid = 0;
            }
        } else {
            isCmdValid = 0;
            printf("Invalid format, usage: tarfgetz size1 size2 <-u>");
        }
    } else if (sscanf(cmd, "getdirf %10s %10s", date1, date2) == 2) {
        if (is_valid_date(date1) && is_valid_date(date2)) {
            if (strcmp(date1, date2) <= 0) {
                isCmdValid = 1;
            } else {
                isCmdValid = 0;
            }
        } else {
            isCmdValid = 0;
        }
    } else {
        isCmdValid = 0;
        printf("Invalid format, usage: tarfgetz size1 size2 <-u>");
    }
}

void validate_command(char *command) {
    char *tempCmd = command;
    if (substrExists(tempCmd, "fgets")) {
        validate_fGets(command);
    } else if (substrExists(tempCmd, "tarfgetz")) {
        validate_tarGets(tempCmd);
    } else if (substrExists(tempCmd, "filesrch")) {
        isCmdValid = 1;
    } else if (substrExists(tempCmd, "targzf")) {
        isCmdValid = 1;
    } else if (substrExists(tempCmd, "getdirf")) {
        validate_getDirf(command);
    } else if (substrExists(tempCmd, "quit")) {
        isCmdValid = 1;
    } else {
        isCmdValid = 0;
    }
}


