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
#include <time.h>
#include <arpa/inet.h>
#include <signal.h>
#include <limits.h>

#define PORT 9002
#define BUFFER_SIZE 1024
#define FILE_TRANSFER_PORT 9003

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    // ...
};

void processclient(int client_socket, pid_t pro_id);


// Function to transfer a file from server to client
int transfer_file(int client_socket, const char *filename, int is_upload) {

    // Open the file
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    // Read and send the file data
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    // Close the file and file transfer socket
    fclose(file);

    return 0;
}

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

// Function to add a file to a tar archive
void add_file_to_tar(const char *tar_filename, const char *file_path, int *flag, char *response) {
    char command[256];
    snprintf(command, sizeof(command), "tar --append --file=%s %s", tar_filename, file_path);
    if (system(command) != 0) {
        sprintf(response, "Error creating TAR archive");
        printf("file not found");
        *flag = 0;
        return;
    }
}

void search_and_add_file(const char *current_directory, const char *target_file,
                         const char *tar_name, int *flag, char *response) {

    DIR *dir = opendir(current_directory);
    if (dir == NULL) {
        fprintf(stderr, "Unable to open directory '%s': %s\n", current_directory, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", current_directory, entry->d_name);

        struct stat file_stat;
        if (stat(file_path, &file_stat) != 0) {
            fprintf(stderr, "File '%s' not found: %s\n", file_path, strerror(errno));
            printf("not found");
            *flag = 0;
            continue;
        }

        if (S_ISREG(file_stat.st_mode) && strcmp(entry->d_name, target_file) == 0) {
            // Add the file to the tar archive
            add_file_to_tar(tar_name, file_path, flag, response);
        } else if (S_ISDIR(file_stat.st_mode)) {
            // Recursively search in subdirectory
            search_and_add_file(file_path, target_file, tar_name, flag, response);
        }
    }

    closedir(dir);
}

void search_files(const char *files[], int num_files, const char *tar_name, int *flag, char *response) {
    const char *root_directory = getenv("HOME");
    strcat(root_directory, "/");
    for (int i = 0; i < num_files; i++) {
        search_and_add_file(root_directory, files[i], tar_name, flag, response);
    }
}

// void send_tar_file(const char *filename, int client_socket) {
//     int fd = open(filename, O_RDONLY);
//     if (fd == -1) {
//         perror("Error opening file");
//         return;
//     }

//     struct stat stat_buf;
//     if (fstat(fd, &stat_buf) != 0) {
//         perror("Error getting file size");
//         close(fd);
//         return;
//     }

//     // Send the file size to the client
//     off_t file_size = stat_buf.st_size;
//     printf("%ld\n", file_size);
//     if (send(client_socket, &file_size, sizeof(off_t), 0) == -1) {
//         perror("Error sending file size");
//         close(fd);
//         return;
//     }

//     // Send the file data using the sendfile() function
//     off_t offset = 0;
//     ssize_t sent_bytes = sendfile(client_socket, fd, &offset, stat_buf.st_size);
//     if (sent_bytes == -1) {
//         perror("Error sending file data");
//         close(fd);
//         return;
//     }

//     printf("Sent %zd bytes of file data\n", sent_bytes);

//     close(fd);
// }

void send_tar_file(const char *file_path, int socket) {
    printf("file_path :: => :: %s\n", file_path);
    printf("socket :: => :: %d\n", socket);
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("Error opening TAR file");
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (send(socket, &file_size, sizeof(file_size), 0) == -1) {
        perror("Error sending file size");
        fclose(file);
        return;
    }
    printf("%ld\n", file_size);

    char *buffer = (char *)malloc(file_size);
    if (buffer == NULL) {
        perror("Memory allocation error");
        fclose(file);
        return;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        perror("Error reading TAR file");
        free(buffer);
        fclose(file);
        return;
    }

    if (send(socket, buffer, file_size, 0) == -1) {
        perror("Error sending TAR file");
    }

    free(buffer);
    fclose(file);
}


void handle_fgets_command(char *arguments, char *response, pid_t pro_id, int client_socket) {
    // Tokenize the space-separated file names from the arguments
    char *file_name = strtok(arguments, " ");
    char *files[4]; // Assuming the maximum of 4 files in fgets command
    int num_files = 0;
    int start_flag = 1;

    while (file_name != NULL && num_files < 4) {
        printf("%s\n", file_name);
        files[num_files] = file_name;
        num_files++;
        file_name = strtok(NULL, " ");
    }

    if (num_files == 0) {
        // No files specified in the command
        sprintf(response, "No files specified");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    } else {
        // Create the directory if it doesn't exist
        char dir_name[PATH_MAX];
        snprintf(dir_name, sizeof(dir_name), "%d", pro_id);
        if (mkdir(dir_name, 0777) != 0 && errno != EEXIST) {
            perror("Error creating directory");
            sprintf(response, "Error creating directory");
            start_flag = 0;
            send(client_socket, &start_flag, sizeof(int), 0);
            return;
        }

        // Create the tar archive within the directory
        char tar_name[PATH_MAX];
        snprintf(tar_name, sizeof(tar_name), "%s/%s", dir_name, "temp.tar.gz");
        remove(tar_name); // previous temp tar file deleting
        search_files(files, num_files, tar_name, &start_flag, response); // Uncomment and implement this function

        // Check if the file exists
        if (access(tar_name, F_OK) != -1) {
            start_flag = 1;
        } else {
            start_flag = 0;
        }

        if (start_flag == 1) {
            sprintf(response, "Tar archive created: %s", tar_name);
            send(client_socket, &start_flag, sizeof(int), 0);
            send_tar_file(tar_name, client_socket);
        } else {
            send(client_socket, &start_flag, sizeof(int), 0);
            sprintf(response, "No file found");
        }


    }
}

// ----------------------------handle_tarfgetz_command------------------------------------

void handle_tarfgetz_command(char *arguments, char *response, pid_t pro_id, int client_socket) {
    // Tokenize the space-separated arguments
    char *size1_str = strtok(arguments, " ");
    char *size2_str = strtok(NULL, " ");
    char *unzip_flag = strtok(NULL, " ");
    int start_flag = 1;

    if (size1_str == NULL || size2_str == NULL) {
        sprintf(response, "Invalid arguments");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Convert size1 and size2 to integers
    int size1 = atoi(size1_str);
    int size2 = atoi(size2_str);

    if (size1 < 0 || size2 < 0 || size1 > size2) {
        sprintf(response, "Invalid size criteria");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Get the user's home directory
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        sprintf(response, "Unable to get home directory");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Create a temporary TAR file name
    char tar_name[PATH_MAX];


    // Create the directory if it doesn't exist
    char dir_name[PATH_MAX];
    snprintf(dir_name, sizeof(dir_name), "%d", pro_id);
    if (mkdir(dir_name, 0777) != 0 && errno != EEXIST) {
        sprintf(response, "Error creating directory");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Use the find command to write the list of files to the file
    char find_command[256];
    snprintf(find_command, sizeof(find_command), "find ~ -type f -size +%dk -a -size -%dk > %d/file_list.txt", size1,
             size2, pro_id);
    if (system(find_command) != 0) {
        sprintf(response, "No files found");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "%d/file_list.txt", pro_id);
    // Open the file in binary read mode
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Seek to the end of the file
    fseek(file, 0, SEEK_END);

    // Get the current position (which is the size of the file)
    long file_size = ftell(file);

    if (file_size == 0) {
        sprintf(response, "No files found");
        start_flag = 0;
        printf("not found");
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Close the file
    fclose(file);


    // Create the TAR archive using the file list
    char tar_command[256];

    snprintf(tar_name, sizeof(tar_name), "%s/%s", dir_name, "temp.tar.gz");
    remove(tar_name); // previous temp tar file deleting

    snprintf(tar_command, sizeof(tar_command), "tar czf %s -T %d/file_list.txt", tar_name, pro_id);
    if (system(tar_command) != 0) {
        sprintf(response, "Error creating TAR archive");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Check if the -u flag is specified for unzipping
    if (unzip_flag != NULL && strcmp(unzip_flag, "-u") == 0) {
        // Send the TAR archive to the client for unzipping
        // send_tar_file(tar_name, client_socket);
    }

    if (start_flag == 1) {
        sprintf(response, "Tar archive created: %s", tar_name);
        printf("sending response...\n");
        send(client_socket, &start_flag, sizeof(int), 0);
        send_tar_file(tar_name, client_socket);
        printf("sending response...\n");
        return;
    } else {
        sprintf(response, "No files found");
        send(client_socket, &start_flag, sizeof(int), 0);
    }
}

// ---------------------------------handle_filesrch_command---------------------------------

void format_creation_time(time_t ctime, char *formatted_time) {
    struct tm *timeinfo;
    timeinfo = localtime(&ctime);
    strftime(formatted_time, 20, "%b %d %H:%M", timeinfo);
}

void handle_filesrch_command(char *arguments, char *response, int client_socket) {
    // Tokenize the command arguments to get the filename
    int start_flag = 0;
    char *filename = strtok(arguments, " ");
    if (filename == NULL) {
        sprintf(response, "No filename specified");
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Get the user's home directory
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        sprintf(response, "Unable to get home directory");
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Create a buffer to store the full path to the target file
    char target_path[PATH_MAX];

    // Use the find command to search for the file
    char find_command[256];
    snprintf(find_command, sizeof(find_command), "find %s -type f -name %s | head -n 1", home_dir, filename);

    FILE *find_output = popen(find_command, "r");
    if (find_output == NULL) {
        sprintf(response, "Error searching for file");
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Read the first line of the find command output, which should be the path to the file
    if (fgets(target_path, sizeof(target_path), find_output) == NULL) {
        pclose(find_output);
        sprintf(response, "File not found");
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Remove the newline character from the path
    target_path[strlen(target_path) - 1] = '\0';

    // Close the find command output pipe
    pclose(find_output);



    // Get file size and creation time
    struct stat file_stat;
    if (stat(target_path, &file_stat) != 0) {
        send(client_socket, &start_flag, sizeof(int), 0);
        sprintf(response, "Error getting file information");
        return;
    }

    // Convert the st_ctime value to formatted creation time
    char formatted_time[20];
    format_creation_time(file_stat.st_ctime, formatted_time);

    // Format the response with filename, size, and formatted creation time
    sprintf(response, "%s %lld %s", filename, (long long) file_stat.st_size, formatted_time);
    send(client_socket, &start_flag, sizeof(int), 0);
}

// -------------------------------handle_targzf_command---------------------------------------------

void handle_targzf_command(char *arguments, char *response, pid_t pro_id, int client_socket) {
    // Tokenize the space-separated arguments
    char *extension_list = strtok(arguments, " ");
    int start_flag = 1;

    // Create the directory if it doesn't exist
    char dir_name[PATH_MAX];
    snprintf(dir_name, sizeof(dir_name), "%d", pro_id);
    if (mkdir(dir_name, 0777) != 0 && errno != EEXIST) {
        perror("Error creating directory");
        sprintf(response, "Error creating directory");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    if (extension_list == NULL) {
        sprintf(response, "Invalid arguments");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Extract file extensions from the extension list
    char *extensions[6]; // Assuming maximum of 6 extensions
    int num_extensions = 0;

    while (extension_list != NULL && num_extensions < 6) {
        extensions[num_extensions] = extension_list;
        num_extensions++;
        extension_list = strtok(NULL, " ");
    }

    char *unzip_flag = strtok(NULL, " ");

    if (num_extensions == 0) {
        // No extensions specified in the command
        sprintf(response, "No file extensions specified");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    char file_list_path[PATH_MAX];
    snprintf(file_list_path, sizeof(file_list_path), "%s/%s", dir_name, "file_list.txt");
    // Create a temporary TAR file name
    char tar_name[PATH_MAX];
    snprintf(tar_name, sizeof(tar_name), "%s/temp.tar.gz", dir_name);

    // Create a temporary file to store the list of files to archive
    FILE *file_list = fopen(file_list_path, "w");
    if (file_list == NULL) {
        sprintf(response, "Error creating file list");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }


    // Get the user's home directory
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        sprintf(response, "Unable to get home directory");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        fclose(file_list);
        return;
    }

    // Use the find command to search for files with specified extensions
    char find_command[256];
    snprintf(find_command, sizeof(find_command), "find %s -type f ", home_dir);

    for (int i = 0; i < num_extensions; i++) {
        if (i > 0) {
            strcat(find_command, "-o ");
        }
        strcat(find_command, "-name '*.");
        strcat(find_command, extensions[i]);
        strcat(find_command, "' ");
    }


    strcat(find_command, " > ");
    strcat(find_command, file_list_path);
    // Execute the find command to generate the file list

    if (system(find_command) != 0) {
        sprintf(response, "No files found");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Close the file
    fclose(file_list);

    // Create the TAR archive using the file list
    char tar_command[PATH_MAX + 50];
    snprintf(tar_command, sizeof(tar_command), "tar czf %s -T %s", tar_name, file_list_path);
    if (system(tar_command) != 0) {
        sprintf(response, "Error creating TAR archive");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Check if the -u flag is specified for unzipping
    if (unzip_flag != NULL && strcmp(unzip_flag, "-u") == 0) {
        // Send the TAR archive to the client for unzipping
        // send_tar_file(tar_name, client_socket);
    }

    if (start_flag == 1) {
        sprintf(response, "Tar archive created: %s", tar_name);
        send(client_socket, &start_flag, sizeof(int), 0);
        send_tar_file(tar_name, client_socket);
    } else {
        sprintf(response, "No file found");
        send(client_socket, &start_flag, sizeof(int), 0);
    }
}


void handle_getdirf_command(char *arguments, char *response, int client_socket) {
    // Tokenize the command arguments
    char *date1 = strtok(arguments, " ");
    char *date2 = strtok(NULL, " ");
    char *unzip_flag = strtok(NULL, " ");
    int start_flag = 1;

    if (date1 == NULL || date2 == NULL) {
        sprintf(response, "Invalid arguments");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Get the user's home directory
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        sprintf(response, "Unable to get home directory");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Create a directory to store the TAR archive
    char dir_name[PATH_MAX];
    snprintf(dir_name, sizeof(dir_name), "%d", (int) getpid());
    if (mkdir(dir_name, 0777) != 0 && errno != EEXIST) {
        perror("Error creating directory");
        sprintf(response, "Error creating directory");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Create a temporary TAR file name
    char tar_name[PATH_MAX];
    snprintf(tar_name, sizeof(tar_name), "%s/temp.tar.gz", dir_name);

    // Use the find command to search for files within the specified date range
    char find_command[512];
    snprintf(find_command, sizeof(find_command), "find %s -type f -newermt %s ! -newermt %s > %s/file_list.txt",
             home_dir, date1, date2, dir_name);

    if (system(find_command) != 0) {
        sprintf(response, "Error creating TAR archive");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Create the TAR archive using the file list
    char tar_command[512];
    snprintf(tar_command, sizeof(tar_command), "tar czf %s -T %s/file_list.txt", tar_name, dir_name);
    if (system(tar_command) != 0) {
        sprintf(response, "Error creating TAR archive");
        start_flag = 0;
        send(client_socket, &start_flag, sizeof(int), 0);
        return;
    }

    // Check if the -u flag is specified for unzipping
    if (unzip_flag != NULL && strcmp(unzip_flag, "-u") == 0) {
        // Send the TAR archive to the client for unzipping
        // send_tar_file(tar_name, client_socket);
    }

    if (start_flag == 1) {
        sprintf(response, "Tar archive created: %s", tar_name);
        send(client_socket, &start_flag, sizeof(int), 0);
        send_tar_file(tar_name, client_socket);
    } else {
        sprintf(response, "No file found");
        send(client_socket, &start_flag, sizeof(int), 0);
    }

}


void processclient(int client_socket, pid_t pro_id) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    printf("%d\n", client_socket);
    while (1) {
        // Receive command from the client
        bytes_received = read(client_socket, buffer, 1024);
        // bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            // Client disconnected or error occurred
            break;
        }

        buffer[bytes_received] = '\0';

        // // Validate the received command syntax
        // if (!validate_command(buffer)) {
        //     char response[] = "Invalid command syntax";
        //     send(client_socket, response, strlen(response), 0);
        //     continue;
        // }

        // Process the client command and send appropriate responses
        char response[BUFFER_SIZE] = {0};

        // Tokenize the command to extract the command type and arguments
        char *command_type = strtok(buffer, " ");
        char *arguments = strtok(NULL, "");

        if (strcmp(command_type, "fgets") == 0) {
            handle_fgets_command(arguments, response, pro_id, client_socket);
        } else if (strcmp(command_type, "tarfgetz") == 0) {
            handle_tarfgetz_command(arguments, response, pro_id, client_socket);
        } else if (strcmp(command_type, "filesrch") == 0) {
            handle_filesrch_command(arguments, response, client_socket);
        } else if (strcmp(command_type, "targzf") == 0) {
            handle_targzf_command(arguments, response, pro_id, client_socket);
        } else if (strcmp(command_type, "getdirf") == 0) {
            handle_getdirf_command(arguments, response, client_socket);
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
        printf("%s", response);
        send(client_socket, response, strlen(response), 0);
    }
}

void server_connections(int server_socket) {
    int client_socket = accept(server_socket, (struct sockaddr *) NULL, NULL);
    if (client_socket < 0) {
        perror("Error accepting client connection");
    }
    pid_t child_pid;

    // Fork a child process to handle the client request
    child_pid = fork();
    if (child_pid < 0) {
        perror("Error forking child process");
        close(client_socket);
    } else if (child_pid == 0) {
        // Child process
        pid_t pro_id = getpid();
        processclient(client_socket, pro_id);
    } else {
        // Parent process
        close(client_socket);
    }
}

void route_forward(char *mirror_ip, int mirror_port, int server_sd) {
    int client_sd = accept(server_sd, (struct sockaddr *) NULL, NULL);

    pid_t pid = fork();
    if (pid == 0) {
        int server_mirror_sd;
        struct sockaddr_in mirror_addr;
        server_mirror_sd = socket(AF_INET, SOCK_STREAM, 0);

        memset(&mirror_addr, 0, sizeof(mirror_addr));
        mirror_addr.sin_family = AF_INET;
        mirror_addr.sin_port = htons((uint16_t) mirror_port);//Port number

        if (inet_pton(AF_INET, mirror_ip, &mirror_addr.sin_addr) < 0) {
            fprintf(stderr, " inet_pton() has failed\n");
            exit(2);
        }

        // Connect to the server
        if (connect(server_mirror_sd, (struct sockaddrnano *) &mirror_addr, sizeof(mirror_addr)) < 0) {//Connect()
            perror("Error connecting to server");
            close(server_mirror_sd);
            exit(3);
        }
        printf("client_Sd :: => :: %d\n", client_sd);
        printf("server_sd :: => :: %d\n", server_sd);
        printf("server_mirror_sd :: => :: %d\n", server_mirror_sd);
        pid_t child_pid = getpid();
        printf("child_pid :: => :: %d\n", child_pid);
        printf("parent pid :: => :: %d\n", getpgid(child_pid));
        while (1) {
            char client_input[1024];
            char mirror_output[1024];
            recv(client_sd, client_input, sizeof(client_input), 0);
            printf("client_input :: => :: %s\n", client_input);
            send(server_mirror_sd, client_input, strlen(client_input), 0);
            if (strcmp(client_input, "quit") == 0) {
                printf("quit\n");
                close(server_mirror_sd);
                kill(child_pid, 0);
                exit(0);
            }
            recv(server_mirror_sd, mirror_output, sizeof(mirror_output), 0);
            send(client_sd, mirror_output, strlen(mirror_output), 0);
            memset(client_input, 0, sizeof(client_input));
        }
    } else {
    }
}

int main(int argc, char *argv[]) {
    int server_sd;
    struct sockaddr_in server_addr;


    if ((server_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Could not create socket\n");
        exit(1);
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((uint16_t) atoi(argv[1]));


    // Bind socket to address and port
    if (bind(server_sd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        close(server_sd);
        exit(1);
    }

    // Listen for client connections
    if (listen(server_sd, 5) < 0) {
        perror("Error listening");
        close(server_sd);
        exit(1);
    }

    int client_connections = 0;
    while (1) {
        if (client_connections < 6) {
            server_connections(server_sd);
            client_connections = client_connections + 1;
            //server
        } else if (client_connections < 12) {
            route_forward(argv[2], atoi(argv[3]), server_sd);
            client_connections = client_connections + 1;
            //mirror
        } else {
            if (client_connections % 2 != 0) {
                server_connections(server_sd);
                client_connections = client_connections + 1;
                //server
            } else if (client_connections % 2 == 0) {
                route_forward(argv[1], atoi(argv[3]), server_sd);
                client_connections = client_connections + 1;
                //mirror
            } else {
                break;
            }
        }
    }

    close(server_sd);
    return 0;
}