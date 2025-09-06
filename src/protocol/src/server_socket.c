#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <errno.h>

#include <time.h>

#include "packet.h"
#include "client_manager.h"
#include "flow_control.h"

#define PORT 12345

// Initialize socket and bind to server address
int init(int server_port, int *server_socket, struct sockaddr_in *server_address)
{
  if ((*server_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("socket(2)");
    return 0;
  }
  memset(server_address, 0, sizeof(*server_address));
  server_address->sin_family = AF_INET;
  server_address->sin_port = htons(server_port);
  server_address->sin_addr.s_addr = INADDR_ANY;

  if (bind(*server_socket, (struct sockaddr *)server_address, sizeof(*server_address)) < 0)
  {
    perror("bind(2)");
    close(*server_socket);
    return 0;
  }

  printf("Server is listening on port %d...\n", server_port);
  return 1;
}

// Helper function to initialize a packet with default values
void init_packet(packet *pkt, uint16_t src_port, uint16_t dst_port)
{
  memset(pkt, 0, sizeof(packet));
  pkt->source_port = src_port;
  pkt->dest_port = dst_port;
  pkt->data_offset = 5;
  pkt->window_size = 1024;
  pkt->urgent_pointer = 0;
  pkt->payload[0] = '\0';
}

// Handle connection request (SYN packet)
void handle_connect(int socket_fd, int server_port, packet *received_packet,
                    struct sockaddr_in *client_address, socklen_t len)
{
  client_info *client = find_client(client_address);

  if (client == NULL)
  {
    client = add_client(client_address);
    if (client != NULL)
    {
      client->current_seq_num = received_packet->seq_num + 1;
      client->last_heartbeat = time(NULL);
      printf("New client connected from port %d.\n", ntohs(received_packet->source_port));
    }
  }

  packet syn_ack_packet;
  init_packet(&syn_ack_packet, server_port, received_packet->source_port);
  syn_ack_packet.seq_num = rand();
  syn_ack_packet.ack_num = received_packet->seq_num + 1;
  syn_ack_packet.flags = SYN | ACK;
  syn_ack_packet.checksum = calculate_checksum(&syn_ack_packet);

  sendto(socket_fd, &syn_ack_packet, sizeof(syn_ack_packet), 0,
         (const struct sockaddr *)client_address, len);
}

// Handle termination request (FIN packet)
void handle_terminate(int socket_fd, int server_port, packet *received_packet,
                      struct sockaddr_in *client_address, socklen_t len)
{
  client_info *client = find_client(client_address);

  if (client != NULL)
  {
    client->last_heartbeat = time(NULL);
    remove_client(client_address);
  }

  packet fin_ack_packet;
  init_packet(&fin_ack_packet, server_port, received_packet->source_port);
  fin_ack_packet.seq_num = rand();
  fin_ack_packet.ack_num = received_packet->seq_num + 1;
  fin_ack_packet.flags = FIN | ACK;
  fin_ack_packet.checksum = calculate_checksum(&fin_ack_packet);

  sendto(socket_fd, &fin_ack_packet, sizeof(fin_ack_packet), 0,
         (const struct sockaddr *)client_address, len);
}

// Handle acknowledgement (ACK packet)
void handle_acknowledge(packet *received_packet, struct sockaddr_in *client_address)
{
  client_info *client = find_client(client_address);

  if (client != NULL)
  {
    client->last_heartbeat = time(NULL);
    printf("Received final ACK for handshake. Connection fully established.\n");
  }
}

// Handle data exchange
void handle_data_exchange(int socket_fd, int server_port, packet *received_packet,
                          struct sockaddr_in *client_address, socklen_t len)
{
  client_info *client = find_client(client_address);

  if (client != NULL)
  {
    client->last_heartbeat = time(NULL);
    printf("Received message: %s\n", received_packet->payload);

    // Send ACK for data packet
    packet ack_packet;
    init_packet(&ack_packet, server_port, received_packet->source_port);
    ack_packet.seq_num = client->current_seq_num;
    ack_packet.ack_num = received_packet->seq_num + strlen(received_packet->payload);
    ack_packet.flags = ACK;
    ack_packet.payload[0] = '\0'; // Empty payload for pure ACK
    ack_packet.checksum = calculate_checksum(&ack_packet);

    sendto(socket_fd, &ack_packet, sizeof(ack_packet), 0,
           (const struct sockaddr *)client_address, len);
  }
}

// Handle data exchange with flow control
void handle_data_with_flow_control(int socket_fd, int server_port, packet *received_packet,
                                   struct sockaddr_in *client_address, socklen_t len)
{
  client_info *client = find_client(client_address);

  if (client == NULL)
  {
    printf("Received data from unknown client. Ignoring.\n");
    return;
  }

  // Update client's last activity time
  client->last_heartbeat = time(NULL);

  // Check if this client already has a flow control state
  if (client->fc_state == NULL)
  {
    // First data packet, initialize flow control
    client->fc_state = malloc(sizeof(flow_control_state));
    if (client->fc_state == NULL)
    {
      perror("malloc failed for flow control state");
      return;
    }

    init_flow_control(client->fc_state, socket_fd, client_address,
                      server_port, received_packet->source_port);
  }

  // Process the received data using flow control
  printf("Received data packet: %u bytes\n", (unsigned int)strlen(received_packet->payload));
}

// Main server loop
void server_loop(int server_socket, int server_port)
{
  struct sockaddr_in client_address;
  packet received_packet;
  fd_set read_fds;
  struct timeval tv;
  socklen_t len;

  while (1)
  {
    FD_ZERO(&read_fds);
    FD_SET(server_socket, &read_fds);

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int ready = select(server_socket + 1, &read_fds, NULL, NULL, &tv);

    if (ready < 0)
    {
      perror("select(2)");
      continue;
    }

    if (ready == 0)
    {
      time_t current_time = time(NULL);
      check_client_timeouts(current_time);
      continue;
    }

    if (FD_ISSET(server_socket, &read_fds))
    {
      len = sizeof(client_address);
      int bytes_received = recvfrom(server_socket, &received_packet, sizeof(received_packet), 0,
                                    (struct sockaddr *)&client_address, &len);
      if (bytes_received < 0)
      {
        perror("recvfrom failed");
        continue;
      }

      // Verify checksum
      uint16_t received_checksum = received_packet.checksum;
      received_packet.checksum = 0;
      if (calculate_checksum(&received_packet) != received_checksum)
      {
        printf("Checksum verification failed. Packet might be corrupted.\n");
        continue;
      }

      // Route packet to appropriate handler based on flags
      if (received_packet.flags & SYN)
      {
        handle_connect(server_socket, server_port, &received_packet, &client_address, len);
      }
      else if (received_packet.flags & FIN)
      {
        handle_terminate(server_socket, server_port, &received_packet, &client_address, len);
      }
      else if (received_packet.flags & ACK && !(received_packet.flags & PSH))
      {
        handle_acknowledge(&received_packet, &client_address);
      }
      else if (received_packet.flags & PSH)
      {
        // Data packet with PSH flag - handle with flow control
        handle_data_with_flow_control(server_socket, server_port, &received_packet,
                                      &client_address, len);
      }
      else
      {
        handle_data_exchange(server_socket, server_port, &received_packet, &client_address, len);
      }
    }
  }
}

int main(int argc, char *argv[])
{
  srand(time(NULL));
  int server_socket;
  struct sockaddr_in server_address;

  int server_port;
  if (argc == 2)
  {
    server_port = atoi(argv[1]);
    printf("Using command-line argument: %d\n", server_port);
  }
  else
  {
    server_port = PORT;
    printf("No arguments provided. Using default values: %d\n", server_port);
  }

  if (!init(server_port, &server_socket, &server_address))
  {
    return 1;
  }

  init_client_table();

  server_loop(server_socket, server_port);

  close(server_socket);
  return 0;
}
