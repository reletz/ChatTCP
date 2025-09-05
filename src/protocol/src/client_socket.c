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

// Initialize socket and set up server address
int init(const char *server_ip, int server_port, int *client_socket, struct sockaddr_in *server_address, int *local_port){
  // Create a UDP socket. SOCK_DGRAM specifies a datagram socket.
  if ((*client_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    perror("socket(2)");
    return 0;
  }

  // Initialize the server address struct with zeros.
  memset(server_address, 0, sizeof(*server_address));

  // Fill the server address structure with the destination information.
  server_address->sin_family = AF_INET;
  server_address->sin_port = htons(server_port);

  if (inet_pton(AF_INET, server_ip, &server_address->sin_addr) <= 0){
    perror("inet_pton(2)");
    close(*client_socket);
    return 0;
  }

  // Get the local port assigned to the socket
  struct sockaddr_in local_address;
  socklen_t len = sizeof(local_address);
  if (getsockname(*client_socket, (struct sockaddr *)&local_address, &len) < 0){
    perror("getsockname(2)");
    *local_port = 0; // Use ephemeral port
  }
  else {
    *local_port = ntohs(local_address.sin_port);
    printf("Client using local port: %d\n", *local_port);
  }

  return 1;
}

// Helper function to initialize a packet with default values
void init_packet(packet *pkt, uint16_t src_port, uint16_t dst_port) {
  memset(pkt, 0, sizeof(packet));
  pkt->source_port = src_port;
  pkt->dest_port = dst_port;
  pkt->data_offset = 5; // 5 x 4 bytes = 20 bytes (standard TCP header size)
  pkt->window_size = 1024;
  pkt->urgent_pointer = 0;
  pkt->payload[0] = '\0';
}

// Perform three-way handshake with server
int connect(int client_socket, struct sockaddr_in *server_address, int local_port, int server_port) {
  // Create a SYN packet to initiate the handshake.
  packet syn_packet;
  init_packet(&syn_packet, local_port, server_port);
  syn_packet.seq_num = rand();
  syn_packet.ack_num = 0;
  syn_packet.flags = SYN;
  syn_packet.checksum = calculate_checksum(&syn_packet);

  int retries = 0;
  int handshake_complete = 0;

  while (retries < MAX_RETRIES && !handshake_complete){
    // Send the SYN packet to the server.
    if (sendto(client_socket, &syn_packet, sizeof(syn_packet), 0,
               (const struct sockaddr *)server_address, sizeof(*server_address)) < 0){
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

    if (sel < 0){
      if (errno == EINTR){
        continue;
      }
      perror("select(2)");
      break;
    }
    else if (sel == 0){
      printf("Timeout. No SYN-ACK received.\n");
      retries++;
      continue;
    }

    // The client will now wait for a SYN-ACK from the server.
    packet received_synack;
    socklen_t len = sizeof(*server_address);
    int n = recvfrom(client_socket, &received_synack, sizeof(received_synack), 0,
                     (struct sockaddr *)server_address, &len);

    if (n < 0){
      perror("recvfrom(2)");
      retries++;
      continue;
    }

    // Verify checksum
    uint16_t received_checksum = received_synack.checksum;
    received_synack.checksum = 0;
    if (calculate_checksum(&received_synack) != received_checksum){
      printf("Checksum verification failed. Packet might be corrupted.\n");
      retries++;
      continue;
    }

    // Check for the SYN-ACK flags
    if ((received_synack.flags & SYN) && (received_synack.flags & ACK)){
      printf("Received SYN-ACK from server. Server Seq: %u, Server Ack: %u\n",
             received_synack.seq_num, received_synack.ack_num);

      // Send the final ACK to complete the handshake.
      packet final_ack_packet;
      init_packet(&final_ack_packet, local_port, server_port);
      final_ack_packet.seq_num = received_synack.ack_num;
      final_ack_packet.ack_num = received_synack.seq_num + 1;
      final_ack_packet.flags = ACK;
      final_ack_packet.checksum = calculate_checksum(&final_ack_packet);

      printf("Sending final ACK to server...\n");
      if (sendto(client_socket, &final_ack_packet, sizeof(final_ack_packet), 0,
                 (const struct sockaddr *)server_address, len) < 0){
        perror("sendto(2)");
      }
      else {
        handshake_complete = 1;
        printf("Handshake complete. Connection established!\n");
      }
    }
    else {
      printf("Received an unexpected packet type.\n");
    }
  }

  if (!handshake_complete){
    printf("Handshake failed after %d retries. Exiting.\n", MAX_RETRIES);
    return 0;
  }

  return 1;
}

// Perform four-way handshake to terminate connection
int terminate(int client_socket, struct sockaddr_in *server_address, int local_port, int server_port){
  int termination_complete = 0;
  int retries = 0;

  // Send FIN packet to initiate termination
  packet fin_packet;
  init_packet(&fin_packet, local_port, server_port);
  fin_packet.seq_num = rand();
  fin_packet.ack_num = 0;
  fin_packet.flags = FIN;
  fin_packet.checksum = calculate_checksum(&fin_packet);

  while (retries < MAX_RETRIES && !termination_complete) {
    printf("Initiating connection termination. Sending FIN...\n");
    if (sendto(client_socket, &fin_packet, sizeof(fin_packet), 0,
               (const struct sockaddr *)server_address, sizeof(*server_address)) < 0){
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

    if (sel < 0){
      if (errno == EINTR){
        continue;
      }
      perror("select(2)");
      break;
    } else if (sel == 0) {
      printf("Timeout. No FIN-ACK received from server. Retrying...\n");
      retries++;
      continue;
    }

    // Receive the combined FIN-ACK packet from the server
    packet received_finack;
    socklen_t len = sizeof(*server_address);
    int n = recvfrom(client_socket, &received_finack, sizeof(received_finack), 0,
                     (struct sockaddr *)server_address, &len);

    if (n < 0) {
      perror("recvfrom(2)");
      retries++;
      continue;
    }

    // Verify checksum
    uint16_t received_checksum = received_finack.checksum;
    received_finack.checksum = 0;
    if (calculate_checksum(&received_finack) != received_checksum){
      printf("Checksum verification failed. Packet might be corrupted.\n");
      retries++;
      continue;
    }

    // Check if the received packet has both FIN and ACK flags set
    if ((received_finack.flags & FIN) && (received_finack.flags & ACK)) {
      printf("Received FIN-ACK from server. Sending final ACK...\n");

      // Send final ACK to complete the handshake
      packet final_ack;
      init_packet(&final_ack, local_port, server_port);
      final_ack.seq_num = received_finack.ack_num;
      final_ack.ack_num = received_finack.seq_num + 1;
      final_ack.flags = ACK;
      final_ack.checksum = calculate_checksum(&final_ack);

      if (sendto(client_socket, &final_ack, sizeof(final_ack), 0,
                 (const struct sockaddr *)server_address, len) < 0){
        perror("sendto(2)");
        retries++;
        continue;
      }

      termination_complete = 1;
      printf("Four-way termination handshake complete. Connection closed.\n");
    } else {
      printf("Received an unexpected packet type during termination. Retrying...\n");
      retries++;
      continue;
    }
  }

  if (!termination_complete) {
    printf("Connection termination failed after %d retries.\n", MAX_RETRIES);
    return 0;
  }

  return 1;
}

int main(int argc, char *argv[]){
  srand(time(NULL));
  int client_socket;
  struct sockaddr_in server_address;
  int local_port;

  const char *server_ip;
  int server_port;

  if (argc == 3){
    server_ip = argv[1];
    server_port = atoi(argv[2]);
    printf("Using command-line arguments: %s:%d\n", server_ip, server_port);
  }
  else {
    server_ip = SERVER_IP;
    server_port = PORT;
    printf("No arguments provided. Using default values: %s:%d\n", server_ip, server_port);
  }

  if (!init(server_ip, server_port, &client_socket, &server_address, &local_port)){
    return 1;
  }

  if (!connect(client_socket, &server_address, local_port, server_port)){
    close(client_socket);
    return 1;
  }

  // <exchange data>

  // Close the connection:
  if (!terminate(client_socket, &server_address, local_port, server_port)){
    printf("Failed to gracefully terminate the connection.\n");
  }

  close(client_socket);
  return 0;
}
