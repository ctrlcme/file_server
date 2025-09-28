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

int
main(int argc, char *argv[]){ 
    
    if (argc != 3) {
        fprintf(stderr,"How to run:\n%s [SERVER] [FILE] \n", argv[0]);
        exit(EXIT_FAILURE);
    } else if (file_exists(argv[2]) == 0) {
        fprintf(stderr, "Usage error: The file %s is a directory.\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    int i, fd, file_fd, b;
    char buffer[BUFLEN];
    char sendbuffer[BUFLEN];

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
        /* I NEED TO FIGURE OUT HOW TO SEND AN ACTUAL FILE / WHOLE 
            * FILE, AM A LITTLE UNSURE. */
        read(fd, buffer, BUFLEN);
        printf("%s\n", buffer);
       
        // send the file name
        send(fd, argv[2], sizeof(argv[2]), 0);

        file_fd = open(argv[2], O_RDONLY);

        if (file_fd < 0) {
            fprintf(stderr, "Opening file path %s failed.\n", argv[2]);
            exit(EXIT_FAILURE);
        }
        
        while (b = read(file_fd, sendbuffer, BUFLEN) > 0) {
            // need to have actual bytes read sent, not BUFLEN
            send(fd, sendbuffer, sizeof(sendbuffer), 0);
        }
        
        printf("The file: %s sent successfully\n", argv[2]);
        close(file_fd);
        close(fd);
        return 0;
    }
}
