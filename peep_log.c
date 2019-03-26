#include "peep_log.h"

void throw(const char *issuer, const char *message) {
    fprintf(stderr, "[E]: %s: %s \n", issuer, message);
    exit(errno);
}

void log_i(const char *issuer, const char *message, ...) {
    char done[1024];
    va_list arg;
    va_start(arg, message);
    vsprintf(done, message, arg);
    va_end(arg);

    printf(ANSI_COLOR_GRAY "[ " ANSI_COLOR_MAGENTA "%li" ANSI_COLOR_GRAY
           " | " ANSI_COLOR_GREEN "%s" ANSI_COLOR_GRAY
           " | " ANSI_COLOR_CYAN "Info" ANSI_COLOR_GRAY
           " ]: " ANSI_COLOR_RESET "%s \n",
           time(0), issuer, done);
}

void print_help() {
    printf(ANSI_COLOR_BLUE "Welcome to GleBash!\n" ANSI_COLOR_RESET);
    printf("List of available commands:\n");
    printf(ANSI_COLOR_CYAN "connect <dest-ip> <dest-port>" ANSI_COLOR_RESET " - connect to a server, <server-dest> is optional, default is 54777" "\n");
    printf(ANSI_COLOR_CYAN "start-server <server-port>" ANSI_COLOR_RESET " - start up a server, <server-port> is optional, default is 54777" "\n");
    printf(ANSI_COLOR_CYAN "help" ANSI_COLOR_RESET " - show this help" "\n");
    printf("\n");
}