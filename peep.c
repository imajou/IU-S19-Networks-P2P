#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "peep.h"
#include "peep_log.h"
#include "peep_util.h"
#include "peep_struct.h"

s_peer_info *peer_info;
struct ifaddrs *interfaces;


void *request_file(void *vargp) {

    s_file_request_args *request_info = (s_file_request_args *) vargp;

    int sock_fd = 0;
    ssize_t sent_bytes = 0;
    int addr_len = 0;

    struct sockaddr_in req_addr;
    struct hostent *host = gethostbyname(request_info->ip);

    req_addr.sin_family = AF_INET;
    req_addr.sin_port = htons(request_info->port);
    req_addr.sin_addr = *((struct in_addr *) host->h_addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(sock_fd, (struct sockaddr *) &req_addr, sizeof(struct sockaddr));


    char buffer[1024];                      // Buffer for raw data
    memset(buffer, 0, sizeof(buffer));
    char file_data[1024];                   // Structured client data
    memset(file_data, 0, sizeof(file_data));

    const int request_code = 0;
    sent_bytes = sendto(sock_fd, &request_code, sizeof(request_code), 0,
                            (struct sockaddr *) &req_addr, sizeof(struct sockaddr));
    sleep(1);
    sent_bytes = sendto(sock_fd, request_info->filename, sizeof(request_info->filename), 0,
                        (struct sockaddr *) &req_addr, sizeof(struct sockaddr));
    sleep(1);

    recvfrom(sock_fd, (char *) buffer, sizeof(buffer), 0,
             (struct sockaddr *) &req_addr, &addr_len);
    int cli_word_count = *((int *) buffer);
    log_i(LOG_FILE_RECV, "Word count received: %i", cli_word_count);

    memset(buffer, 0, sizeof(buffer));
    memset(file_data, 0, sizeof(file_data));

    // Accept further connections for every word
    for (int word = 0; word < cli_word_count; ++word) {
        // Receive next word
        recvfrom(sock_fd, (char *) buffer, sizeof(buffer), 0,
                 (struct sockaddr *) &req_addr, &addr_len);
        log_i(LOG_FILE_RECV, "Received word: %s", buffer);
        // Store data
        strcat(file_data, buffer);
        strcat(file_data, " ");
        memset(buffer, 0, sizeof(buffer));
    }

    log_i(LOG_FILE_RECV, "Filename: %s", request_info->filename);
    // Write received data to a file
    FILE *recv_file = fopen(request_info->filename, "w");
    fprintf(recv_file, "%s", file_data);
    fclose(recv_file);

    pthread_exit(NULL);
}

void *request_sync(void *vargp) {
    s_file_request_args *server_info = (s_file_request_args *) vargp;

    int sock_fd = 0;
    ssize_t cli_sent_bytes = 0;

    struct sockaddr_in srv_addr;
    struct hostent *host = gethostbyname(server_info->ip);

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(server_info->port);
    srv_addr.sin_addr = *((struct in_addr *) host->h_addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(sock_fd, (struct sockaddr *) &srv_addr, sizeof(struct sockaddr));


    const int request_code = 1;
    cli_sent_bytes = sendto(sock_fd, &request_code, sizeof(request_code), 0,
                            (struct sockaddr *) &srv_addr, sizeof(struct sockaddr));
    if (cli_sent_bytes == 0) {
        close(sock_fd);
        log_i(LOG_SYNC_SEND, "Failed to connect to %s", server_info->ip);
        pthread_exit(NULL);
    }
    log_i(LOG_SYNC_SEND, "Request for sync sent, of total bytes %d to server %s", cli_sent_bytes,
          server_info->ip);


    cli_sent_bytes = sendto(sock_fd, &peer_info->_short_info, sizeof(peer_info->_short_info), 0,
                            (struct sockaddr *) &srv_addr, sizeof(struct sockaddr));
    if (cli_sent_bytes == 0) {
        close(sock_fd);
        log_i(LOG_SYNC_SEND, "Failed to connect to %s", server_info->ip);
        pthread_exit(NULL);
    }
    log_i(LOG_SYNC_SEND, "Peer info sent, of total bytes %d to server %s", cli_sent_bytes, server_info->ip);


    int number_of_peers = 0;
    cli_sent_bytes = sendto(sock_fd, &number_of_peers, sizeof(number_of_peers), 0,
                            (struct sockaddr *) &srv_addr, sizeof(struct sockaddr));
    if (cli_sent_bytes == 0) {
        close(sock_fd);
        log_i(LOG_SYNC_SEND, "Failed to connect to %s", server_info->ip);
        pthread_exit(NULL);
    }
    log_i(LOG_SYNC_SEND, "Number of peers sent, of total bytes %d to server %s", cli_sent_bytes,
          server_info->ip);


    while (1) {
        cli_sent_bytes = sendto(sock_fd, &peer_info->_full_info, sizeof(peer_info->_full_info), 0,
                                (struct sockaddr *) &srv_addr, sizeof(struct sockaddr));
        if (cli_sent_bytes == 0) {
            close(sock_fd);
            log_i(LOG_SYNC_SEND, "Failed to connect to %s", server_info->ip);
            pthread_exit(NULL);
        }
        sleep(3);
    }
}

/**
 * Thread to connect client with master socket passed
 *
 * Thread receives connection from the client to the dedicated socket,
 * receives count of words from the client, and receives words one by one.
 * Writes the data to receive.txt
 *
 * @param vargp Master socket file descriptor
 * @return nothing
 */
void *receive(void *vargp) {

    int *master_sock = (int *) vargp;   // Master socket

    int cli_sock = 0;                   // Client socket

    struct sockaddr_in cli_addr;        // Client ip
    socklen_t addr_len = 0;             // Client ip length
    ssize_t recv_bytes = 0, sent_bytes = 0;         // Bytes received counter

    char buffer[1024];                  // Buffer for raw data
    memset(buffer, 0, sizeof(buffer));
    char cli_data[1024];                // Structured client data
    memset(cli_data, 0, sizeof(cli_data));

    // Client socket assignment
    cli_sock =
            accept(*master_sock, (struct sockaddr *) &cli_addr, &addr_len);
    if (cli_sock < 0) {
        throw(LOG_SERVER, "Accept error");
    }

    log_i(LOG_SERVER, "Connection accepted from client : %s:%u",
          inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

    // Receive request code from client
    recv_bytes = recvfrom(cli_sock, (char *) buffer, sizeof(buffer), 0,
                              (struct sockaddr *) &cli_addr, &addr_len);
    log_i(LOG_SERVER, "Received %d bytes from client %s:%u.",
          recv_bytes, inet_ntoa(cli_addr.sin_addr),
          ntohs(cli_addr.sin_port));

    if (recv_bytes == 0) {
        close(cli_sock);
        pthread_exit(NULL);
    }

    int request_code = *((int *) buffer);

    log_i(LOG_SERVER, "Request code received: %i", request_code);
    if (request_code == 1) {
        // Save received peer info
        // Keep-alive connection
    }

    if (request_code == 0) {
        // Receive filename
        recv_bytes = recvfrom(cli_sock, (char *) buffer, sizeof(buffer), 0,
                                  (struct sockaddr *) &cli_addr, &addr_len);
        log_i(LOG_FILE_SEND, "Filename received: received %d bytes from client %s:%u.",
              recv_bytes, inet_ntoa(cli_addr.sin_addr),
              ntohs(cli_addr.sin_port));
        char *filename = malloc(sizeof(char) * strlen(buffer) + 16);
        strcpy(filename, buffer);
        // Send word count
        // Send words one by one

        log_i(LOG_FILE_SEND, "Accessing file %s", filename);

        if (access(filename, F_OK)) {
            log_i(LOG_FILE_SEND, "File %s does not exist", filename);
            pthread_exit(NULL);
        }
        if (access(filename, R_OK)) {
            log_i(LOG_FILE_SEND, "File %s has no access for read", filename);
            pthread_exit(NULL);
        }


        FILE *src_file = fopen(filename, "rb");
        fseek(src_file, 0l, SEEK_END);
        long src_file_size = ftell(src_file);
        fseek(src_file, 0l, SEEK_SET);

        char *src_file_data = malloc((size_t) (src_file_size + 1));
        fread(src_file_data, (size_t) src_file_size, 1, src_file);
        fclose(src_file);

        src_file_data[src_file_size] = 0;

        int src_file_word_count = count_words(src_file_data);

        log_i("asdasd", "Nice init");

        sent_bytes = sendto(cli_sock, &src_file_word_count, sizeof(src_file_word_count), 0,
                                (struct sockaddr *) &cli_addr, sizeof(struct sockaddr));

        log_i(LOG_FILE_SEND, "Word count sent = %d, of total bytes %d to server %s",
              src_file_word_count, sent_bytes, inet_ntoa(cli_addr.sin_addr));

        if (sent_bytes == 0) {
            close(cli_sock);
            pthread_exit(NULL);
        }

        char word_buff[64];
        memset(word_buff, 0, sizeof(word_buff));
        int word_index = 0, char_index = 0;
        while (1) {
            if (src_file_data[char_index] == ' ' || src_file_data[char_index] == '\0') {
                log_i(LOG_FILE_SEND, "Sending word_buff: %s", word_buff);
                sleep(1);
                sendto(cli_sock, &word_buff, sizeof(word_buff), 0,
                       (struct sockaddr *) &cli_addr, sizeof(struct sockaddr));
                word_index = 0;
                if (src_file_data[char_index] == '\0') break;
                char_index++;
                memset(word_buff, 0, sizeof(word_buff));
                continue;
            }
            word_buff[word_index++] = src_file_data[char_index++];
        }

        pthread_exit(NULL);

    }

    return NULL;
}


/**
 * Initialize listening server
 * Listen for incoming connections and create a new thread for each new
 * Number of connections is limited to 5
 *
 * @param vargp nothing
 * @return nothing
 */
void *srv_init(void *vargp) {

    int server_port = *(int *) vargp;

    peer_info = malloc(sizeof(peer_info));

    peer_info->port = server_port;
    getifaddrs(&interfaces);
    strcpy(peer_info->ip, inet_ntoa(((struct sockaddr_in *) interfaces->ifa_addr)->sin_addr));

    int test_fd;
    struct ifreq ifr;
    test_fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    snprintf(ifr.ifr_name, IFNAMSIZ, "en0");
    ioctl(test_fd, SIOCGIFADDR, &ifr);

    strcpy(peer_info->ip,
           inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));


    strcpy(peer_info->name, "glebasher");
    strcpy(peer_info->files, "kek.txt,rickrolled.txt,hellothere.txt");

    strcpy(peer_info->_full_info, "glebasher");
    strcat(peer_info->_full_info, ":");
    strcat(peer_info->_full_info, peer_info->ip);
    strcat(peer_info->_full_info, ":");
    char buffer[10];
    memset(&buffer, 0, sizeof(buffer)); // zero out the buffer
    sprintf(buffer, "%d", server_port);
    strcat(peer_info->_full_info, buffer);

    strcpy(peer_info->_short_info, peer_info->_full_info);
    strcat(peer_info->_full_info, ":");
    strcat(peer_info->_full_info, peer_info->files);

    log_i(LOG_SERVER, "Short info: %s", peer_info->_short_info);
    log_i(LOG_SERVER, "Full info: %s", peer_info->_full_info);

    int master_sock_tcp_fd = 0;

    fd_set readfds;

    struct sockaddr_in server_addr;

    if ((master_sock_tcp_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        throw(LOG_SERVER, "Socket creation failed");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(master_sock_tcp_fd, (struct sockaddr *) &server_addr,
             sizeof(struct sockaddr)) == -1) {
        log_i(LOG_SERVER, "Socket bind failed");
        pthread_exit(NULL);
    }

    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(master_sock_tcp_fd, (struct sockaddr *) &sin, &len) == -1)
        throw(LOG_SERVER, "Socket name acquiring failed");
    else
        log_i(LOG_SERVER, "Port assigned");

    log_i(LOG_SERVER, "Port number %d", ntohs(sin.sin_port));

    if (listen(master_sock_tcp_fd, 5) < 0) {
        throw(LOG_SERVER, "Listen failed");
        pthread_exit(NULL);
    }

    pthread_t threads[5];
    unsigned int thread_count = 0;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(master_sock_tcp_fd, &readfds);

        log_i(LOG_SERVER, "Waiting for connection");

        select(master_sock_tcp_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(master_sock_tcp_fd, &readfds)) {
            log_i(LOG_SERVER, "New connection received");
            //receive(master_sock_tcp_fd);
            pthread_create(&threads[thread_count], NULL, receive,
                           (void *) &master_sock_tcp_fd);
            pthread_join(threads[thread_count], NULL);
            thread_count++;
        } else break;
    }

    return 0;
}

/**
 * Parsing command from user input
 * @param input  User input
 */
void command_parse(char *input) {
    remove_all_chars(input, '\n');

    char *arg;
    arg = strtok(input, " ");

    printf("\n");

    if (!strcmp(arg, "help")) {
        print_help();
        return;
    }

    if (!strcmp(arg, "file")) {

        arg = strtok(NULL, " ");
        if (arg == NULL) {
            log_i(LOG_SHELL, "No filename specified");
            return;
        }
        char *filename = malloc(sizeof(arg));
        strcpy(filename, arg);

        log_i(LOG_SHELL, "Arg: %s\n", arg);
        log_i(LOG_SHELL, "Filename: %s\n", filename);

        char *address;
        arg = strtok(NULL, " ");
        if (arg != NULL) {
            address = malloc(sizeof(arg));
            strcpy(address, arg);
        } else address = "127.0.0.1";

        unsigned int port = 54777;
        arg = strtok(NULL, " ");
        if (arg != NULL) port = (unsigned int) atoi(arg);



        s_file_request_args *s = malloc(sizeof(s_file_request_args));
        s->ip = malloc(sizeof(address));
        strcpy(s->ip, address);
        s->port = port;
        s->filename = malloc(sizeof(filename));
        strcpy(s->filename, filename);

        log_i(LOG_SHELL, "Filename new: %s\n", s->filename);

        pthread_t c_tid = 0;
        pthread_create(&c_tid, NULL, request_file, (void *) s);
        return;
    }

    if (!strcmp(arg, "sync")) {

        char *address;
        arg = strtok(NULL, " ");
        if (arg != NULL) {
            address = malloc(sizeof(arg));
            strcpy(address, arg);
        } else address = "127.0.0.1";

        unsigned int port = 54777;
        arg = strtok(NULL, " ");
        if (arg != NULL) port = (unsigned int) atoi(arg);

        s_file_request_args *s = malloc(sizeof(s_file_request_args));
        s->ip = address;
        s->port = port;

        pthread_t c_tid = 0;
        pthread_create(&c_tid, NULL, request_sync,
                       (void *) s);
        return;
    }

    if (!strcmp(arg, "server")) {

        int *server_port = malloc(sizeof(int));
        *server_port = 54777;

        arg = strtok(NULL, " ");
        if (arg != NULL) {
            *server_port = atoi(arg);
        }
        log_i(LOG_SHELL, "Starting server on port %i", *server_port);
        pthread_t c_tid = 0;
        pthread_create(&c_tid, NULL, srv_init, server_port);
        sleep(1);
        return;
    }
}

/**
 * Shell to enter commands
 * @param command Command to be executed on init, may be NULL
 */
void init_glebash(char *command) {
    if (command != NULL) command_parse(command);
    print_help();
    while (1) {
        printf("\n" ANSI_COLOR_YELLOW "glebash-0.3$ " ANSI_COLOR_RESET);
        char input[64] = "";
        fgets(input, 64, stdin);
        if (!strcmp(input, "exit\n")) return;
        command_parse(input);
    }
}

int main(int argc, char **argv) {
    if (argc > 1) {
        char *command = malloc(0);
        for (int i = 1; i < argc; ++i) {
            command = realloc(command, sizeof(argv[i]) + 2);
            strcat(command, argv[i]);
            strcat(command, " ");
        }
        printf("%s\n", command);
        init_glebash(command);
        return 0;
    }
    init_glebash(NULL);
    return 0;
}
