#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#include "packet.h"

#define SERVER_IP "127.0.0.1"
#define PORT 12345
#define MAX_RETRIES 3
#define TIMEOUT_SEC 2

int main(int argc, char *argv[]){
  int client_socket;
  packet received_packet;
  struct sockaddr_in server_address;

  const char *server_ip;
  int server_port;

  if (argc == 3) {
    server_ip = argv[1];
    server_port = atoi(argv[2]);
    printf("Using command-line arguments: %s:%d\n", server_ip, server_port);
  } else {
    server_ip = SERVER_IP;
    server_port = PORT;
    printf("No arguments provided. Using default values: %s:%d\n", server_ip, server_port);
  }

  // Create a UDP socket. SOCK_DGRAM specifies a datagram socket.
  if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    perror("socket(2)");
    return 1;
  }

  // Initialize the server address struct with zeros.
  memset(&server_address, 0, sizeof(server_address));

  // Fill the server address structure with the destination information.
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(server_port);

  if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0){
    perror("inet_pton(2)");
    close(client_socket);
    return 1;
  }

  // Create a SYN packet to initiate the handshake.
  packet syn_packet;
  syn_packet.seq_num = 54321;
  syn_packet.ack_num = 0;
  syn_packet.flags = SYN;

  int retries = 0;
  int handshake_complete = 0;

  while (retries < MAX_RETRIES && !handshake_complete){
    // Send the SYN packet to the server.
    if (sendto(client_socket, &syn_packet, sizeof(syn_packet), 0, (const struct sockaddr *)&server_address, sizeof(server_address)) < 0){
      perror("sendto(2)");
      retries++;
      continue;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(client_socket, &read_fds);
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    int sel = select(client_socket + 1, &read_fds, NULL, NULL, &timeout);

    if (sel < 0) {
      if (errno == EINTR) { continue; }
        perror("select(2)");
        break;
    } else if (sel == 0) {
      printf("Timeout. No SYN-ACK received.\n");
      retries++;
      continue;
    }

    // The client will now wait for a SYN-ACK from the server.
    packet received_synack;
    socklen_t len = sizeof(server_address);
    int n = recvfrom(client_socket, &received_synack, sizeof(received_synack), 0, (struct sockaddr *)&server_address, &len);

    if (n < 0){
      perror("recvfrom(2)");
      retries++;
      continue;
    }

    // Check for the SYN-ACK flags
    if ((received_synack.flags & SYN) && (received_synack.flags & ACK)) {
      printf("Received SYN-ACK from server. Server Seq: %u, Server Ack: %u\n", received_synack.seq_num, received_synack.ack_num);

      // Send the final ACK to complete the handshake.
      packet final_ack_packet;
      final_ack_packet.seq_num = received_synack.ack_num;
      final_ack_packet.ack_num = received_synack.seq_num + 1;
      final_ack_packet.flags = ACK;
        
      printf("Sending final ACK to server...\n");
      if (sendto(client_socket, &final_ack_packet, sizeof(final_ack_packet), 0, (const struct sockaddr *)&server_address, len) < 0) {
        perror("sendto(2)");
      } else {
        handshake_complete = 1;
        printf("Handshake complete. Connection established!\n");
      }
    } else {
      printf("Received an unexpected packet type.\n");
    }
  }

  if (!handshake_complete) {
    printf("Handshake failed after %d retries. Exiting.\n", MAX_RETRIES);
  }

  close(client_socket);
  return 0;
}
