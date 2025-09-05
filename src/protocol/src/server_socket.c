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

#define PORT    12345 

int main(int argc, char *argv[]){
  int server_socket;
  packet received_packet;
  struct sockaddr_in server_address, client_address;

  int server_port;

  if (argc == 2) {
    server_port = atoi(argv[2]);
    printf("Using command-line arguments: %d\n", server_port);
  } else {
    server_port = PORT;
    printf("No arguments provided. Using default values: %d\n", server_port);
  }

  // Create a UDP socket. SOCK_DGRAM specifies a datagram socket
  if ((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket(2)");
    return 1;
  }

  memset(&server_address, 0, sizeof(server_address)); 
  memset(&client_address, 0, sizeof(client_address)); 

  // Fill the server address structure
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(server_port);
  server_address.sin_addr.s_addr = INADDR_ANY;

  // Bind the socket to the server address
  if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0){
    perror("bind(2)");
    close(server_socket);
    return 1;
  };

  printf("Server is listening on port %d...\n", server_port);

  socklen_t len;
  len = sizeof(client_address);
  memset(&client_address, 0, len);

  int n;

  // Main loop to continuously receive packets
  while (1){
    // Receive SYN
    n = recvfrom(server_socket, &received_packet, sizeof(received_packet), 0, &client_address, &len);

    if (n < 0) {
      perror("recvfrom(2)");
      continue;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("Packet received from %s:%d\n", client_ip, ntohs(client_address.sin_port));

    if (received_packet.flags & SYN){
      // Send SYN-ACK Packet
      packet synack;

      synack.seq_num = 12345;
      synack.ack_num = received_packet.seq_num + 1;
      synack.flags = SYN | ACK;

      if (sendto(server_socket, &synack, sizeof(packet), 0, &client_address, &len) < 0) perror("sendto(2)");
    }

    close(server_socket);
    return 0;
  }
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