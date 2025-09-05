#include "client_manager.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

client_info *client_table[MAX_CLIENTS];

unsigned int hash_address(struct sockaddr_in *address) {
  unsigned int hash = 0;
  hash += address->sin_port;
  hash += address->sin_addr.s_addr;
  return hash % MAX_CLIENTS;
}

void init_client_table() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    client_table[i] = NULL;
  }
}

client_info* find_client(struct sockaddr_in *address) {
  unsigned int index = hash_address(address);
  client_info *current = client_table[index];

  while (current != NULL) {
    if (current->address.sin_port == address->sin_port &&
      current->address.sin_addr.s_addr == address->sin_addr.s_addr) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

client_info* add_client(struct sockaddr_in *address) {
  if (find_client(address) != NULL) {
    return find_client(address);
  }

  unsigned int index = hash_address(address);
  client_info *new_client = (client_info*)malloc(sizeof(client_info));
  if (new_client == NULL) {
    return NULL;
  }

  memset(new_client, 0, sizeof(client_info));
  memcpy(&new_client->address, address, sizeof(struct sockaddr_in));
  new_client->last_heartbeat = time(NULL);
  new_client->next = client_table[index];
  client_table[index] = new_client;
  
  return new_client;
}

void remove_client(struct sockaddr_in *address) {
  unsigned int index = hash_address(address);
  client_info *current = client_table[index];
  client_info *prev = NULL;

  while (current != NULL) {
    if (current->address.sin_port == address->sin_port &&
      current->address.sin_addr.s_addr == address->sin_addr.s_addr) {
      if (prev == NULL) {
        client_table[index] = current->next;
      } else {
        prev->next = current->next;
      }
      free(current);
      printf("Client removed from map.\n");
      return;
    }
    prev = current;
    current = current->next;
  }
}

void check_client_timeouts(time_t current_time) {
  int i;
  for (i = 0; i < MAX_CLIENTS; i++) {
    client_info *current = client_table[i];
    client_info *prev = NULL;
    
    // Iterate through the linked list at this index
    while (current != NULL) {
      // If client hasn't been active for TIMEOUT_HEARTBEAT seconds
      if (current_time - current->last_heartbeat > TIMEOUT_HEARTBEAT) {
        printf("Client %s:%d timed out\n", 
            inet_ntoa(current->address.sin_addr),
            ntohs(current->address.sin_port));
        
        // Remove the client from the linked list
        client_info *to_remove = current;
        
        if (prev == NULL) {
          // First node in the list
          client_table[i] = current->next;
        } else {
          // Middle or end of list
          prev->next = current->next;
        }
        
        current = current->next;
        free(to_remove);
      } else {
        // Move to next node
        prev = current;
        current = current->next;
      }
    }
  }
}