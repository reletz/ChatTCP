#include "test_utils.h"
#include "congestion_control.h"

// Test initialization of congestion control state
int test_congestion_control_init()
{
  congestion_control_state cc_state;
  uint16_t mss = 536; // Standard MSS value

  init_congestion_control(&cc_state, mss);

  // Verify initial state
  ASSERT_EQUAL(INITIAL_CWND_MSS * mss, cc_state.cwnd);
  ASSERT_EQUAL(SSTHRESH_INITIAL, cc_state.ssthresh);
  ASSERT_EQUAL(SLOW_START, cc_state.state);
  ASSERT_EQUAL(0, cc_state.last_ack);
  ASSERT_EQUAL(0, cc_state.duplicate_acks);
  ASSERT_EQUAL(mss, cc_state.mss);

  return TEST_PASS;
}

// Test slow start phase of congestion control
int test_slow_start()
{
  congestion_control_state cc_state;
  flow_control_state fc_state;
  uint16_t mss = 536;
  uint16_t initial_cwnd;

  // Initialize states
  init_congestion_control(&cc_state, mss);
  fc_state.current_window = 10000;
  fc_state.receiver_window = 10000;

  initial_cwnd = cc_state.cwnd;

  // Simulate receiving ACKs during slow start
  update_congestion_window(&cc_state, &fc_state, 1000, 0);

  // Verify cwnd increases by MSS for each ACK during slow start
  ASSERT_EQUAL(initial_cwnd + mss, cc_state.cwnd);
  ASSERT_EQUAL(SLOW_START, cc_state.state);

  return TEST_PASS;
}

// Test transition to congestion avoidance
int test_congestion_avoidance_transition()
{
  congestion_control_state cc_state;
  flow_control_state fc_state;
  uint16_t mss = 536;

  // Initialize states
  init_congestion_control(&cc_state, mss);

  // Properly initialize flow control state to avoid unpredictable behavior
  memset(&fc_state, 0, sizeof(flow_control_state));
  fc_state.current_window = 10000;
  fc_state.receiver_window = 10000;

  // Set cwnd to a value that will definitely remain in slow start after one update
  cc_state.cwnd = cc_state.ssthresh - (3 * mss);

  // This should keep us in slow start
  update_congestion_window(&cc_state, &fc_state, 1000, 0);

  // Debug output to help diagnose the issue
  printf("After first update: cwnd=%u, ssthresh=%u, state=%d\n",
         cc_state.cwnd, cc_state.ssthresh, cc_state.state);

  ASSERT_EQUAL(SLOW_START, cc_state.state);

  // Now set cwnd to exactly ssthresh - mss
  cc_state.cwnd = cc_state.ssthresh - mss;

  // This should increase cwnd by mss, making it equal to ssthresh
  // Which should trigger transition to congestion avoidance since condition is >=
  update_congestion_window(&cc_state, &fc_state, 2000, 0);

  // Debug output
  printf("After second update: cwnd=%u, ssthresh=%u, state=%d\n",
         cc_state.cwnd, cc_state.ssthresh, cc_state.state);

  ASSERT_EQUAL(CONGESTION_AVOIDANCE, cc_state.state);

  return TEST_PASS;
}

// Test fast retransmit and recovery
int test_fast_retransmit()
{
  congestion_control_state cc_state;
  flow_control_state fc_state;
  uint16_t mss = 536;
  uint16_t original_cwnd;

  // Initialize states
  init_congestion_control(&cc_state, mss);
  fc_state.current_window = 10000;
  fc_state.receiver_window = 10000;

  // Put us in congestion avoidance with a larger window
  cc_state.state = CONGESTION_AVOIDANCE;
  cc_state.cwnd = 10 * mss;
  cc_state.last_ack = 1000;
  original_cwnd = cc_state.cwnd;

  // First duplicate ACK
  update_congestion_window(&cc_state, &fc_state, 1000, 1);
  ASSERT_EQUAL(1, cc_state.duplicate_acks);
  ASSERT_EQUAL(CONGESTION_AVOIDANCE, cc_state.state);

  // Second duplicate ACK
  update_congestion_window(&cc_state, &fc_state, 1000, 1);
  ASSERT_EQUAL(2, cc_state.duplicate_acks);
  ASSERT_EQUAL(CONGESTION_AVOIDANCE, cc_state.state);

  // Third duplicate ACK - should trigger fast retransmit
  update_congestion_window(&cc_state, &fc_state, 1000, 1);
  ASSERT_EQUAL(3, cc_state.duplicate_acks);
  ASSERT_EQUAL(FAST_RECOVERY, cc_state.state);
  ASSERT_EQUAL(original_cwnd / 2, cc_state.ssthresh);

  return TEST_PASS;
}

// Test timeout handling
int test_timeout_handling()
{
  congestion_control_state cc_state;
  uint16_t mss = 536;
  uint16_t original_cwnd, original_ssthresh;

  // Initialize state
  init_congestion_control(&cc_state, mss);

  // Put us in congestion avoidance with a larger window
  cc_state.state = CONGESTION_AVOIDANCE;
  cc_state.cwnd = 10 * mss;
  cc_state.ssthresh = 20 * mss;
  original_cwnd = cc_state.cwnd;
  original_ssthresh = cc_state.ssthresh;

  // Handle timeout
  handle_timeout(&cc_state);

  // Verify state after timeout
  ASSERT_EQUAL(SLOW_START, cc_state.state);
  ASSERT_EQUAL(mss, cc_state.cwnd);                   // Reset to 1 MSS
  ASSERT_EQUAL(original_cwnd / 2, cc_state.ssthresh); // Half of previous cwnd

  return TEST_PASS;
}

int main()
{
  // Seed random number generator
  srand(time(NULL));

  // Run tests
  RUN_TEST(test_congestion_control_init);
  RUN_TEST(test_slow_start);
  RUN_TEST(test_congestion_avoidance_transition);
  RUN_TEST(test_fast_retransmit);
  RUN_TEST(test_timeout_handling);

  printf("All congestion control tests passed!\n");
  return TEST_PASS;
}
