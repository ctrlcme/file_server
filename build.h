#ifndef FILE_SERVER_H
#define FILE_SERVER_H      /* Prevent accidental double inclusion */

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT "5000"
#define BUFLEN 100
#define MAX_SIZE 1024
#define DEFAULT_CHUNK 262144 /* 256k */

void *get_in_addr(struct sockaddr *sa);
int create_sock();
void create_pserv();

#endif
