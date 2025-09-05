#include "packet.h"
#include <string.h>

uint16_t calculate_checksum(packet *pkt){
  uint16_t sum = 0;
  uint16_t *ptr = (uint16_t *)pkt;
  int i, len = sizeof(packet) / 2;

  // Save the current checksum value and set it to 0 for calculation
  uint16_t orig_checksum = pkt->checksum;
  pkt->checksum = 0;

  // Sum all 16-bit words
  for (i = 0; i < len; i++){
    sum += *ptr++;
  }

  // Add any odd byte
  if (sizeof(packet) % 2){
    sum += *((uint8_t *)pkt + sizeof(packet) - 1);
  }

  // Add carry
  while (sum >> 16){
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  // Take one's complement
  sum = ~sum;

  // Restore the original checksum
  pkt->checksum = orig_checksum;

  return sum;
}
