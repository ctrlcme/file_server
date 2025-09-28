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
    int status, fd, rv;
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
// later task
int
recv_file(int fd, char* buff) {
    return 0;
}

// function to create the poll server
void
create_pserv() {
    char *intro_msg = "\nYou have connected to Tokka's fileserver...\n";
    char *exit_msg = "\nYou are disconnecting from Tokka's fileserver...\n";

    struct pollfd pfds[MAX_CONN];
    // initially I was multiplying sizeof by MAX_CONN but 
    // memset was not happy with it
    memset(pfds, 0, sizeof(pfds));

    // listen on fd, new connection on new_fd
    int fd, newfd, clients, x;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    
    sin_size = sizeof(their_addr);

    fd = create_sock();
    pfds[0].fd = fd;
    pfds[0].events = POLLIN | POLLOUT;
    clients = 1; // number of fds picked up via poll

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
                char buff[MAX_SIZE + 1];
                char filename[BUFLEN];
                int file_fd, total, b;
                // send to log
                printf("Found a communicating socket at position: %d\n", i);
                // THIS PART MIGHT NEED CHANGED FOR FILE TRANSFER
                //x = recv_file(pfds[i].fd, buff);
                send(pfds[i].fd, intro_msg, strlen(intro_msg), 0);
                
                // receive the file name
                read(pfds[i].fd, filename, BUFLEN);
                // change to be dynamic file name
                if ((file_fd = open("/home/tokka/copiedfile", O_CREAT | O_WRONLY  )) < 0) {
                    printf("Copying file failed.\n");
                    exit(EXIT_FAILURE);
                }

                total = 0;
                // think I need to do something different with the while
                // I was also passing MAX_SIZE instead of sizeof()
                // before...
                while (b = read(pfds[i].fd, buff, sizeof(buff)) > 0) {
                    total += b;
                    write(file_fd, buff, sizeof(buff));
                }

                printf("Received %d bytes\n", total);

                close(pfds[i].fd);
            }
            // I'd like to handle disconnects here...
            if (pfds[i].revents & POLLERR | POLLHUP){
                printf("The client has closed the connection.\n");
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
