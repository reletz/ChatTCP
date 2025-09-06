#include "test_utils.h"
#include "packet.h"

// Test three-way handshake for connection establishment
int test_three_way_handshake()
{
  int client_sock, server_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  packet pkt;
  int port = TEST_PORT_BASE;

  // Create sockets
  client_sock = create_test_socket(port);
  server_sock = create_test_socket(port + 1);

  ASSERT_TRUE(client_sock >= 0);
  ASSERT_TRUE(server_sock >= 0);

  // Prepare server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port + 1);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Step 1: Client sends SYN
  memset(&pkt, 0, sizeof(pkt));
  pkt.source_port = port;
  pkt.dest_port = port + 1;
  pkt.seq_num = 1000;
  pkt.flags = SYN;
  pkt.checksum = calculate_checksum(&pkt);

  ASSERT_TRUE(sendto(client_sock, &pkt, sizeof(pkt), 0,
                     (struct sockaddr *)&server_addr, addr_len) > 0);

  // Step 2: Server receives SYN and sends SYN-ACK
  memset(&pkt, 0, sizeof(pkt));
  ASSERT_TRUE(recvfrom(server_sock, &pkt, sizeof(pkt), 0,
                       (struct sockaddr *)&client_addr, &addr_len) > 0);

  ASSERT_TRUE(pkt.flags & SYN);
  ASSERT_EQUAL(1000, pkt.seq_num);

  memset(&pkt, 0, sizeof(pkt));
  pkt.source_port = port + 1;
  pkt.dest_port = port;
  pkt.seq_num = 2000;
  pkt.ack_num = 1001;
  pkt.flags = SYN | ACK;
  pkt.checksum = calculate_checksum(&pkt);

  ASSERT_TRUE(sendto(server_sock, &pkt, sizeof(pkt), 0,
                     (struct sockaddr *)&client_addr, addr_len) > 0);

  // Step 3: Client receives SYN-ACK and sends ACK
  memset(&pkt, 0, sizeof(pkt));
  ASSERT_TRUE(recvfrom(client_sock, &pkt, sizeof(pkt), 0,
                       (struct sockaddr *)&server_addr, &addr_len) > 0);

  ASSERT_TRUE(pkt.flags & SYN);
  ASSERT_TRUE(pkt.flags & ACK);
  ASSERT_EQUAL(2000, pkt.seq_num);
  ASSERT_EQUAL(1001, pkt.ack_num);

  memset(&pkt, 0, sizeof(pkt));
  pkt.source_port = port;
  pkt.dest_port = port + 1;
  pkt.seq_num = 1001;
  pkt.ack_num = 2001;
  pkt.flags = ACK;
  pkt.checksum = calculate_checksum(&pkt);

  ASSERT_TRUE(sendto(client_sock, &pkt, sizeof(pkt), 0,
                     (struct sockaddr *)&server_addr, addr_len) > 0);

  // Step 4: Server receives final ACK
  memset(&pkt, 0, sizeof(pkt));
  ASSERT_TRUE(recvfrom(server_sock, &pkt, sizeof(pkt), 0,
                       (struct sockaddr *)&client_addr, &addr_len) > 0);

  ASSERT_TRUE(pkt.flags & ACK);
  ASSERT_EQUAL(1001, pkt.seq_num);
  ASSERT_EQUAL(2001, pkt.ack_num);

  // Clean up
  close(client_sock);
  close(server_sock);

  return TEST_PASS;
}

// Test four-way handshake for connection termination
int test_four_way_handshake()
{
  int client_sock, server_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  packet pkt;
  int port = TEST_PORT_BASE + 2;

  // Create sockets
  client_sock = create_test_socket(port);
  server_sock = create_test_socket(port + 1);

  ASSERT_TRUE(client_sock >= 0);
  ASSERT_TRUE(server_sock >= 0);

  // Prepare server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port + 1);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Step 1: Client sends FIN
  memset(&pkt, 0, sizeof(pkt));
  pkt.source_port = port;
  pkt.dest_port = port + 1;
  pkt.seq_num = 1000;
  pkt.flags = FIN;
  pkt.checksum = calculate_checksum(&pkt);

  ASSERT_TRUE(sendto(client_sock, &pkt, sizeof(pkt), 0,
                     (struct sockaddr *)&server_addr, addr_len) > 0);

  // Step 2: Server receives FIN and sends FIN-ACK
  memset(&pkt, 0, sizeof(pkt));
  ASSERT_TRUE(recvfrom(server_sock, &pkt, sizeof(pkt), 0,
                       (struct sockaddr *)&client_addr, &addr_len) > 0);

  ASSERT_TRUE(pkt.flags & FIN);
  ASSERT_EQUAL(1000, pkt.seq_num);

  memset(&pkt, 0, sizeof(pkt));
  pkt.source_port = port + 1;
  pkt.dest_port = port;
  pkt.seq_num = 2000;
  pkt.ack_num = 1001;
  pkt.flags = FIN | ACK;
  pkt.checksum = calculate_checksum(&pkt);

  ASSERT_TRUE(sendto(server_sock, &pkt, sizeof(pkt), 0,
                     (struct sockaddr *)&client_addr, addr_len) > 0);

  // Step 3: Client receives FIN-ACK and sends ACK
  memset(&pkt, 0, sizeof(pkt));
  ASSERT_TRUE(recvfrom(client_sock, &pkt, sizeof(pkt), 0,
                       (struct sockaddr *)&server_addr, &addr_len) > 0);

  ASSERT_TRUE(pkt.flags & FIN);
  ASSERT_TRUE(pkt.flags & ACK);
  ASSERT_EQUAL(2000, pkt.seq_num);
  ASSERT_EQUAL(1001, pkt.ack_num);

  memset(&pkt, 0, sizeof(pkt));
  pkt.source_port = port;
  pkt.dest_port = port + 1;
  pkt.seq_num = 1001;
  pkt.ack_num = 2001;
  pkt.flags = ACK;
  pkt.checksum = calculate_checksum(&pkt);

  ASSERT_TRUE(sendto(client_sock, &pkt, sizeof(pkt), 0,
                     (struct sockaddr *)&server_addr, addr_len) > 0);

  // Step 4: Server receives final ACK
  memset(&pkt, 0, sizeof(pkt));
  ASSERT_TRUE(recvfrom(server_sock, &pkt, sizeof(pkt), 0,
                       (struct sockaddr *)&client_addr, &addr_len) > 0);

  ASSERT_TRUE(pkt.flags & ACK);
  ASSERT_EQUAL(1001, pkt.seq_num);
  ASSERT_EQUAL(2001, pkt.ack_num);

  // Clean up
  close(client_sock);
  close(server_sock);

  return TEST_PASS;
}

int main()
{
  // Seed random number generator
  srand(time(NULL));

  // Run tests
  RUN_TEST(test_three_way_handshake);
  RUN_TEST(test_four_way_handshake);

  printf("All connection tests passed!\n");
  return TEST_PASS;
}
