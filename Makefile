# Name of the binary
BINARY = bankingsystem
# Object files
OBJS = bankingsystem.o
# Compiler for C (gcc for C, instead of g++ for C++)
CC = gcc
# Compiler flags
CFLAGS = -Werror -Wall -I.
# Linker flags
LFLAGS =

# All target: builds all important targets
all: $(BINARY)

# Links the binary
$(BINARY): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) -pthread

# Compiles a source file into an object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rules can not only be used for compiling a program but also for executing a program
run: $(BINARY)
	./$(BINARY)

# Delete all build artifacts
clean:
	rm -rf $(BINARY) $(OBJS)

# All and clean are "phony" targets, meaning they are not files
.PHONY: all clean run
