//
// Created by Gleb Petrakov on 2019-03-20.
//

#ifndef PEEP2PEEP_P2P_STRUCTS_H
#define PEEP2PEEP_P2P_STRUCTS_H

typedef struct {
    char *ip;
    unsigned int port;
    char *filename;
} s_file_request_args;

typedef struct {
    char _short_info[128];
    char _full_info[256];

    char name[32];
    char ip[32];
    unsigned int port;
    char files[128];
} s_peer_info;

#endif //PEEP2PEEP_P2P_STRUCTS_H
