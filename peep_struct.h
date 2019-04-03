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
    char _short_info[1024];
    char _full_info[2014];

    char name[1024];
    char ip[1024];
    unsigned int port;
    char files[1024];
} s_self_info;

typedef struct {
    char ip[1024];
    char name[1024];
    char files[1024];
    unsigned int port;

    int count;
} s_saved_peer;

#endif //PEEP2PEEP_P2P_STRUCTS_H
