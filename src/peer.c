#include "../include/peer/peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

static struct peer *peer_list = NULL;

// Function to add a peer
void add_peer(struct peer *new_peer) {
    // List Traversal, keeping tack of peers
    new_peer->next = NULL;
    if (!peer_list) {
        peer_list = new_peer;
    } else {
        struct peer *current = peer_list;
        while (current->next) {
            current = current->next;
        }
        current->next = new_peer;
    }
}

// Function to remove a peer
void remove_peer(struct peer *peer_to_remove) {
    struct peer **current = &peer_list;
    // Find peer to remove
    while (*current && *current != peer_to_remove) {
        current = &(*current)->next;
    }
    if (*current) {
        struct peer *to_free = *current;
        // Unlink peer
        *current = (*current)->next;
        close(to_free->socket);
        free(to_free);
    }
}

// Function to find a peer by IP and port
struct peer *find_peer(const char *ip, int port) {
    struct peer *current = peer_list;
    while (current) {
        // Compare IP and Port Number
        if (strcmp(current->ip, ip) == 0 && current->port == port) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Function to get the peer list
struct peer *get_peer_list() {
    return peer_list;
}

// Function to connect to a peer
int connect_to_peer(const char *ip, int port) {
    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Server address structure
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // Set address family
    server_addr.sin_port = htons(port); // Set port number
    // Convert IP address to binary
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(sockfd);
        return -1;
    }

    // Connect to Server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return -1;
    }

    // Add peer to the list
    struct peer *new_peer = (struct peer *)malloc(sizeof(struct peer));
    strcpy(new_peer->ip, ip); // Copy IP
    new_peer->port = port; // Set Port
    new_peer->socket = sockfd; // Set Socket File Descriptor
    add_peer(new_peer);

    return sockfd;
}

// Function to accept a connection
int accept_connection(int listening_socket) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    // Accept Connection
    int new_socket = accept(listening_socket, (struct sockaddr *)&client_addr, &addr_len);
    if (new_socket < 0) {
        perror("Accept failed");
        return -1;
    }

    // Allocate memory for the new peer
    struct peer *new_peer = (struct peer *)malloc(sizeof(struct peer));
    inet_ntop(AF_INET, &client_addr.sin_addr, new_peer->ip, INET_ADDRSTRLEN);
    new_peer->port = ntohs(client_addr.sin_port);
    new_peer->socket = new_socket;
    add_peer(new_peer);

    return new_socket;
}

// Function to disconnect a peer
void disconnect_peer(struct peer *peer) {
    struct btide_packet dsn_packet;
    dsn_packet.msg_code = PKT_MSG_DSN;
    dsn_packet.error = 0;
    memset(dsn_packet.pl.data, 0, PAYLOAD_MAX);

    send(peer->socket, &dsn_packet, sizeof(dsn_packet), 0);
    close(peer->socket);
    remove_peer(peer);
}

void check_for_disconnection(int peer_socket) {
    char buffer[1];
    int result = recv(peer_socket, buffer, sizeof(buffer), MSG_PEEK);
    if (result == 0) {
        // Connection closed by peer
        struct peer *peer = peer_list;
        while (peer) {
            if (peer->socket == peer_socket) {
                printf("Peer disconnected.\n");
                remove_peer(peer);
                break;
            }
            peer = peer->next;
        }
    } else if (result < 0) {
        perror("recv");
    }
}
