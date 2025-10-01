#include "build.h"

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

void
menu_selection(int fd) {
    uint32_t exists, tmp_exists, answer, tmp_answer;
    
    read(fd, &tmp_exists, sizeof(tmp_exists));
    exists = ntohl(tmp_exists);

    if (exists) {
        printf("The file exists on the file server, please select an option from below:\n");
        printf("\t 0) Don't overwrite the file\n");
        printf("\t 1) Overwrite the file\n");
        printf("Selection: ");

        scanf("%u", &answer);
        

        switch((int)answer) {
            case 0:
                printf("No action to do, stopping...\n");
                exit(1);
                break;
            case 1:
                printf("Overwriting the file..\n");
                break;
            default:
                printf("Please select a valid option (1 or 0)\n");
                exit(1);
        }

        tmp_answer = htonl(answer);
        send(fd, &tmp_answer, sizeof(tmp_answer), 0);

    } else 
        printf("Copying the file now...\n");
}

int
send_file(char *file, int fd, const size_t chunk) {
    const size_t size = (chunk > 0) ? chunk : DEFAULT_CHUNK;
    char *data, *ptr, *end;
    int file_fd, err;
    ssize_t bytes;

    // send the file name

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
    printf("The file: %s sent successfully.\n", file);
    return 0;
}

int
main(int argc, char *argv[]){ 
    
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
        printf("\nYou have connected to the fileserver...\n\n");

        // implementation of menu options and passing values to the server
        //printf("You chose option: %s\n", menu_selection(fd));
        menu_selection(fd);

        send_file(argv[2], fd, 0);

        return 0;
    }
}
