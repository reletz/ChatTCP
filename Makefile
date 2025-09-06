CC = gcc
CFLAGS = -Wall -Wextra -g
INCLUDES = -Isrc/protocol/include
LIBS = 

# Directories
SRC_DIR = src/protocol/src
INC_DIR = src/protocol/include
TEST_DIR = test
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_BINS = $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%_test,$(TEST_SRCS))

# Main executables
SERVER = $(BIN_DIR)/server_socket
CLIENT = $(BIN_DIR)/client_socket

# Default target
all: directories $(SERVER) $(CLIENT)

# Create directories if they don't exist
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Compile server
$(SERVER): $(OBJ_DIR)/server_socket.o $(filter-out $(OBJ_DIR)/client_socket.o,$(OBJS))
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compile client
$(CLIENT): $(OBJ_DIR)/client_socket.o $(filter-out $(OBJ_DIR)/server_socket.o,$(OBJS))
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compile protocol source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile test files
$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build test executables
$(BIN_DIR)/%_test: $(OBJ_DIR)/%.o $(filter-out $(OBJ_DIR)/server_socket.o $(OBJ_DIR)/client_socket.o,$(OBJS))
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Run all tests
test: directories $(TEST_BINS)
	@echo "Running TCP-over-UDP tests..."
	@for test in $(TEST_BINS); do \
		echo "\n--- Running $$test ---"; \
		./$$test; \
		if [ $$? -ne 0 ]; then \
			echo "--- $$test FAILED ---"; \
			exit 1; \
		else \
			echo "--- $$test PASSED ---"; \
		fi; \
	done
	@echo "\nAll tests passed successfully!"

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all directories test clean
