#include "build.h"

/* GENERAL CLEANUP STILL NEEDS TO OCCUR 
 * WITH VARS, MEM, RETURNS, ETC.
 * BUT PERSISTENT MENU WORKS AS WELL AS ALL OPTIONS */

// check if desired file for copying to fileserver exists
int
file_exists(char *file) {
    struct stat buffer;

    if(stat(file, &buffer) == 0)
        return (S_ISREG(buffer.st_mode));
    else {
        fprintf(stderr, "Usage error: The file (%s) does not exist.\n", file);
        return -16;
    }    
}

// get dynamically allocated string
// can probably remove this to instead provide filename
// on either option 1 or 2 and use
// (char*)malloc(filename * sizeof(char));
char
*get_str() {
    char *str = NULL, *tmp_str = NULL;
    size_t size = 0, index = 0;
    int ch = EOF;

    while (ch) {
        ch = getc(stdin);

        // check if we should stop
        if (ch == EOF || ch == '\n')
            ch = 0;

        // check if we should expand
        if (size <= index) {
            size += 5;
            tmp_str = realloc(str, size);

            if (!tmp_str) {
                free(str);
                str = NULL;
                break;
            }   
            str = tmp_str;
        }
        str[index++] = ch;
    }
    str[index++] = '\0';
    return str;
}

// send the selected file to the file server
int
send_file(char *file, int fd, const size_t chunk) {
    const size_t size = (chunk > 0) ? chunk : DEFAULT_CHUNK;
    char *data, *ptr, *end;
    int file_fd, err;
    struct stat st;
    ssize_t bytes, total = 0;
    uint32_t total_size;

    file_fd = open(file, O_RDONLY);

    if (file_fd == -1) {
        return errno;
    }
    
    data = malloc(size);
    if (!data) {
        close(fd);
        close(file_fd);
        return ENOMEM;
    }

    if (fstat(file_fd, &st) == -1) {
        perror("stat failed");
        return errno;
    }

    total_size = htonl((uint32_t)st.st_size);
    send(fd, &total_size, sizeof(total_size), 0);

    while(1) {
        bytes = read(file_fd, data, size);

        if (bytes < 0) {
            if (bytes == -1)
                err = errno;
            else
                err = EIO;
            free(data);
            close(fd);
            close(file_fd);
            exit(err);
        }
        else {
            if (bytes == 0)
                break;
        }

        total += bytes;

        ptr = data;
        end = data + bytes;
        while (ptr < end) {
            // there might be problems with this
            bytes = send(fd, ptr, (size_t)(end - ptr), 0);
            
            if (bytes <= 0) {
                if (bytes == -1)
                    err = errno;
                else
                    err = EIO;
                free(data);
                close(fd);
                close(file_fd);
                exit(err);
            }
            else {
                ptr += bytes;
            }
        }
    }

    free(data);

    err = 0;
    if (close(file_fd))
        return EIO;
    
    printf("Sent %zd bytes\n", total);

    if (total != total_size) {
        fprintf(stderr, "The total sent bytes does not match the total size of the file\n");
    }
    else
        printf("The file %s sent successfully.\n", file);

    return 0;
}

// Handle the processing for upload option from menu_selection
int
upload_opt(int fd) {
    uint32_t exists, tmp_exists, length, tmp_length, answer, tmp_answer, able;
    char *filename, *path;
    int running = 1;

    printf("\nWhat file would you like to upload to the server?\nName: ");
    path = get_str();

    if (file_exists(path) == -16) {
        able = htonl(-16);
        send(fd, &able, sizeof(able), 0);
        return -16;
    }
    else if (file_exists(path) == 0) {
        fprintf(stderr, "Usage error: The file %s is a directory.\n", path);
        able = htonl(-7);
        send(fd, &able, sizeof(able), 0);
        return -7;
    }

    able = htonl(0);
    send(fd, &able, sizeof(able), 0);

    printf("\nWhat filename would you like the file server to save this file as?\nName: ");
    filename = get_str();

    tmp_length = strlen(filename);
    length = htonl(tmp_length);
    send(fd, &length, sizeof(length), 0);
    send(fd, filename, tmp_length, 0);
    read(fd, &tmp_exists, sizeof(tmp_exists));
    exists = ntohl(tmp_exists);

    if (exists) {
        printf("\nThe file exists on the file server, please select an option from below:\n\n");
        printf(" 1) Don't overwrite the file\n");
        printf(" 2) Overwrite the file\n");
        printf("\nSelection: ");

        scanf("%u", &answer);
        tmp_answer = htonl(answer);

        // Do I need to cast to an int?
        while (running) {
            switch(answer) {
                case 1:
                    send(fd, &tmp_answer, sizeof(tmp_answer), 0);
                    printf("\nNo action to do, stopping...\n");
                    running = 0;
                    break;
                case 2:
                    send(fd, &tmp_answer, sizeof(tmp_answer), 0);
                    printf("\nOverwriting the file..\n");
                    send_file(path, fd, 0);
                    running = 0;
                    break;
                default:
                    printf("\nPlease select a valid option (1 or 2)\n");
                    break;
            }
        }
    } 
    else { 
        printf("\nUploading the file now...\n");
        send_file(path, fd, 0);
    }

    return 0;
}

// Handle the download option selected from menu_selection
int
download_opt(int fd, const size_t chunk) {
    const size_t size = (chunk > 0) ? chunk : DEFAULT_CHUNK;
    char *data, *ptr, *end, *filename, *dir;
    ssize_t bytes, total = 0;
    int file_fd, err;
    uint32_t length, tmp_length, able, tmp_able, total_size;

    printf("\nWhat file would you like to download from the server?\nName: ");
    filename = get_str();

    printf("\nWhat directory would you like to download this file to?\nName: ");
    dir = get_str();
    
    if (file_exists(dir) == -16) {
        able = htonl(-16);
        send(fd, &able, sizeof(able), 0);
        return -16;
    }
    else if (file_exists(dir) != 0) {
        fprintf(stderr, "Usage error: The directory you specified (%s) is NOT a directory.\n", dir);
        able = htonl(-4);
        send(fd, &able, sizeof(able), 0);
        return -4;
    }

    if (dir[strlen(dir) - 1] != '/') {
        fprintf(stderr, "Usage error: Please put a '/' at the end of the directory you enter.\n");
        able = htonl(-5);
        send(fd, &able, sizeof(able), 0);
        return -5;
    }

    able = htonl(0);
    send(fd, &able, sizeof(able), 0);

    // realloc dir with filename attached
    dir = realloc(dir, (strlen(dir) + strlen(filename)));

    if (!dir) {
        free(dir);
        free(filename);
        perror("realloc failed");
        return errno;
    }  

    strcat(dir, filename);

    tmp_length = strlen(filename);
    length = htonl(tmp_length);
    send(fd, &length, sizeof(length), 0);
    send(fd, filename, tmp_length, 0);

    read(fd, &tmp_able, sizeof(tmp_able));
    able = ntohl(tmp_able);

    if (able != 0) 
        return able;

    if ((file_fd = open(dir, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) < 0) {
        if (errno == EEXIST) {
            if ((file_fd = open(dir, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
            // need to handle overwriting on client side
            fprintf(stderr, "Downloading the file failed.\n");
            free(filename);
            free(dir);
            printf("The errno was: %d\n", errno);
            return errno;
            }

            printf("This file already exists in the specified directory, overwriting...\n");
        }
        else {
            fprintf(stderr, "Downloading the file failed.\n");
            free(filename);
            free(dir);
            printf("The errno was: %d\n", errno);
            return errno;
        }
    }
    
    data = malloc(size);
    if (!data) {
        close(fd);
        close(file_fd);
        unlink(dir);
        free(filename);
        free(dir);
        return ENOMEM;
    }

    read(fd, &total_size, sizeof(total_size));
    total_size = ntohl(total_size);

    while (total < total_size) {
        bytes = read(fd, data, size);
        if (bytes < 0) {
            err = errno;
            free(data);
            close(fd);
            close(file_fd);
            unlink(dir);
            free(filename);
            free(dir);
            return err;
        }
        else if (bytes == 0)
            continue;

        total += bytes;

        ptr = data;
        end = data + bytes;
        while (ptr < end) {
            bytes = write(file_fd, ptr, (size_t)(end - ptr));
            if (bytes < 0) {
                err = errno;
                free(data);
                close(fd);
                close(file_fd);
                unlink(dir);
                free(filename);
                free(dir);
                return err;
            }
            ptr += bytes;
        }
    }

    free(data);

    err = 0;
    if (close(file_fd)) {
        unlink(dir);
        return EIO;
    }

    printf("Received %zd bytes\n", total);

    if (total != total_size) {
        fprintf(stderr, "The total downloaded bytes does not match the total size of the file\n");
    }
    else
        printf("The file %s was downloaded successfully.\n", dir);

    free(filename);
    free(dir);

    return 0;
}

// Handle the list option selected from menu_selection
int
list_opt(int fd) {
    char *files;
    uint32_t length, tmp_length;
    ssize_t bytes;
    
    // receive the files length first
    read(fd, &tmp_length, sizeof(tmp_length));
    length = ntohl(tmp_length);

    files = malloc(length);
    if (files == NULL) {
        perror("malloc failed for files");
        return errno;
    }

    if ((bytes = read(fd, files, length)) != length) {
        fprintf(stderr, "Read %zd bytes for files, expected %u\n", bytes, length);
        free(files);
        return -3;
    }

    files[length] = '\0';

    printf("Files on file server :\n%s\n", files);

    free(files);
    return 0;
}

// handle the remove option selected from menu_selection
int
remove_opt(int fd) {
    uint32_t length, tmp_length, able, tmp_able;
    char *filename;
    printf("\nWhat file would you like the file server to remove?\nName: ");

    filename = get_str();
    tmp_length = strlen(filename);
    length = htonl(tmp_length);
    send(fd, &length, sizeof(length), 0);
    send(fd, filename, tmp_length, 0);
    read(fd, &tmp_able, sizeof(tmp_able));
    able = ntohl(tmp_able);

    if (able != 0) {
        return able;
    }

    printf("The file server successfully deleted the file: %s\n", filename);
    
    free(filename);
    return 0;
}

// TUI menu for client
// THIS HAPPENS FIRST, KEEP THAT IN MIND WHEN TAMPERING WITH FILE SERVER READS AND WRITES
void
menu_selection(int fd) {
    uint32_t choice, tmp_choice; 
    int running = 1, error, c;
    char input[100];

    // Leave menu up even after option selected
    while (running) {

        printf("\n======= File Server Options =======\n\n");
        printf(" 1) Upload a file\n");
        printf(" 2) Download a file\n");
        printf(" 3) List the contents of file server\n");
        printf(" 4) Remove a file from the file server\n");
        printf(" 5) Exit the client\n");
        printf("\nSelection: ");
        
        fgets(input, sizeof(input), stdin);
        sscanf(input, "%u", &choice);

        tmp_choice = htonl(choice);
        
        switch (choice) {
            case 1:
                send(fd, &tmp_choice, sizeof(tmp_choice), 0);
                if ((error = upload_opt(fd)) != 0)
                    fprintf(stderr, "upload_opt: return value of %d\n", error);

                // Don't display menu right away
                printf("\nPress enter to continue.");
                fflush(stdout);
                while ((c = getchar()) != '\n' && c != EOF);

                break;
            case 2:
                send(fd, &tmp_choice, sizeof(tmp_choice), 0);
                if ((error = download_opt(fd, 0)) != 0) 
                    fprintf(stderr, "download_opt: return value of %d\n", error);

                // Don't display menu right away
                printf("\nPress enter to continue.");
                fflush(stdout);
                while ((c = getchar()) != '\n' && c != EOF);

                break;
            case 3:
                printf("\nListing the contents of file server...\n");
                send(fd, &tmp_choice, sizeof(tmp_choice), 0);
                if ((error = list_opt(fd)) != 0)
                    fprintf(stderr, "list_opt: return value of %d\n", error);

                // Don't display menu right away
                printf("\nPress enter to continue.");
                fflush(stdout);
                while ((c = getchar()) != '\n' && c != EOF);
                break;
            case 4:
                send(fd, &tmp_choice, sizeof(tmp_choice), 0);
                if ((error = remove_opt(fd)) != 0) {
                    if (error == 2)
                        fprintf(stderr, "remove_opt: the file does not exist on fileserver.\n");
                    fprintf(stderr, "remove_opt: return value of %d\n", error);
                }
                // Don't display menu right away
                printf("\nPress enter to continue.");
                fflush(stdout);
                while ((c = getchar()) != '\n' && c != EOF);
                break;
            case 5:
                printf("\nExiting...\n");
                send(fd, &tmp_choice, sizeof(tmp_choice), 0);
                running = 0;
                break;
            default:
                printf("\nPlease select a valid option (1, 2, 3, 4, or 5)\n");
                break;
        }
    }
}

int
main(int argc, char *argv[]){ 
    //path = argv[2];

    if (argc != 2) {
        fprintf(stderr,"How to run:\n%s [SERVER]\n", argv[0]);
        exit(EXIT_FAILURE);
    } 

    int i, fd;

    struct addrinfo hints, *ai0, *ai;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    /* When turning this into a service, how can I provide values
       from a .conf file? */
    if ((i = getaddrinfo(argv[1], PORT, &hints, &ai0)) != 0) {
        fprintf(stderr, "Error: Unable to lookup IP address: %s\n", strerror(i));
        exit(EXIT_FAILURE);
    }

    for (ai = ai0; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

        if (fd < 0)
            continue; // unable to create socket, try next in linked list
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(fd);
            continue;
        }
        break; // successfully connected
    }

    if (ai == NULL) {
        perror("Connecting to address failed.");
        exit(EXIT_FAILURE);
    } else {
        // intro msg
        printf("\nYou have connected to the fileserver...\n");

        // implementation of menu options and passing values to the server
        menu_selection(fd);

        return 0;
    }
}
