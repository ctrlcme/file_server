#include "build.h"

char *path;

// check if desired file for copying to fileserver exists
int
file_exists(char *file) {
    struct stat buffer;

    if(stat(file, &buffer) == 0)
        return (S_ISREG(buffer.st_mode));
    else {
        fprintf(stderr, "Usage error: The file %s does not exist.\n", file);
        exit(EXIT_FAILURE);
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
    ssize_t bytes;

    file_fd = open(file, O_RDONLY);

    if (file_fd == -1) {
        exit(errno);
    }
    
    data = malloc(size);
    if(!data) {
        close(fd);
        close(file_fd);
        return ENOMEM;
    }

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
    if (close(fd))
        err = EIO;
    if (close(file_fd))
        err = EIO;
    if (err) 
        return(err);
    
    // add logic to check that the bytes sent == bytes received
    // perhaps some logic to validate the file sent OK?
    printf("\nThe file: %s sent successfully.\n", file);
    return 0;
}

// Handle the processing for upload option from menu_selection
void
upload_opt(int fd) {
    uint32_t exists, tmp_exists, length, tmp_length, answer, tmp_answer;
    char *filename;
    printf("\nWhat filename would you like the file server to copy this file as?\nName: ");
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
        switch(answer) {
            case 1:
                send(fd, &tmp_answer, sizeof(tmp_answer), 0);
                printf("\nNo action to do, stopping...\n");
                exit(1);
                break;
            case 2:
                send(fd, &tmp_answer, sizeof(tmp_answer), 0);
                printf("\nOverwriting the file..\n");
                send_file(path, fd, 0);
                break;
            default:
                printf("\nPlease select a valid option (1 or 2)\n");
                exit(1);
        }
    } 
    else { 
        printf("\nCopying the file now...\n");
        send_file(path, fd, 0);
    }
}

// Handle the download option selected from menu_selection
int
download_opt(int fd) {
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
    return 0;
}

// handle the remove option selected from menu_selection
int
remove_opt() {
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
                upload_opt(fd);
                running = 0;
                break;
            case 2:
                printf("\nNot yet implemented >.>\n");
                send(fd, &tmp_choice, sizeof(tmp_choice), 0);
                running = 0; // remove when implemented?
                break;
            case 3:
                printf("\nListing the contents of file server...\n");
                send(fd, &tmp_choice, sizeof(tmp_choice), 0);
                if ((error = list_opt(fd)) != 0)
                    fprintf(stderr, "list_files: return value of %d\n", error);

                // Don't display menu right away
                printf("\nPress enter to continue.");
                fflush(stdout);
                while ((c = getchar()) != '\n' && c != EOF);

                break;
            case 4:
                printf("\nNot yet implemented >.>\n");
                send(fd, &tmp_choice, sizeof(tmp_choice), 0);
                running = 0; // remove when implemented?
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
    path = argv[2];

    if (argc != 3) {
        fprintf(stderr,"How to run:\n%s [SERVER] [FILE] \n", argv[0]);
        exit(EXIT_FAILURE);
    } else if (file_exists(argv[2]) == 0) {
        fprintf(stderr, "Usage error: The file %s is a directory.\n", argv[2]);
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
