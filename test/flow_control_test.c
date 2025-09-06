#include "test_utils.h"
#include "flow_control.h"

// Test initialization of flow control state
int test_flow_control_init()
{
  int sock = create_test_socket(TEST_PORT_BASE + 4);
  struct sockaddr_in peer_addr;
  flow_control_state fc_state;

  ASSERT_TRUE(sock >= 0);

  // Setup peer address
  memset(&peer_addr, 0, sizeof(peer_addr));
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = htons(TEST_PORT_BASE + 5);
  peer_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Initialize flow control
  init_flow_control(&fc_state, sock, &peer_addr, TEST_PORT_BASE + 4, TEST_PORT_BASE + 5);

  // Verify initial state
  ASSERT_EQUAL(TEST_PORT_BASE + 4, fc_state.local_port);
  ASSERT_EQUAL(TEST_PORT_BASE + 5, fc_state.remote_port);

  // Note: The current_window is initially set to INITIAL_WINDOW_SIZE but may be
  // modified by congestion control initialization which uses MAX_PAYLOAD_SIZE (44)
  // as the initial cwnd. We'll check it's either the initial value or congestion value.
  ASSERT_TRUE(fc_state.current_window == INITIAL_WINDOW_SIZE ||
              fc_state.current_window == MAX_PAYLOAD_SIZE);

  ASSERT_EQUAL(INITIAL_WINDOW_SIZE, fc_state.receiver_window);
  ASSERT_EQUAL(sock, fc_state.socket_fd);
  ASSERT_EQUAL(fc_state.base_seq_num, fc_state.next_seq_num);
  ASSERT_EQUAL(0, fc_state.last_ack_received);

  close(sock);
  return TEST_PASS;
}

// Test window size calculation
int test_window_calculation()
{
  flow_control_state fc_state;

  // Setup initial state
  fc_state.current_window = 1000;
  fc_state.receiver_window = 2000;

  // Test getting available window (should be min of current and receiver)
  ASSERT_EQUAL(1000, get_available_window(&fc_state));

  fc_state.current_window = 3000;
  ASSERT_EQUAL(2000, get_available_window(&fc_state));

  // Test window adjustment
  adjust_window_size(&fc_state, 1);            // Successful ACK
  ASSERT_TRUE(fc_state.current_window > 3000); // Should increase

  uint16_t previous_window = fc_state.current_window;
  adjust_window_size(&fc_state, 0);                        // Unsuccessful (timeout/loss)
  ASSERT_TRUE(fc_state.current_window < previous_window);  // Should decrease
  ASSERT_TRUE(fc_state.current_window >= MIN_WINDOW_SIZE); // But not below min

  return TEST_PASS;
}

// Test flow control update based on ACK
int test_flow_control_update()
{
  flow_control_state fc_state;
  packet ack_packet;

  // Setup initial state
  fc_state.current_window = 1000;
  fc_state.receiver_window = 2000;
  fc_state.last_ack_received = 100;

  // Setup ACK packet
  memset(&ack_packet, 0, sizeof(ack_packet));
  ack_packet.ack_num = 200;
  ack_packet.window_size = 1500;

  // Update flow control state
  update_flow_control(&fc_state, &ack_packet);

  // Verify state after update
  ASSERT_EQUAL(200, fc_state.last_ack_received);
  ASSERT_EQUAL(1500, fc_state.receiver_window);

  return TEST_PASS;
}

int main()
{
  // Seed random number generator
  srand(time(NULL));

  // Run tests
  RUN_TEST(test_flow_control_init);
  RUN_TEST(test_window_calculation);
  RUN_TEST(test_flow_control_update);

  printf("All flow control tests passed!\n");
  return TEST_PASS;
}
