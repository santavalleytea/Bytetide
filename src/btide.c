#include "../include/net/packet.h"
#include "../include/peer/peer.h"
#include "../include/pkg/package.h"
#include "../include/chk/pkgchk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CONN 10

Config config;

// Listening socket, accepts connections from peers
void *accept_connections(void *arg) {
    int listen_socket = *(int *)arg;
    while (1) {
        int peer_socket = accept_connection(listen_socket);
        if (peer_socket >= 0) {
            printf("Accepted connection from peer.\n");
        }
    }
    return NULL;
}

// Print peers a user is connected to
void print_peers() {
    struct peer *current = get_peer_list();
    if (!current) {
        printf("Not connected to any peers\n");
    } else {
        printf("Connected to:\n\n");
        int count = 1;
        while (current) {
            printf("%d. %s:%d\n", count, current->ip, current->port);
            current = current->next;
            count++;
        }
    }
}

// Handles the arguments from FETCH
void handle_fetch(char *ip_port, char *identifier, char *hash, char *offset_str) {
    if (!ip_port || !identifier || !hash) {
        printf("Missing arguments from command\n");
        return;
    }

    char *ip = strtok(ip_port, ":");
    char *port_str = strtok(NULL, ":");
    if (!ip || !port_str) {
        printf("Invalid IP or port\n");
        return;
    }

    int port = atoi(port_str);
    struct peer *peer = find_peer(ip, port);
    if (!peer) {
        printf("Unable to request chunk, peer not in list\n");
        return;
    }

    struct package *pkg = find_package(identifier);
    if (!pkg) {
        printf("Unable to request chunk, package is not managed\n");
        return;
    }
    
    int chunk_index = -1;
    for (int i = 0; i < pkg->nchunks; i++) {
        if (strcmp(pkg->chunks[i].hash, hash) == 0) {
            chunk_index = i;
            break;
        }
    }

    if (chunk_index == -1) {
        printf("Unable to request chunk, chunk hash does not belong to package.\n");
        return;
    }

    int offset = offset_str ? atoi(offset_str) : 0;
    if (offset < 0 || offset >= pkg->chunks[chunk_index].size) {
        printf("Invalid offset value.\n");
        return;
    }

    // Send REQ packet to the peer
    struct btide_packet req_packet;
    req_packet.msg_code = PKT_MSG_REQ;
    req_packet.error = 0;
    snprintf((char *)req_packet.pl.data, sizeof(req_packet.pl.data), 
    "%u %s %s %s", offset, identifier, hash, offset_str ? offset_str : "0");

    if (send_packet(peer->socket, &req_packet) < 0) {
        printf("Failed to send request packet to peer.\n");
    } else {
        printf("Request packet sent to peer %s:%d\n", ip, port);
    }
}

struct chunk *fetch_chunk(const struct package *pkg, const char *hash, uint32_t offset) {
    for (int i = 0; i < pkg->nchunks; i++) {
        if (strcmp(pkg->chunks[i].hash, hash) == 0 && pkg->chunks[i].offset == offset) {
            return &pkg->chunks[i];
        }
    }
    return NULL;
}

int store_data(struct chunk *chk, const uint8_t *data, size_t data_len) {
    if (data_len > chk->size) {
        fprintf(stderr, "Data length exceeds chunk size\n");
        return -1;
    }

    memcpy(chk->data, data, data_len);
    return 0;
}

// Main handling of command flags
void handle_command(char *command) {
    char *token = strtok(command, " ");
    if (strcmp(token, "CONNECT") == 0) {
        char *ip_port = strtok(NULL, " ");
        if (!ip_port) {
            printf("Missing address and port argument.\n");
            return;
        }

        char *ip = strtok(ip_port, ":");
        char *port_str = strtok(NULL, ":");
        if (!ip || !port_str) {
            printf("Missing address and port argument.\n");
            return;
        }

        int port = atoi(port_str);
        if (connect_to_peer(ip, port) >= 0) {
            printf("Connection established with peer\n");
        } else {
            printf("Unable to connect to requested peer.\n");
        }
    } else if (strcmp(token, "DISCONNECT") == 0) {
        char *ip_port = strtok(NULL, " ");
        if (!ip_port) {
            printf("Missing address and port argument.\n");
            return;
        }

        char *ip = strtok(ip_port, ":");
        char *port_str = strtok(NULL, ":");
        if (!ip || !port_str) {
            printf("Missing address and port argument.\n");
            return;
        }

        int port = atoi(port_str);
        struct peer *peer = find_peer(ip, port);
        if (peer) {
            disconnect_peer(peer);
            printf("Disconnected from peer\n");
        } else {
            printf("Unknown peer, not connected.\n");
        }
    } else if (strcmp(token, "ADDPACKAGE") == 0) {
        char *filename = strtok(NULL, " ");
        if (!load_package(filename)) {
            printf("Failed to add package.\n");
        }
    } else if (strcmp(token, "REMPACKAGE") == 0) {
        char *ident = strtok(NULL, " ");
        if (!ident || strlen(ident) < 20) {
            printf("Missing identifier argument\n");
            return;
        }

        struct package *pkg = find_package(ident);
        if (!pkg) {
            printf("Identifier provided does not match managed packages.\n");
        } else {
            remove_package(pkg->identifier);
            printf("Package has been removed\n");
        }
    } else if (strcmp(token, "PACKAGES") == 0) {
        print_packages();
    } else if (strcmp(token, "PEERS") == 0) {
        print_peers();
    } else if (strcmp(token, "FETCH") == 0) {
        char *ip_port = strtok(NULL, " ");
        char *identifier = strtok(NULL, " ");
        char *hash = strtok(NULL, " ");
        char *offset = strtok(NULL, " ");
        if (!ip_port || !identifier || !hash) {
            printf("Missing arguments from command\n");
            return;
        }
        handle_fetch(ip_port, identifier, hash, offset);
    } else if (strcmp(token, "QUIT") == 0) {
        exit(0);
    } else {
        printf("Invalid command\n");
    }
}

void *monitor_disconnections(void *arg) {
    while (1) {
        struct peer *current = get_peer_list();
        while (current) {
            check_for_disconnection(current->socket);
            current = current->next;
        }
        sleep(1);
    }
    return NULL;
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        exit(1);
    }

    // Load the configuration file
    if (load_config(argv[1], &config) != 0) {
        fprintf(stderr, "Failed to load configuration file.\n");
        exit(1);
    }

    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(listen_socket, MAX_CONN) < 0) {
        perror("Listen failed");
        return 1;
    }

    pthread_t accept_thread, monitor_thread;
    pthread_create(&accept_thread, NULL, accept_connections, &listen_socket);
    pthread_create(&monitor_thread, NULL, monitor_disconnections, NULL);

    char command[5520];
    while (1) {
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0; // Remove trailing newline
        handle_command(command);
    }

    close(listen_socket);
    return 0;
}
