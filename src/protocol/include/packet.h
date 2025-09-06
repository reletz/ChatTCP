#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

// TCP Control Flags
#define URG 0x20
#define ACK 0x10
#define PSH 0x08
#define RST 0x04
#define SYN 0x02
#define FIN 0x01

#define MAX_PAYLOAD_SIZE 44

typedef struct {
  // Standard TCP Header (20 bytes)
  uint16_t source_port;    // 2 bytes
  uint16_t dest_port;      // 2 bytes
  uint32_t seq_num;        // 4 bytes
  uint32_t ack_num;        // 4 bytes
  uint8_t data_offset : 4; // Header length in 32-bit words
  uint8_t reserved : 4;    // Reserved for future use
  uint8_t flags;           // Control flags
  uint16_t window_size;    // 2 bytes
  uint16_t checksum;       // 2 bytes
  uint16_t urgent_pointer; // 2 bytes

  // Payload (44 bytes)
  char payload[MAX_PAYLOAD_SIZE];
} packet;

// Calculate TCP checksum
uint16_t calculate_checksum(packet *pkt);

#endif