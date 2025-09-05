#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define SYN 0x01
#define ACK 0x02
#define FIN 0x04

#define MAX_PAYLOAD_SIZE 1000

typedef struct {
  uint32_t seq_num;
  uint32_t ack_num;
  uint8_t  flags;
  char     payload[MAX_PAYLOAD_SIZE];
} packet;

#endif