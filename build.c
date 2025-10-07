#include "build.h"

#define BACKLOG 10
#define MAX_CONN 10

// get sockaddr, IPv4 or IPv6
void
*get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &((struct sockaddr_in*)sa)->sin_addr;
    }

    return &((struct sockaddr_in6*)sa)->sin6_addr;
}

// create a TCP socket
// supply port value from a .conf - later task
// send to log ifle from .conf - later task
int
create_sock() {
    int fd, rv;
    struct addrinfo hints, *servinfo;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "Error - getaddrinfo: %s\n", gai_strerror(rv));
        exit(EXIT_FAILURE);
    }

    if ((fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)  {
        perror("socket: couldn't create");
        exit(EXIT_FAILURE);
        }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt: error setting options");
        exit(EXIT_FAILURE);
    }

    if (bind(fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(fd);
        perror("server: error binding");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);

    if (listen(fd, BACKLOG) == -1) {
        perror("listen: could not listen on fd"); 
        exit(EXIT_FAILURE);
    }

    return fd;
}

// function for receiving the file from a connected client
// make sure to only use returns
// grab the fileserver directory from .conf - later task
int
recv_file(int fd, const size_t chunk) {
    const size_t size = (chunk > 0) ? chunk : DEFAULT_CHUNK;
    char *data, *ptr, *end, *filename;
    char path[35] = "/etc/fileserver/";
    ssize_t bytes, total; // learn more about ssize_t use cases
    int file_fd, err;
    uint32_t exists, tmp_exists, answer, tmp_answer, length, tmp_length;

    // receive the file name length first
    read(fd, &tmp_length, sizeof(tmp_length));
    length = ntohl(tmp_length);

    // validate the length is reasonable
    if (length == 0 || length > 200) {
        fprintf(stderr, "Invalid filename length: %u\n", length);
        return -1;
    }
    
    filename = malloc(length);
    if (filename == NULL) {
        perror("malloc failed for filename");
        return -1;
    }
    
    if ((bytes = read(fd, filename, length)) != length) {
        fprintf(stderr, "Read %zd bytes for filename, expected %u\n", bytes, length);
        free(filename);
        return -1;
    }

    filename[length] = '\0';

    printf("Received filename: %s\n", filename);

    // ADD LOGIC FOR CONCAT FILENAME TO FILE SERVER DIR
    // get length from conf file
    strcat(path, filename);

    // change to be dynamic file name
    if ((file_fd = open(path, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) < 0) {
        if (errno == EEXIST) {
            if ((file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
                fprintf(stderr, "Copying file failed.\n");
                free(filename);
                printf("The errno was: %d\n", errno);
                return errno;
            }

            // handle asking the client about overwriting
            tmp_exists = 1;
            exists = htonl(tmp_exists);
            send(fd, &exists, sizeof(exists), 0);
            printf("The file already exists on the file server.\n");

            bytes = read(fd, &tmp_answer, sizeof(tmp_answer));
            if (bytes < 0) {
                if (bytes == -1)
                    err = errno;
                else
                    err = EIO;
                close(fd);
                close(file_fd);
                unlink(path);
                free(filename);
                return err;
            } 

            answer = ntohl(tmp_answer);

            // DEBUGGING
            printf("Answer variable value: %u\n", answer);

            // need to figure out what I want to return here
            if (answer != 1)
                return -9;
            else
                printf("The client is OK with overwrite...\n");

        }
        else {
            fprintf(stderr, "Copying file failed.\n");
            free(filename);
            printf("The errno was: %d\n", errno);
            return errno;
        }
    } 
    else {
        tmp_exists = 0;
        exists = htonl(tmp_exists);
        send(fd, &exists, sizeof(exists), 0);
    }
    
    data = malloc(size);
    if (!data) {
        close(fd);
        close(file_fd);
        // need to pass dynamic filename here
        unlink(path);
        free(filename);
        return ENOMEM;
    }
    
    // copy loop
    while(1) {
        bytes = read(fd, data, size);
        if (bytes < 0) {
            if (bytes == -1) 
                err = errno;
            else
                err = EIO;
            free(data);
            close(fd);
            close(file_fd);
            // need to pass dynamic filename here
            unlink(path);
            free(filename);
            return err;
        } 
        else {
            if (bytes == 0)
                break;
        }
        
        ptr = data;
        end = data + bytes;
        while (ptr < end) {
            bytes = write(file_fd, ptr, (size_t)(end - ptr));
            if (bytes < 0){
                if (bytes == -1)
                    err = errno;
                else
                    err = EIO;
                free(data);
                close(fd);
                close(file_fd);
                // need to pass dynamic filename here
                unlink(path);
                free(filename);
                return err;
            } 
            else {
                ptr += bytes;
                total += bytes;
            }
        }
    }

    free(data);

    err = 0;
    if (close(fd))
        err = EIO;
    if (close(file_fd))
        err = EIO;
    if (err) {
        // need to pass dynamic filename here
        unlink(path);
        return err;
    }

    printf("Received %zd bytes\n", total);
    printf("The file %s was copied successfully.\n", path);
    free(filename);
    return 0;
}

// function to create the poll server
void
create_pserv() {
    char path[35] = "/etc/fileserver/";
    struct stat fsdir;

    struct pollfd pfds[MAX_CONN];
    memset(pfds, 0, sizeof(pfds));

    // listen on fd, new connection on new_fd
    int fd, newfd, clients, error;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    
    sin_size = sizeof(their_addr);

    fd = create_sock();
    pfds[0].fd = fd;
    pfds[0].events = POLLIN | POLLOUT;
    clients = 1; // number of fds picked up via poll

    // create file server directory if needed
    if (mkdir(path, S_IRWXU | S_IRGRP | S_IROTH) == -1){
        if (errno == EEXIST) {
            if (stat(path, &fsdir) == -1) {
              perror("stat: file error");
              exit(EXIT_FAILURE);
            }
            
            if ((fsdir.st_mode & (S_IRWXU + S_IRGRP + S_IROTH)) == 0)  
                printf("File server directory perms: OK\n");
            else {
                printf("Correcting the file server directory perms...\n");
                if (chmod(path, fsdir.st_mode & (S_IRWXU + S_IRGRP + S_IROTH)) == -1) {
                    //fprintf(stderr, "chmod error: %d\n", errno);
                    perror("chmod: set perms error");
                    exit(EXIT_FAILURE);
                }
            }
        }
        else {
           fprintf(stderr, "mkdir failure: %d\n", errno);
           exit(EXIT_FAILURE);
        }
    }

    // can be sent to log file
    printf("File server has started, listening on port %s\n", PORT);
    
    while(1) {
        int poll_count = poll(pfds, clients, -1);

        if (poll_count == -1) {
            perror("poll_count: failed polling sockets");
            exit(EXIT_FAILURE);
        }

        if (pfds[0].revents & POLLIN) {
            // new connection
            newfd = accept(fd, (struct sockaddr *)&their_addr, &sin_size);
            if (newfd == -1) {
                perror("accept: error accepting socket");
                exit(EXIT_FAILURE);
            }

            inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
           
            // add connected fd to socket list
            pfds[clients].fd = newfd;
            pfds[clients].events = POLLIN | POLLOUT;
            clients++;

            // can be sent to log file
            printf("server: received a new connection from %s\n", s);
        
        } 
        else {
            // send to log
            fprintf(stderr,"Listener socket error during polling. revents == %d\n", pfds[0].revents);
            exit(EXIT_FAILURE);
        }

        for (int i = 1; i < clients; i++) {
            if (pfds[i].revents & POLLIN | POLLOUT) {
                // send to log
                printf("Found a communicating socket at position: %d\n", i);
                

                //read(pfds[i].fd, overwrite, sizeof(overwrite));
                //printf("overwrite value: %s\n", overwrite);

                if ((error = recv_file(pfds[i].fd, 0)) < 0)
                    fprintf(stderr, "recv_file: return value of %d\n", error);
            }
            // Disconnects handled here
            if (pfds[i].revents & POLLERR | POLLHUP){
                printf("The client has closed the connection.\n\n");
                pfds[i] = pfds[clients-1];
                clients--;
            }
        }
    }
    free(pfds);
}

int
main () {
    create_pserv();
}
