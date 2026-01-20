# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -g
LDFLAGS = 

# Target executable name
TARGET = testing_server

# Source files and Object files
SRCS = udp_server.c
OBJS = $(SRCS:.c=.o)

# Header files (for dependency tracking)
DEPS = Project_header.h

# Default rule
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile source files into object files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove build artifacts
clean:
	rm -f $(TARGET) $(OBJS)
	rm -f calls_count.txt testing.log

# Phony targets
.PHONY: all clean