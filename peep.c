#include "peep.h"
#include "peep_log.h"
#include "peep_util.h"
#include "peep_struct.h"

#define SERVER_PORT 54777

/**
 * Thread to connect client with master socket passed
 *
 * Thread receives connection from the client to the dedicated socket,
 * receives count of words from the client, and receives words one by one.
 * Writes the data to receive.txt
 *
 * @param mater_sock Master socket file descriptor
 * @return
 */
void *srv_accept_client(void *vargp) {

    int *master_sock = (int *) vargp;   // Master socket

    int cli_sock = 0;                   // Client socket

    struct sockaddr_in cli_addr;        // Client address
    socklen_t addr_len = 0;             // Client address length
    ssize_t srv_recv_bytes = 0;         // Bytes received counter

    char buffer[1024];                  // Buffer for raw data
    memset(buffer, 0, sizeof(buffer));
    char cli_data[1024];                // Structured client data
    memset(cli_data, 0, sizeof(cli_data));

    // Client socket assignment
    cli_sock =
            accept(*master_sock, (struct sockaddr *) &cli_addr, &addr_len);
    if (cli_sock < 0) {
        throw(LOG_NAME_CLIENT, "Accept error");
    }

    log_i(LOG_NAME_SERVER, "Connection accepted from client : %s:%u",
          inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

    // Receive word count from client
    srv_recv_bytes = recvfrom(cli_sock, (char *) buffer, sizeof(buffer), 0,
                              (struct sockaddr *) &cli_addr, &addr_len);
    log_i(LOG_NAME_SERVER, "Received %d bytes from client %s:%u.",
          srv_recv_bytes, inet_ntoa(cli_addr.sin_addr),
          ntohs(cli_addr.sin_port));

    if (srv_recv_bytes == 0) {
        close(cli_sock);
        pthread_exit(NULL);
    }

    // Init word count for further iteration
    int cli_word_count = *((int *) buffer);
    log_i(LOG_NAME_SERVER, "Word count received: %i", cli_word_count);

    memset(buffer, 0, sizeof(buffer));
    memset(cli_data, 0, sizeof(cli_data));

    // Accept further connections for every word
    for (int word = 0; word < cli_word_count; ++word) {
        // Receive next word
        recvfrom(cli_sock, (char *) buffer, sizeof(buffer), 0,
                 (struct sockaddr *) &cli_addr, &addr_len);
        log_i(LOG_NAME_SERVER, "Received word: %s", buffer);
        // Store data
        strcat(cli_data, buffer);
        strcat(cli_data, " ");
        memset(buffer, 0, sizeof(buffer));
    }

    // Write received data to a file
    FILE *recv_file = fopen("receive.txt", "w");
    fprintf(recv_file, "%s", cli_data);
    fclose(recv_file);

    return NULL;
}


/**
 * Send message to the server
 * Message contains initial packet of number of words within a file
 * Then, sequentially sent packages for every word
 *
 * @param address Address of the server to send request to
 * @param port Server port
 * @return
 */
void *cli_send_message(void *vargp) {

    s_connect_args *server_info = (s_connect_args *) vargp;

    int sock_fd = 0;
    ssize_t cli_sent_bytes = 0;

    struct sockaddr_in srv_addr;
    struct hostent *host = gethostbyname(server_info->address);

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(server_info->port);
    srv_addr.sin_addr = *((struct in_addr *) host->h_addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(sock_fd, (struct sockaddr *) &srv_addr, sizeof(struct sockaddr));

    if (access("source.txt", F_OK)) throw(LOG_NAME_CLIENT, "File source.txt does not exist");
    if (access("source.txt", R_OK)) throw(LOG_NAME_CLIENT, "File source.txt has no access for read");
    FILE *src_file = fopen("source.txt", "rb");
    fseek(src_file, 0l, SEEK_END);
    long src_file_size = ftell(src_file);
    fseek(src_file, 0l, SEEK_SET);

    char *src_file_data = malloc((size_t) (src_file_size + 1));
    fread(src_file_data, (size_t) src_file_size, 1, src_file);
    fclose(src_file);

    src_file_data[src_file_size] = 0;

    int src_file_word_count = count_words(src_file_data);

    cli_sent_bytes = sendto(sock_fd, &src_file_word_count, sizeof(src_file_word_count), 0,
                            (struct sockaddr *) &srv_addr, sizeof(struct sockaddr));

    log_i(LOG_NAME_CLIENT, "Word count sent = %d, of total bytes %d to server %s",
          src_file_word_count, cli_sent_bytes, server_info->address);

    if (cli_sent_bytes == 0) {
        close(sock_fd);
        pthread_exit(NULL);
    }

    char word_buff[64];
    memset(word_buff, 0, sizeof(word_buff));
    int word_index = 0, char_index = 0;
    while (1) {
        if (src_file_data[char_index] == ' ' || src_file_data[char_index] == '\0') {
            log_i(LOG_NAME_CLIENT, "Sending word_buff: %s", word_buff);
            sleep(1);
            sendto(sock_fd, &word_buff, sizeof(word_buff), 0,
                   (struct sockaddr *) &srv_addr, sizeof(struct sockaddr));
            word_index = 0;
            if (src_file_data[char_index] == '\0') break;
            char_index++;
            memset(word_buff, 0, sizeof(word_buff));
            continue;
        }
        word_buff[word_index++] = src_file_data[char_index++];
    }

    free(vargp);

    return 0;
}


/**
 * Initialize listening server
 * Listen for incoming connections and create a new thread for each new
 * Number of connections is limited to 5
 *
 * @param vargp
 * @return
 */
void *srv_init(void *vargp) {
    int master_sock_tcp_fd = 0;

    fd_set readfds;

    struct sockaddr_in server_addr;

    if ((master_sock_tcp_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        throw(LOG_NAME_SERVER, "Socket creation failed");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(master_sock_tcp_fd, (struct sockaddr *) &server_addr,
             sizeof(struct sockaddr)) == -1) {
        log_i(LOG_NAME_SERVER, "Socket bind failed");
        pthread_exit(NULL);
    }

    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(master_sock_tcp_fd, (struct sockaddr *) &sin, &len) == -1)
        throw(LOG_NAME_SERVER, "Socket name acquiring failed");
    else
        log_i(LOG_NAME_SERVER, "Port assigned");

    log_i(LOG_NAME_SERVER, "Port number %d", ntohs(sin.sin_port));

    if (listen(master_sock_tcp_fd, 5) < 0) {
        throw(LOG_NAME_SERVER, "Listen failed");
        pthread_exit(NULL);
    }

    pthread_t threads[5];
    unsigned int thread_count = 0;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(master_sock_tcp_fd, &readfds);

        log_i(LOG_NAME_SERVER, "Waiting for connection");

        select(master_sock_tcp_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(master_sock_tcp_fd, &readfds)) {
            log_i(LOG_NAME_SERVER, "New connection received");
            //srv_accept_client(master_sock_tcp_fd);
            pthread_create(&threads[thread_count], NULL, srv_accept_client,
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

    if (!strcmp(arg, "connect")) {

        arg = strtok(NULL, " ");
        if (arg == NULL) {
            log_i(LOG_NAME_SHELL, "No server address specified");
            return;
        }
        char *address = malloc(sizeof(arg));
        strcpy(address, arg);

        unsigned int port = 54777;
        arg = strtok(NULL, " ");
        if (arg != NULL)
            port = (unsigned int) atoi(arg);


        s_connect_args *s = malloc(sizeof(s_connect_args));
        s->address = address;
        s->port = port;

        pthread_t c_tid = 0;
        pthread_create(&c_tid, NULL, cli_send_message,
                       (void *) s);
        return;
    }

    if (!strcmp(arg, "start-server")) {

        int *server_port = malloc(sizeof(int));
        *server_port = 54777;

        arg = strtok(NULL, " ");
        if (arg != NULL) {
            *server_port = atoi(arg);
        }
        log_i(LOG_NAME_SHELL, "Starting server on port %i", *server_port);
        pthread_t c_tid = 0;
        pthread_create(&c_tid, NULL, srv_init, NULL);
        sleep(1);
        return;
    }
}

/**
 * Shell to enter commands
 * @param command Command to be executed on init, may be NULL
 * @return
 */
int init_glebash(char *command) {
    if (command != NULL) command_parse(command);
    print_help();
    while (1) {
        printf(ANSI_COLOR_YELLOW "glebash-0.3$ " ANSI_COLOR_RESET);
        char input[64] = "";
        fgets(input, 64, stdin);
        if (!strcmp(input, "exit\n")) return 0;
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
