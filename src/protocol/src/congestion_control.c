#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "congestion_control.h"

// Initialize congestion control state
void init_congestion_control(congestion_control_state *cc_state, uint16_t mss)
{
  cc_state->cwnd = INITIAL_CWND_MSS * mss; // Start with 1 MSS
  cc_state->ssthresh = SSTHRESH_INITIAL;   // Initial slow start threshold
  cc_state->state = SLOW_START;            // Start in slow start phase
  cc_state->last_ack = 0;                  // No ACKs received yet
  cc_state->duplicate_acks = 0;            // No duplicate ACKs yet
  cc_state->mss = mss;                     // Store MSS value

  printf("Congestion control initialized: cwnd=%u, ssthresh=%u, state=%s\n",
         cc_state->cwnd, cc_state->ssthresh,
         cc_state->state == SLOW_START ? "SLOW_START" : cc_state->state == CONGESTION_AVOIDANCE ? "CONGESTION_AVOIDANCE"
                                                                                                : "FAST_RECOVERY");
}

// Update congestion window based on received ACK
void update_congestion_window(congestion_control_state *cc_state,
                              flow_control_state *fc_state,
                              uint32_t ack_num,
                              int is_duplicate)
{
  // Check for duplicate ACK
  if (ack_num == cc_state->last_ack)
  {
    if (is_duplicate)
    {
      cc_state->duplicate_acks++;
      printf("Duplicate ACK received (%d/%d)\n",
             cc_state->duplicate_acks, DUPLICATE_ACK_THRESHOLD);

      // Check for fast retransmit threshold
      if (cc_state->duplicate_acks == DUPLICATE_ACK_THRESHOLD)
      {
        // Enter fast recovery
        cc_state->ssthresh = cc_state->cwnd / 2;
        if (cc_state->ssthresh < cc_state->mss)
        {
          cc_state->ssthresh = cc_state->mss;
        }
        cc_state->cwnd = cc_state->ssthresh + 3 * cc_state->mss;
        cc_state->state = FAST_RECOVERY;

        printf("Fast retransmit triggered: cwnd=%u, ssthresh=%u, state=FAST_RECOVERY\n",
               cc_state->cwnd, cc_state->ssthresh);
      }
      else if (cc_state->state == FAST_RECOVERY)
      {
        // Increase cwnd for each duplicate ACK in fast recovery
        cc_state->cwnd += cc_state->mss;
        printf("Fast recovery: increased cwnd to %u\n", cc_state->cwnd);
      }
    }
    return;
  }

  // New ACK received (not a duplicate)
  cc_state->last_ack = ack_num;
  cc_state->duplicate_acks = 0;

  if (cc_state->state == FAST_RECOVERY)
  {
    // Exit fast recovery
    cc_state->cwnd = cc_state->ssthresh;
    cc_state->state = CONGESTION_AVOIDANCE;
    printf("Exiting fast recovery: cwnd=%u, state=CONGESTION_AVOIDANCE\n", cc_state->cwnd);
  }
  else if (cc_state->state == SLOW_START)
  {
    // Exponential growth during slow start
    cc_state->cwnd += cc_state->mss;
    printf("Slow start: increased cwnd to %u\n", cc_state->cwnd);

    // Check if we should transition to congestion avoidance
    if (cc_state->cwnd >= cc_state->ssthresh)
    {
      cc_state->state = CONGESTION_AVOIDANCE;
      printf("Transitioning to congestion avoidance: cwnd=%u, ssthresh=%u\n",
             cc_state->cwnd, cc_state->ssthresh);
    }
  }
  else if (cc_state->state == CONGESTION_AVOIDANCE)
  {
    // Additive increase during congestion avoidance
    // Increase cwnd by MSS * MSS / cwnd bytes
    cc_state->cwnd += (cc_state->mss * cc_state->mss) / cc_state->cwnd;
    printf("Congestion avoidance: increased cwnd to %u\n", cc_state->cwnd);
  }

  // Apply the congestion window to the flow control state
  apply_congestion_window(cc_state, fc_state);
}

// Handle packet loss (timeout)
void handle_timeout(congestion_control_state *cc_state)
{
  // Set ssthresh to half of cwnd (minimum 1 MSS)
  cc_state->ssthresh = cc_state->cwnd / 2;
  if (cc_state->ssthresh < cc_state->mss)
  {
    cc_state->ssthresh = cc_state->mss;
  }

  // Reset cwnd to 1 MSS
  cc_state->cwnd = cc_state->mss;

  // Reset duplicate ACK counter
  cc_state->duplicate_acks = 0;

  // Return to slow start
  cc_state->state = SLOW_START;

  printf("Timeout occurred: cwnd=%u, ssthresh=%u, state=SLOW_START\n",
         cc_state->cwnd, cc_state->ssthresh);
}

// Get current congestion window size
uint16_t get_congestion_window(congestion_control_state *cc_state)
{
  return cc_state->cwnd;
}

// Check if we can send data based on congestion window
int can_send_data(congestion_control_state *cc_state,
                  flow_control_state *fc_state,
                  size_t data_size)
{
  // Calculate in-flight data
  uint32_t in_flight = fc_state->next_seq_num - fc_state->last_ack_received;

  // Check if there's enough space in the congestion window
  if (in_flight + data_size <= cc_state->cwnd)
  {
    return 1; // Can send data
  }

  return 0; // Cannot send data yet
}

// Update flow control state with congestion window
void apply_congestion_window(congestion_control_state *cc_state,
                             flow_control_state *fc_state)
{
  // Set the flow control window to the minimum of:
  // 1. The receiver's advertised window (flow control)
  // 2. The congestion window (congestion control)
  if (cc_state->cwnd < fc_state->receiver_window)
  {
    fc_state->current_window = cc_state->cwnd;
  }
  else
  {
    fc_state->current_window = fc_state->receiver_window;
  }

  printf("Applied congestion window: effective window=%u bytes\n",
         fc_state->current_window);
}
