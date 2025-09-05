// server_socket.c
// https://www.geeksforgeeks.org/cpp/udp-server-client-implementation-c/
// https://www.cs.rpi.edu/academics/courses/spring98/netprog/lectures/ppthtml/sockets/sld001.htm

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "packet.h"

#define PORT 12345

// Initialize socket and bind to server address
int initialize_server_socket(int server_port, int *server_socket, struct sockaddr_in *server_address){
  // Create a UDP socket. SOCK_DGRAM specifies a datagram socket
  if ((*server_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("socket(2)");
    return 0;
  }

  memset(server_address, 0, sizeof(*server_address));

  // Fill the server address structure
  server_address->sin_family = AF_INET;
  server_address->sin_port = htons(server_port);
  server_address->sin_addr.s_addr = INADDR_ANY;

  // Bind the socket to the server address
  if (bind(*server_socket, (struct sockaddr *)server_address, sizeof(*server_address)) < 0){
    perror("bind(2)");
    close(*server_socket);
    return 0;
  }

  printf("Server is listening on port %d...\n", server_port);
  return 1;
}

// Handle the three-way handshake with a client
int handle_handshake(int server_socket, struct sockaddr_in *client_address){
  packet received_packet;
  socklen_t len = sizeof(*client_address);
  int n;

  // Receive SYN
  n = recvfrom(server_socket, &received_packet, sizeof(received_packet), 0,
               (struct sockaddr *)client_address, &len);

  if (n < 0)
  {
    perror("recvfrom(2)");
    return 0;
  }

  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(client_address->sin_addr), client_ip, INET_ADDRSTRLEN);
  printf("Packet received from %s:%d\n", client_ip, ntohs(client_address->sin_port));

  if (received_packet.flags & SYN){
    // Send SYN-ACK Packet
    packet synack;

    synack.seq_num = 12345;
    synack.ack_num = received_packet.seq_num + 1;
    synack.flags = SYN | ACK;

    if (sendto(server_socket, &synack, sizeof(packet), 0,
               (struct sockaddr *)client_address, len) < 0){
      perror("sendto(2)");
      return 0;
    }
    else {
      // Wait for final ACK to complete the handshake
      packet final_ack;

      printf("Waiting for final ACK from client...\n");
      n = recvfrom(server_socket, &final_ack, sizeof(final_ack), 0,
                   (struct sockaddr *)client_address, &len);

      if (n < 0) {
        perror("recvfrom(2)");
        return 0;
      }
      else if (final_ack.flags & ACK) {
        printf("Handshake complete. Connection established!\n");
        return 1;
      } else {
        printf("Received unexpected packet type instead of ACK.\n");
        return 0;
      }
    }
  }

  return 0;
}

// Handle the four-way handshake for connection termination
int handle_connection_termination(int server_socket, struct sockaddr_in *client_address){
  packet received_packet;
  socklen_t len = sizeof(*client_address);
  int n;

  // Wait for FIN from client
  printf("Waiting for FIN packet from client...\n");
  n = recvfrom(server_socket, &received_packet, sizeof(received_packet), 0,
               (struct sockaddr *)client_address, &len);

  if (n < 0){
    perror("recvfrom(2)");
    return 0;
  }

  if (received_packet.flags & FIN){
    printf("Received FIN from client. Client initiating connection termination.\n");

    // Server sends ACK|FIN
    packet fin_ack_packet;
    fin_ack_packet.seq_num = 12345;
    fin_ack_packet.ack_num = received_packet.seq_num + 1;
    fin_ack_packet.flags = FIN|ACK;

    printf("Sending ACK|FIN in response to client's FIN...\n");
    if (sendto(server_socket, &ack_packet, sizeof(packet), 0,
               (struct sockaddr *)client_address, len) < 0){
      perror("sendto(2)");
      return 0;
    }

    // Wait for final ACK from client
    packet final_ack;
    printf("Waiting for final ACK from client...\n");
    n = recvfrom(server_socket, &final_ack, sizeof(final_ack), 0,
                 (struct sockaddr *)client_address, &len);

    if (n < 0){
      perror("recvfrom(2)");
      return 0;
    }

    if (final_ack.flags & ACK ){
      printf("Received final ACK. Connection terminated successfully.\n");
      return 1;
    } else {
      printf("Expected ACK but received different packet type.\n");
      return 0;
    }
  }
  else{
    printf("Expected FIN but received different packet type.\n");
    return 0;
  }
}

int main(int argc, char *argv[]){
  int server_socket;
  struct sockaddr_in server_address, client_address;
  int server_port;

  if (argc == 2){
    server_port = atoi(argv[1]);
    printf("Using command-line arguments: %d\n", server_port);
  }
  else {
    server_port = PORT;
    printf("No arguments provided. Using default values: %d\n", server_port);
  }

  if (!initialize_server_socket(server_port, &server_socket, &server_address)){
    return 1;
  }

  memset(&client_address, 0, sizeof(client_address));

  // Main loop to continuously receive packets
  while (1){
    if (handle_handshake(server_socket, &client_address)){
      printf("Connection established with client\n");

      // <Data exchange>

      // Close the connection:
      if (handle_connection_termination(server_socket, &client_address)){
        printf("Connection terminated gracefully\n");
      }
      else{
        printf("Connection termination failed\n");
      }
    }
  }

  close(server_socket);
  return 0;
}

/*
Creating a Socket
int socket(int family, int type, int proto);

family specifies the protocol family (PF_INET for TCP/IP).
type specifies the type of service (SOCK_STREAM, SOCK_DGRAM).
protocol specifies the specific protocol (usually 0 which means the default).
*/

/*
calling bind() assigns the address specified by the sockaddr structure to the socket descriptor.
You can give bind() a sockaddr_in structure:
bind(mysock, (struct sockaddr*)&myaddr, sizeof(myaddr) );
*/

/*
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                struct sockaddr *src_addr, socklen_t *addrlen)
Receive a message from the socket.
*/

/*
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
             const struct sockaddr *dest_addr, socklen_t addrlen)
Send a message on the socket
*/