#ifndef FILE_SERVER_H
#define FILE_SERVER_H      /* Prevent accidental double inclusion */

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#define PORT "5000"
#define BUFLEN 1500

#endif
