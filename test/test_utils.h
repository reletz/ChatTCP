#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#include "packet.h"
#include "flow_control.h"
#include "congestion_control.h"

// Testing constants
#define TEST_PORT_BASE 50000
#define TEST_TIMEOUT_SEC 2
#define TEST_PACKET_LOSS_RATE 10 // percentage (0-100)
#define TEST_PASS 0
#define TEST_FAIL 1

// Colors for test output
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_RESET "\x1b[0m"

// Test assertion macros
#define ASSERT_TRUE(condition)                                           \
  do                                                                     \
  {                                                                      \
    if (!(condition))                                                    \
    {                                                                    \
      printf(COLOR_RED "ASSERTION FAILED" COLOR_RESET " at %s:%d: %s\n", \
             __FILE__, __LINE__, #condition);                            \
      return TEST_FAIL;                                                  \
    }                                                                    \
  } while (0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQUAL(expected, actual)                                         \
  do                                                                           \
  {                                                                            \
    if ((expected) != (actual))                                                \
    {                                                                          \
      printf(COLOR_RED "ASSERTION FAILED" COLOR_RESET " at %s:%d: %s != %s\n", \
             __FILE__, __LINE__, #expected, #actual);                          \
      printf("  Expected: %d\n  Actual: %d\n", (expected), (actual));          \
      return TEST_FAIL;                                                        \
    }                                                                          \
  } while (0)

#define ASSERT_STRING_EQUAL(expected, actual)                                  \
  do                                                                           \
  {                                                                            \
    if (strcmp((expected), (actual)) != 0)                                     \
    {                                                                          \
      printf(COLOR_RED "ASSERTION FAILED" COLOR_RESET " at %s:%d: %s != %s\n", \
             __FILE__, __LINE__, #expected, #actual);                          \
      printf("  Expected: %s\n  Actual: %s\n", (expected), (actual));          \
      return TEST_FAIL;                                                        \
    }                                                                          \
  } while (0)

// Test setup helpers
int create_test_socket(int port)
{
  int sock;
  struct sockaddr_in addr;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("socket creation failed");
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("bind failed");
    close(sock);
    return -1;
  }

  return sock;
}

// Network condition simulators
int should_drop_packet()
{
  // Simulate packet loss based on TEST_PACKET_LOSS_RATE
  return (rand() % 100) < TEST_PACKET_LOSS_RATE;
}

// Execute test function and report result
#define RUN_TEST(test_func)                                               \
  do                                                                      \
  {                                                                       \
    printf(COLOR_YELLOW "Running test: %s" COLOR_RESET "\n", #test_func); \
    int result = test_func();                                             \
    if (result == TEST_PASS)                                              \
    {                                                                     \
      printf(COLOR_GREEN "PASSED: %s" COLOR_RESET "\n\n", #test_func);    \
    }                                                                     \
    else                                                                  \
    {                                                                     \
      printf(COLOR_RED "FAILED: %s" COLOR_RESET "\n\n", #test_func);      \
      exit(TEST_FAIL);                                                    \
    }                                                                     \
  } while (0)

#endif // TEST_UTILS_H
