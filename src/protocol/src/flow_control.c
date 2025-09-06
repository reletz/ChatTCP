#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#include "flow_control.h"
#include "packet.h"
#include "congestion_control.h"

// Initialize flow control state
void init_flow_control(flow_control_state *state, int socket_fd,
                       struct sockaddr_in *peer_addr,
                       uint16_t local_port, uint16_t remote_port)
{
  state->base_seq_num = rand();
  state->next_seq_num = state->base_seq_num;
  state->last_ack_received = 0;
  state->current_window = INITIAL_WINDOW_SIZE;
  state->receiver_window = INITIAL_WINDOW_SIZE;
  state->socket_fd = socket_fd;
  memcpy(&state->peer_addr, peer_addr, sizeof(struct sockaddr_in));
  state->addr_len = sizeof(struct sockaddr_in);
  state->local_port = local_port;
  state->remote_port = remote_port;

  printf("Flow control initialized. Base sequence number: %u\n", state->base_seq_num);

  // Initialize congestion control
  congestion_control_state cc_state;
  init_congestion_control(&cc_state, MAX_PAYLOAD_SIZE);

  // Apply initial congestion window
  apply_congestion_window(&cc_state, state);
}

// Helper function to initialize a packet for data transfer
static void prepare_data_packet(packet *pkt, flow_control_state *state,
                                const char *data, size_t len)
{
  memset(pkt, 0, sizeof(packet));
  pkt->source_port = state->local_port;
  pkt->dest_port = state->remote_port;
  pkt->seq_num = state->next_seq_num;
  pkt->ack_num = state->last_ack_received;
  pkt->data_offset = 5;
  pkt->flags = PSH;
  pkt->window_size = state->current_window;
  pkt->urgent_pointer = 0;

  size_t copy_len = (len > MAX_PAYLOAD_SIZE) ? MAX_PAYLOAD_SIZE : len;
  memcpy(pkt->payload, data, copy_len);
  pkt->payload[copy_len] = '\0';

  // Calculate checksum
  pkt->checksum = calculate_checksum(pkt);
}

// Send data with flow control
int send_data_with_flow_control(flow_control_state *state,
                                const char *data, size_t data_len)
{
  size_t bytes_sent = 0;
  int retransmissions = 0;
  congestion_control_state cc_state;

  // Initialize congestion control
  init_congestion_control(&cc_state, MAX_PAYLOAD_SIZE);

  while (bytes_sent < data_len)
  {
    // Calculate available window using congestion control
    uint16_t window = get_available_window(state);
    size_t remaining = data_len - bytes_sent;
    size_t chunk_size = (remaining < window) ? remaining : window;

    // Limit chunk size to maximum payload size
    if (chunk_size > MAX_PAYLOAD_SIZE)
    {
      chunk_size = MAX_PAYLOAD_SIZE;
    }

    // Check if we can send data based on congestion window
    if (!can_send_data(&cc_state, state, chunk_size))
    {
      // Wait for ACKs to open the congestion window
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(state->socket_fd, &read_fds);

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000; // 100ms

      if (select(state->socket_fd + 1, &read_fds, NULL, NULL, &timeout) > 0)
      {
        // Process ACKs to update congestion window
        packet ack_packet;
        socklen_t addr_len = state->addr_len;

        if (recvfrom(state->socket_fd, &ack_packet, sizeof(ack_packet), 0,
                     (struct sockaddr *)&state->peer_addr, &addr_len) > 0)
        {

          if (ack_packet.flags & ACK)
          {
            int is_duplicate = (ack_packet.ack_num == cc_state.last_ack);
            update_congestion_window(&cc_state, state, ack_packet.ack_num, is_duplicate);
          }
        }
      }
      continue;
    }

    // Prepare and send packet
    packet data_packet;
    prepare_data_packet(&data_packet, state, data + bytes_sent, chunk_size);

    printf("Sending %zu bytes, seq=%u, window=%u, cwnd=%u\n",
           chunk_size, state->next_seq_num, state->current_window, cc_state.cwnd);

    if (sendto(state->socket_fd, &data_packet, sizeof(data_packet), 0,
               (struct sockaddr *)&state->peer_addr, state->addr_len) < 0)
    {
      perror("sendto(2) failed in flow control");
      return -1;
    }

    // Wait for ACK with timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(state->socket_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = FLOW_CONTROL_TIMEOUT_SEC;
    timeout.tv_usec = FLOW_CONTROL_TIMEOUT_USEC;

    int select_result = select(state->socket_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (select_result < 0)
    {
      if (errno == EINTR)
        continue;
      perror("select(2) failed in flow control");
      return -1;
    }

    if (select_result == 0)
    {
      // Timeout occurred, retransmit
      printf("Timeout waiting for ACK. Retransmitting... (%d/%d)\n",
             retransmissions + 1, MAX_RETRANSMISSIONS);

      retransmissions++;
      if (retransmissions >= MAX_RETRANSMISSIONS)
      {
        printf("Maximum retransmissions reached. Giving up.\n");
        return -1;
      }

      // Handle timeout in congestion control
      handle_timeout(&cc_state);
      continue;
    }

    // Reset retransmission counter
    retransmissions = 0;

    // Receive ACK
    packet ack_packet;
    socklen_t addr_len = state->addr_len;

    if (recvfrom(state->socket_fd, &ack_packet, sizeof(ack_packet), 0,
                 (struct sockaddr *)&state->peer_addr, &addr_len) < 0)
    {
      perror("recvfrom(2) failed in flow control");
      continue;
    }

    // Verify checksum
    uint16_t received_checksum = ack_packet.checksum;
    ack_packet.checksum = 0;
    if (calculate_checksum(&ack_packet) != received_checksum)
    {
      printf("Checksum verification failed. Packet might be corrupted.\n");
      continue;
    }

    // Process ACK
    if (ack_packet.flags & ACK)
    {
      printf("Received ACK: %u, window: %u\n",
             ack_packet.ack_num, ack_packet.window_size);

      // Update flow control state
      update_flow_control(state, &ack_packet);

      // Update congestion control state
      int is_duplicate = (ack_packet.ack_num == cc_state.last_ack);
      update_congestion_window(&cc_state, state, ack_packet.ack_num, is_duplicate);

      // Check if this ACK acknowledges our data
      if (ack_packet.ack_num > state->next_seq_num)
      {
        // Calculate acknowledged bytes
        size_t acked_bytes = ack_packet.ack_num - state->next_seq_num;
        if (acked_bytes > chunk_size)
        {
          acked_bytes = chunk_size;
        }

        // Advance in the data stream
        bytes_sent += acked_bytes;
        state->next_seq_num += acked_bytes;
      }
    }
    else
    {
      printf("Received non-ACK packet. Ignoring.\n");
    }
  }

  printf("All data sent successfully.\n");
  return data_len;
}

// Receive data with flow control
int receive_data_with_flow_control(flow_control_state *state,
                                   char *buffer, size_t buffer_size,
                                   size_t *bytes_received)
{
  packet received_packet;
  socklen_t addr_len = state->addr_len;

  printf("Waiting to receive data with flow control...\n");

  // Use a timeout for the receive operation
  fd_set read_fds;
  struct timeval timeout;
  FD_ZERO(&read_fds);
  FD_SET(state->socket_fd, &read_fds);
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  if (select(state->socket_fd + 1, &read_fds, NULL, NULL, &timeout) <= 0)
  {
    printf("Timeout waiting for data packet\n");
    return -1;
  }

  // Wait for incoming data
  int bytes = recvfrom(state->socket_fd, &received_packet, sizeof(received_packet), 0,
                       (struct sockaddr *)&state->peer_addr, &addr_len);

  if (bytes < 0)
  {
    perror("recvfrom(2) failed in flow control");
    return -1;
  }

  printf("Received packet with flags=0x%x, seq=%u\n",
         received_packet.flags, received_packet.seq_num);

  // Verify checksum
  uint16_t received_checksum = received_packet.checksum;
  received_packet.checksum = 0;
  if (calculate_checksum(&received_packet) != received_checksum)
  {
    printf("Checksum verification failed. Packet might be corrupted.\n");
    return 0;
  }

  // Check if this is a data packet
  if (received_packet.flags & PSH)
  {
    // Copy data to buffer
    size_t payload_len = strlen(received_packet.payload);
    if (payload_len > buffer_size)
    {
      payload_len = buffer_size;
    }

    memcpy(buffer, received_packet.payload, payload_len);
    *bytes_received = payload_len;

    printf("Received data packet with %zu bytes, sending ACK\n", payload_len);

    // Send ACK
    packet ack_packet;
    memset(&ack_packet, 0, sizeof(packet));
    ack_packet.source_port = state->local_port;
    ack_packet.dest_port = state->remote_port;
    ack_packet.seq_num = state->next_seq_num;
    ack_packet.ack_num = received_packet.seq_num + payload_len;
    ack_packet.data_offset = 5;
    ack_packet.flags = ACK;
    ack_packet.window_size = state->current_window;
    ack_packet.checksum = calculate_checksum(&ack_packet);

    if (sendto(state->socket_fd, &ack_packet, sizeof(ack_packet), 0,
               (struct sockaddr *)&state->peer_addr, addr_len) < 0)
    {
      perror("sendto(2) failed in flow control");
      return -1;
    }

    // Update last received sequence number
    state->last_ack_received = received_packet.seq_num + payload_len;

    return payload_len;
  }
  else
  {
    printf("Received non-data packet (flags=0x%x)\n", received_packet.flags);
  }

  // Not a data packet
  return 0;
}

// Update flow control state based on received ACK
void update_flow_control(flow_control_state *state, packet *ack_packet)
{
  // Update last ACK received
  if (ack_packet->ack_num > state->last_ack_received)
  {
    state->last_ack_received = ack_packet->ack_num;
  }

  // Update receiver window
  state->receiver_window = ack_packet->window_size;
}

// Calculate available window size
uint16_t get_available_window(flow_control_state *state)
{
  // Return the minimum of our window and receiver's window
  return (state->current_window < state->receiver_window) ? state->current_window : state->receiver_window;
}

// Adjust window size based on network conditions
void adjust_window_size(flow_control_state *state, int ack_received)
{
  if (ack_received)
  {
    // Successful transmission, increase window (additive increase)
    if (state->current_window < MAX_WINDOW_SIZE)
    {
      state->current_window += MAX_PAYLOAD_SIZE;
      if (state->current_window > MAX_WINDOW_SIZE)
      {
        state->current_window = MAX_WINDOW_SIZE;
      }
    }
  }
  else
  {
    // Timeout or loss, decrease window (multiplicative decrease)
    state->current_window /= 2;
    if (state->current_window < MIN_WINDOW_SIZE)
    {
      state->current_window = MIN_WINDOW_SIZE;
    }
  }
}
