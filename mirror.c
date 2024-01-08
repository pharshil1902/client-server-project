#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <tar.h>


#define PORT 9002
#define BUFFER_SIZE 1024

void processclient(int client_socket);

// ------------------------------------- validate_command -------------------------------

int is_valid_date_format(const char *date_str) {
    // Check if the date string is not NULL and has the correct length (10 characters for yyyy-mm-dd)
    if (date_str == NULL || strlen(date_str) != 10) {
        return 0;
    }

    // Check if each character in the date string is a digit or a hyphen
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            // The 5th and 8th characters should be hyphens
            if (date_str[i] != '-') {
                return 0;
            }
        } else {
            // All other characters should be digits
            if (!isdigit(date_str[i])) {
                return 0;
            }
        }
    }

    // Parse the year, month, and day from the date string
    int year = atoi(date_str);
    int month = atoi(date_str + 5);
    int day = atoi(date_str + 8);

    // Check if the parsed values are within valid ranges
    if (year < 1000 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    // Additional checks for specific months with fewer days
    if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) {
        return 0;
    }

    // Check for February with leap year
    if (month == 2) {
        int is_leap_year = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if ((is_leap_year && day > 29) || (!is_leap_year && day > 28)) {
            return 0;
        }
    }

    // The date format is valid
    return 1;
}


int validate_command(char *command) {
    // Tokenize the command to extract the command type and arguments
    char *command_type = strtok(command, " ");
    char *arguments = strtok(NULL, "");

    if (strcmp(command_type, "fgets") == 0) {
        // Check syntax for 'fgets' command: fgets file1 file2 file3 file4
        // The command must start with 'fgets' followed by at least one filename.
        if (arguments == NULL) {
            return 0;
        }
    } else if (strcmp(command_type, "tarfgetz") == 0) {
        // Check syntax for 'tarfgetz' command: tarfgetz size1 size2 <-u>
        // The command must start with 'tarfgetz' followed by two integers (size1 and size2).
        // An optional '-u' flag can be included at the end.
        char *size1_str = strtok(arguments, " ");
        char *size2_str = strtok(NULL, " ");
        char *unzip_flag = strtok(NULL, " ");
        if (size1_str == NULL || size2_str == NULL) {
            return 0;
        }
        int size1 = atoi(size1_str);
        int size2 = atoi(size2_str);
        if (size1 < 0 || size2 < 0 || size1 > size2) {
            return 0;
        }
        if (unzip_flag != NULL && strcmp(unzip_flag, "-u") != 0) {
            return 0;
        }
    } else if (strcmp(command_type, "filesrch") == 0) {
        // Check syntax for 'filesrch' command: filesrch filename
        // The command must start with 'filesrch' followed by a filename.
        if (arguments == NULL) {
            return 0;
        }
    } else if (strcmp(command_type, "targzf") == 0) {
        // Check syntax for 'targzf' command: targzf <extension list> <-u>
        // The command must start with 'targzf' followed by at least one file extension.
        // An optional '-u' flag can be included at the end.
        char *extensions = strtok(arguments, " ");
        char *unzip_flag = strtok(NULL, " ");
        if (extensions == NULL) {
            return 0;
        }
        if (unzip_flag != NULL && strcmp(unzip_flag, "-u") != 0) {
            return 0;
        }
    } else if (strcmp(command_type, "getdirf") == 0) {
        // Check syntax for 'getdirf' command: getdirf date1 date2 <-u>
        // The command must start with 'getdirf' followed by two dates in yyyy-mm-dd format.
        // An optional '-u' flag can be included at the end.
        char *date1 = strtok(arguments, " ");
        char *date2 = strtok(NULL, " ");
        char *unzip_flag = strtok(NULL, " ");
        if (date1 == NULL || date2 == NULL) {
            return 0;
        }
        if (!is_valid_date_format(date1) || !is_valid_date_format(date2)) {
            return 0;
        }
        if (unzip_flag != NULL && strcmp(unzip_flag, "-u") != 0) {
            return 0;
        }
    } else if (strcmp(command_type, "quit") == 0) {
        // Check syntax for 'quit' command: quit
        // The command must be exactly 'quit'.
        if (arguments != NULL) {
            return 0;
        }
    } else {
        // Invalid command type
        return 0;
    }

    // All syntax checks passed, the command is valid
    return 1;
}

// -------------------------- handle_fgets_command ------------------------

//void add_file_to_tar(FILE *tar_file, const char *file_path) {
//    // Open the file in read mode
//    FILE *input_file = fopen(file_path, "r");
//    if (input_file == NULL) {
//        fprintf(stderr, "Error opening file '%s': %s\n", file_path, strerror(errno));
//        return;
//    }
//
//    // Get the size of the file
//    fseek(input_file, 0, SEEK_END);
//    long file_size = ftell(input_file);
//    fseek(input_file, 0, SEEK_SET);
//
//    // Allocate memory to read the file content
//    char *file_content = (char *) malloc(file_size + 1);
//    if (file_content == NULL) {
//        fclose(input_file);
//        fprintf(stderr, "Error allocating memory for file content: %s\n", strerror(errno));
//        return;
//    }
//
//    // Read the file content into memory
//    size_t bytes_read = fread(file_content, 1, file_size, input_file);
//    if (bytes_read != (size_t) file_size) {
//        fprintf(stderr, "Error reading file '%s': %s\n", file_path, strerror(errno));
//        free(file_content);
//        fclose(input_file);
//        return;
//    }
//    file_content[bytes_read] = '\0';
//
//    // Create the tar header for the file
//    struct tar_header th;
//    th.typeflag = REGTYPE;
//    th.linkname[0] = '\0';
//    th.size = file_size;
//    th.chksum = 0;
//    th.prefix[0] = '\0';
//    th.name = file_path;
//
//    // Write the tar header to the tar file
//    if (fwrite(&th, 1, sizeof(th), tar_file) != sizeof(th)) {
//        fprintf(stderr, "Error writing tar header for file '%s': %s\n", file_path, strerror(errno));
//        free(file_content);
//        fclose(input_file);
//        return;
//    }
//
//    // Write the file content to the tar file
//    if (fwrite(file_content, 1, file_size, tar_file) != (size_t) file_size) {
//        fprintf(stderr, "Error writing file content to tar file: %s\n", strerror(errno));
//    }
//
//    // Free allocated memory and close the file
//    free(file_content);
//    fclose(input_file);
//}

void search_files(const char *files[], int num_files, const char *tar_name) {
    // Open the tar file in write mode
    FILE *tar_file = fopen(tar_name, "w");
    if (tar_file == NULL) {
        fprintf(stderr, "Error creating tar archive '%s': %s\n", tar_name, strerror(errno));
        return;
    }

    // Traverse the server's directory tree and search for the specified files
    // You may need to modify the root directory path based on your server's file structure.
    // Replace "/path/to/server/root/" with the actual path to the root directory of the server.
    const char *root_directory = "/path/to/server/root/";
    for (int i = 0; i < num_files; i++) {
        // Construct the absolute file path
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "%s%s", root_directory, files[i]);

        // Check if the file exists
        struct stat file_stat;
        if (stat(file_path, &file_stat) != 0) {
            fprintf(stderr, "File '%s' not found: %s\n", files[i], strerror(errno));
            continue;
        }

        // Check if the file is a regular file
        if (!S_ISREG(file_stat.st_mode)) {
            fprintf(stderr, "File '%s' is not a regular file\n", files[i]);
            continue;
        }

        // Add the file to the tar archive
//        add_file_to_tar(tar_file, file_path);
    }

    // Close the tar file
    fclose(tar_file);
}


void handle_fgets_command(char *arguments, char *response) {
    // Tokenize the space-separated file names from the arguments
    char *file_name = strtok(arguments, " ");
    char *files[4]; // Assuming the maximum of 4 files in fgets command
    int num_files = 0;

    while (file_name != NULL && num_files < 4) {
        files[num_files] = file_name;
        num_files++;
        file_name = strtok(NULL, " ");
    }

    if (num_files == 0) {
        // No files specified in the command
        sprintf(response, "No files specified");
    } else {
        // Create the tar archive
        char tar_name[] = "temp.tar.gz";
        search_files(files, num_files, tar_name);
        sprintf(response, "Tar archive created: %s", tar_name);
    }
}

// ----------------------------------------------------------------

void handle_tarfgetz_command(char *arguments, char *response) {
    // Implement logic for handling 'tarfgetz' command
    // For demonstration purposes, let's assume we found the files and create the tar archive.
    char tar_name[] = "temp.tar.gz";
    sprintf(response, "Tar archive created: %s", tar_name);
}

void handle_filesrch_command(char *arguments, char *response) {
    // Implement logic for handling 'filesrch' command
    // For demonstration purposes, let's assume we found the file and have the file details.
    char file_details[] = "sample.txt 1234 2023-08-06";
    strcpy(response, file_details);
}

void handle_targzf_command(char *arguments, char *response) {
    // Implement logic for handling 'targzf' command
    // For demonstration purposes, let's assume we found the files and create the tar archive.
    char tar_name[] = "temp.tar.gz";
    sprintf(response, "Tar archive created: %s", tar_name);
}

void handle_getdirf_command(char *arguments, char *response) {
    // Implement logic for handling 'getdirf' command
    // For demonstration purposes, let's assume we found the files and create the tar archive.
    char tar_name[] = "temp.tar.gz";
    sprintf(response, "Tar archive created: %s", tar_name);
}

void processclient(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (1) {
        // Receive command from the client
        bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            // Client disconnected or error occurred
            break;
        }

        buffer[bytes_received] = '\0';

        // Validate the received command syntax
        if (!validate_command(buffer)) {
            char response[] = "Invalid command syntax";
            send(client_socket, response, strlen(response), 0);
            continue;
        }

        // Process the client command and send appropriate responses
        char response[BUFFER_SIZE] = {0};

        // Tokenize the command to extract the command type and arguments
        char *command_type = strtok(buffer, " ");
        char *arguments = strtok(NULL, "");

        if (strcmp(command_type, "fgets") == 0) {
            handle_fgets_command(arguments, response);
        } else if (strcmp(command_type, "tarfgetz") == 0) {
            handle_tarfgetz_command(arguments, response);
        } else if (strcmp(command_type, "filesrch") == 0) {
            handle_filesrch_command(arguments, response);
        } else if (strcmp(command_type, "targzf") == 0) {
            handle_targzf_command(arguments, response);
        } else if (strcmp(command_type, "getdirf") == 0) {
            handle_getdirf_command(arguments, response);
        } else if (strcmp(command_type, "quit") == 0) {
            // Handle 'quit' command
            char quit_response[] = "Goodbye!";
            send(client_socket, quit_response, strlen(quit_response), 0);
            break;
        } else {
            // Invalid command
            char invalid_response[] = "Invalid command";
            send(client_socket, invalid_response, strlen(invalid_response), 0);
        }

        // Send the response back to the client
        send(client_socket, response, strlen(response), 0);
    }
}


int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    int listen_fd, connection_fd, portNumber;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pid_t child_pid;


    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Could not create socket\n");
        exit(1);
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((uint16_t) atoi(argv[1]));


    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        close(server_socket);
        exit(1);
    }

    // Listen for client connections
    if (listen(server_socket, 5) < 0) {
        perror("Error listening");
        close(server_socket);
        exit(1);
    }

    while (1) {
        // Accept client connection
        client_socket = accept(server_socket, (struct sockaddr *) NULL, NULL);
        if (client_socket < 0) {
            perror("Error accepting client connection");
            continue;
        }

        // Fork a child process to handle the client request
        child_pid = fork();
        if (child_pid < 0) {
            perror("Error forking child process");
            close(client_socket);
            continue;
        } else if (child_pid == 0) {
            // Child process
            pid_t pid = getpid();
            printf("child_pid :: => :: %d\n", pid);

            while (1) {
                printf("Message from the client\n");
                char buff1[1024];
                recv(client_socket, buff1, sizeof(buff1), 0);
                if (strcmp(buff1, "quit") == 0) {
                    break;
                }
                printf("%s", buff1);
                if (strcmp(buff1, "quit") == 0) {
                    exit(0);
                }
                char buff[1024];
                printf("\nType your message to the client\n");
                fgets(buff, sizeof(buff), stdin);
                // Remove the newline character from the end of the input
                size_t input_length = strlen(buff);
                if (input_length > 0 && buff[input_length - 1] == '\n') {
                    buff[input_length - 1] = '\0';
                }
                send(client_socket, buff, strlen(buff), 0);
            }
        } else {
            // Parent process
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}