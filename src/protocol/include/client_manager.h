#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLIENTS 100
#define TIMEOUT_HEARTBEAT 30

typedef struct client_info {
  struct sockaddr_in address;
  uint32_t current_seq_num;
  time_t last_heartbeat;
  struct client_info *next;
} client_info;

void init_client_table();
client_info* find_client(struct sockaddr_in *address);
client_info* add_client(struct sockaddr_in *address);
void remove_client(struct sockaddr_in *address);
void check_client_timeouts(time_t current_time);

#endif