#include "../include/net/packet.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define PACKET_SIZE 4096

int send_packet(int socket, struct btide_packet *packet) {
    uint8_t buffer[PACKET_SIZE];
    memset(buffer, 0, PACKET_SIZE);

    // Serialize the packet
    uint16_t msg_code = htons(packet->msg_code);
    uint16_t error = htons(packet->error);

    memcpy(buffer, &msg_code, sizeof(msg_code));
    memcpy(buffer + sizeof(msg_code), &error, sizeof(error));
    memcpy(buffer + sizeof(msg_code) + sizeof(error), packet->pl.data, sizeof(packet->pl.data));

    // Send the packet over the socket
    ssize_t sent_bytes = send(socket, buffer, PACKET_SIZE, 0);
    if (sent_bytes != PACKET_SIZE) {
        perror("send");
        return -1;
    }

    return 0;
}

int receive_packet(int socket, struct btide_packet *packet) {
    uint8_t buffer[PACKET_SIZE];
    ssize_t received_bytes = recv(socket, buffer, PACKET_SIZE, 0);
    if (received_bytes <= 0) {
        if (received_bytes == 0) {
            printf("Peer has closed the connection\n");
        } else {
            perror("recv");
        }
        return -1;
    }

    if (received_bytes != PACKET_SIZE) {
        fprintf(stderr, "Incomplete packet received\n");
        return -1;
    }

    // Deserialize the packet
    memcpy(&packet->msg_code, buffer, sizeof(packet->msg_code));
    memcpy(&packet->error, buffer + sizeof(packet->msg_code), sizeof(packet->error));
    memcpy(packet->pl.data, buffer + sizeof(packet->msg_code) + sizeof(packet->error), sizeof(packet->pl.data));

    packet->msg_code = ntohs(packet->msg_code);
    packet->error = ntohs(packet->error);

    return 0;
}
