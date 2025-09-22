#include "file_server.h"

#define MAX_CLIENTS 10

void
sigchld_handler(int s) {
    (void)s; // quiet unused variable warning
    
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6
void
*get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr;
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr;
}

int
main(){
    // listen on fd, new connection on new_fd
    int fd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    struct pollfd pfds[MAX_CLIENTS + 1];
    pds[0].fd = fd;
    pfds[0].events = POLLIN | POLLPRI;

    char *intro_msg = "\nYou have connected to Tokka's fileserver...\n";
    char *exit_msg = "\nYou are disconnecting from Tokka's fileserver...\n";

    memset(&hints, 0, sizeof(hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my ip
    
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "Error: getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first one we can
    for (p = servinfo; p!= NULL; p = p->ai_next) {
        if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        if (bind(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "Error: server failed to bind\n");
        exit(EXIT_FAILURE);
    }

    if (listen(fd, MAX_CLIENTS) == -1){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    /* Responsible for reaping zombie processes that appear as the
        forked()ed cild processes exit */
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("Fileserver: waiting for connections...\n"
}
