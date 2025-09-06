#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <netinet/in.h>
#include <time.h>
#include "flow_control.h"

typedef struct
{
  struct sockaddr_in address;
  time_t last_heartbeat;
  uint32_t current_seq_num;
  flow_control_state *fc_state; // Flow control state for this client
} client_info;

// Initialize client table
void init_client_table();

// Find a client by address
client_info *find_client(struct sockaddr_in *address);

// Add a client to the table
client_info *add_client(struct sockaddr_in *address);

// Remove a client from the table
void remove_client(struct sockaddr_in *address);

// Check for client timeouts
void check_client_timeouts(time_t current_time);

#endif