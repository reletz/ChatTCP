#ifndef CONGESTION_CONTROL_H
#define CONGESTION_CONTROL_H

#include <stdint.h>
#include "flow_control.h"

// Congestion control states
#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define FAST_RECOVERY 2

// Congestion control constants
#define INITIAL_CWND_MSS 1
#define SSTHRESH_INITIAL 65535
#define DUPLICATE_ACK_THRESHOLD 3

// Congestion control state structure
typedef struct
{
  uint16_t cwnd;      // Congestion window in bytes
  uint16_t ssthresh;  // Slow start threshold
  int state;          // Current state (SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY)
  uint32_t last_ack;  // Last acknowledged sequence number
  int duplicate_acks; // Count of duplicate ACKs
  uint16_t mss;       // Maximum segment size
} congestion_control_state;

// Initialize congestion control state
void init_congestion_control(congestion_control_state *cc_state, uint16_t mss);

// Update congestion window based on received ACK
void update_congestion_window(congestion_control_state *cc_state,
                              flow_control_state *fc_state,
                              uint32_t ack_num,
                              int is_duplicate);

// Handle packet loss (timeout)
void handle_timeout(congestion_control_state *cc_state);

// Get current congestion window size
uint16_t get_congestion_window(congestion_control_state *cc_state);

// Check if we can send data based on congestion window
int can_send_data(congestion_control_state *cc_state,
                  flow_control_state *fc_state,
                  size_t data_size);

// Update flow control state with congestion window
void apply_congestion_window(congestion_control_state *cc_state,
                             flow_control_state *fc_state);

#endif
