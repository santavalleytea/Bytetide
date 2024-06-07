#ifndef PEER_H
#define PEER_H

#include <arpa/inet.h>
#include "../net/packet.h"

#define INET_ADDRSTRLEN 16

struct peer {
    char ip[INET_ADDRSTRLEN];
    int port;
    int socket;
    struct peer *next;
};

// Function declarations
void add_peer(struct peer *new_peer);
void remove_peer(struct peer *peer_to_remove);
struct peer *find_peer(const char *ip, int port);
struct peer *get_peer_list();
int connect_to_peer(const char *ip, int port);
int accept_connection(int listening_socket);
void disconnect_peer(struct peer *peer);
void check_for_disconnection(int peer_socket);

#endif 
