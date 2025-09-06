#include "test_utils.h"
#include "packet.h"

// Test packet structure initialization
int test_packet_init()
{
  packet pkt;

  // Initialize packet fields
  memset(&pkt, 0, sizeof(pkt));
  pkt.source_port = 12345;
  pkt.dest_port = 54321;
  pkt.seq_num = 1000;
  pkt.ack_num = 2000;
  pkt.data_offset = 5;
  pkt.flags = SYN | ACK;
  pkt.window_size = 4096;
  pkt.urgent_pointer = 0;
  strcpy(pkt.payload, "Test message");

  // Verify fields
  ASSERT_EQUAL(12345, pkt.source_port);
  ASSERT_EQUAL(54321, pkt.dest_port);
  ASSERT_EQUAL(1000, pkt.seq_num);
  ASSERT_EQUAL(2000, pkt.ack_num);
  ASSERT_EQUAL(5, pkt.data_offset);
  ASSERT_EQUAL(SYN | ACK, pkt.flags);
  ASSERT_EQUAL(4096, pkt.window_size);
  ASSERT_EQUAL(0, pkt.urgent_pointer);
  ASSERT_STRING_EQUAL("Test message", pkt.payload);

  return TEST_PASS;
}

// Test checksum calculation
int test_checksum()
{
  packet pkt1, pkt2;
  uint16_t checksum1, checksum2;

  // Initialize first packet
  memset(&pkt1, 0, sizeof(pkt1));
  pkt1.source_port = 12345;
  pkt1.dest_port = 54321;
  pkt1.seq_num = 1000;
  pkt1.ack_num = 2000;
  pkt1.data_offset = 5;
  pkt1.flags = SYN | ACK;
  pkt1.window_size = 4096;
  pkt1.urgent_pointer = 0;
  strcpy(pkt1.payload, "Test message");

  // Calculate checksum
  checksum1 = calculate_checksum(&pkt1);
  pkt1.checksum = checksum1;

  // Verify checksum is non-zero
  ASSERT_TRUE(checksum1 != 0);

  // Create identical packet
  memcpy(&pkt2, &pkt1, sizeof(packet));

  // Verify checksums match
  checksum2 = calculate_checksum(&pkt2);
  ASSERT_EQUAL(checksum1, checksum2);

  // Modify packet and verify checksum changes
  pkt2.seq_num = 1001;
  checksum2 = calculate_checksum(&pkt2);
  ASSERT_TRUE(checksum1 != checksum2);

  return TEST_PASS;
}

// Test packet flags
int test_packet_flags()
{
  packet pkt;

  // Test SYN flag
  memset(&pkt, 0, sizeof(pkt));
  pkt.flags = SYN;
  ASSERT_TRUE(pkt.flags & SYN);
  ASSERT_FALSE(pkt.flags & ACK);
  ASSERT_FALSE(pkt.flags & FIN);

  // Test ACK flag
  memset(&pkt, 0, sizeof(pkt));
  pkt.flags = ACK;
  ASSERT_FALSE(pkt.flags & SYN);
  ASSERT_TRUE(pkt.flags & ACK);
  ASSERT_FALSE(pkt.flags & FIN);

  // Test FIN flag
  memset(&pkt, 0, sizeof(pkt));
  pkt.flags = FIN;
  ASSERT_FALSE(pkt.flags & SYN);
  ASSERT_FALSE(pkt.flags & ACK);
  ASSERT_TRUE(pkt.flags & FIN);

  // Test multiple flags
  memset(&pkt, 0, sizeof(pkt));
  pkt.flags = SYN | ACK;
  ASSERT_TRUE(pkt.flags & SYN);
  ASSERT_TRUE(pkt.flags & ACK);
  ASSERT_FALSE(pkt.flags & FIN);

  // Test flag clearing
  pkt.flags &= ~SYN;
  ASSERT_FALSE(pkt.flags & SYN);
  ASSERT_TRUE(pkt.flags & ACK);

  return TEST_PASS;
}

int main()
{
  // Run tests
  RUN_TEST(test_packet_init);
  RUN_TEST(test_checksum);
  RUN_TEST(test_packet_flags);

  printf("All packet tests passed!\n");
  return TEST_PASS;
}
