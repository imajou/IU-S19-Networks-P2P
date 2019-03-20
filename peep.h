#ifndef PEEP2PEEP_PEEP_H
#define PEEP2PEEP_PEEP_H

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_GRAY "\x1b[90m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define LOG_NAME_SERVER "SERVER"
#define LOG_NAME_CLIENT "CLIENT"

#include <arpa/inet.h>
#include <errno.h>
#include <memory.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <zconf.h>
#include <time.h>
#include <unistd.h>
#include <pthread/pthread.h>

#endif //PEEP2PEEP_PEEP_H
