#ifndef FLOW_CONTROL_H
#define FLOW_CONTROL_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "packet.h"

// Flow control constants
#define INITIAL_WINDOW_SIZE 1024
#define MIN_WINDOW_SIZE 128
#define MAX_WINDOW_SIZE 65535
#define MAX_RETRANSMISSIONS 5
#define FLOW_CONTROL_TIMEOUT_SEC 2
#define FLOW_CONTROL_TIMEOUT_USEC 0

// Flow control state structure
typedef struct {
  uint32_t base_seq_num;        // Base sequence number for this connection
  uint32_t next_seq_num;        // Next sequence number to be sent
  uint32_t last_ack_received;   // Last ACK number received
  uint16_t current_window;      // Current window size
  uint16_t receiver_window;     // Last advertised window from receiver
  int socket_fd;                // Socket file descriptor
  struct sockaddr_in peer_addr; // Peer address information
  socklen_t addr_len;           // Length of address structure
  uint16_t local_port;          // Local port number
  uint16_t remote_port;         // Remote port number
} flow_control_state;

// Initialize flow control state
void init_flow_control(flow_control_state *state, int socket_fd,
                       struct sockaddr_in *peer_addr,
                       uint16_t local_port, uint16_t remote_port);

// Send data with flow control
int send_data_with_flow_control(flow_control_state *state,
                                const char *data, size_t data_len);

// Receive data with flow control
int receive_data_with_flow_control(flow_control_state *state,
                                   char *buffer, size_t buffer_size,
                                   size_t *bytes_received);

// Update flow control state based on received ACK
void update_flow_control(flow_control_state *state, packet *ack_packet);

// Calculate available window size
uint16_t get_available_window(flow_control_state *state);

// Adjust window size based on network conditions
void adjust_window_size(flow_control_state *state, int ack_received);

#endif