#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "client_manager.h"

#define MAX_CLIENTS 10
#define CLIENT_TIMEOUT 60 // seconds

static client_info clients[MAX_CLIENTS];
static int num_clients = 0;

// Initialize client table
void init_client_table()
{
  num_clients = 0;
}

// Compare two sockaddr_in structures
static int addr_equal(struct sockaddr_in *a, struct sockaddr_in *b)
{
  return (a->sin_addr.s_addr == b->sin_addr.s_addr) &&
         (a->sin_port == b->sin_port);
}

// Find a client by address
client_info *find_client(struct sockaddr_in *address)
{
  for (int i = 0; i < num_clients; i++)
  {
    if (addr_equal(&clients[i].address, address))
    {
      return &clients[i];
    }
  }
  return NULL;
}

// Add a client to the table
client_info *add_client(struct sockaddr_in *address)
{
  if (num_clients >= MAX_CLIENTS)
  {
    printf("Client table full. Cannot add more clients.\n");
    return NULL;
  }

  client_info *client = &clients[num_clients++];
  memcpy(&client->address, address, sizeof(struct sockaddr_in));
  client->last_heartbeat = time(NULL);
  client->current_seq_num = 0;
  client->fc_state = NULL; // Initialize flow control state as NULL

  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(address->sin_addr), ip_str, INET_ADDRSTRLEN);
  printf("Added client %s:%d\n", ip_str, ntohs(address->sin_port));

  return client;
}

// Remove a client from the table
void remove_client(struct sockaddr_in *address)
{
  int i;
  for (i = 0; i < num_clients; i++)
  {
    if (addr_equal(&clients[i].address, address))
    {
      // Free flow control state if allocated
      if (clients[i].fc_state != NULL)
      {
        free(clients[i].fc_state);
        clients[i].fc_state = NULL;
      }

      char ip_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(address->sin_addr), ip_str, INET_ADDRSTRLEN);
      printf("Removed client %s:%d\n", ip_str, ntohs(address->sin_port));

      // Shift remaining clients
      if (i < num_clients - 1)
      {
        memmove(&clients[i], &clients[i + 1],
                (num_clients - i - 1) * sizeof(client_info));
      }
      num_clients--;
      return;
    }
  }
}

// Check for client timeouts
void check_client_timeouts(time_t current_time)
{
  for (int i = 0; i < num_clients; i++)
  {
    if (current_time - clients[i].last_heartbeat > CLIENT_TIMEOUT)
    {
      char ip_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(clients[i].address.sin_addr), ip_str, INET_ADDRSTRLEN);
      printf("Client %s:%d timed out\n", ip_str, ntohs(clients[i].address.sin_port));

      // Free flow control state if allocated
      if (clients[i].fc_state != NULL)
      {
        free(clients[i].fc_state);
        clients[i].fc_state = NULL;
      }

      // Shift remaining clients
      if (i < num_clients - 1)
      {
        memmove(&clients[i], &clients[i + 1],
                (num_clients - i - 1) * sizeof(client_info));
      }
      num_clients--;
      i--; // Adjust index after removing a client
    }
  }
}