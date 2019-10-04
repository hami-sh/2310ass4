CC = gcc
CFLAGS = -Wall -pedantic -std=gnu99
DEBUG = -g
TARGETS = 2310depot

# Mark the default target to run (otherwise make will select the first target in the file)
.DEFAULT: all
## Mark targets as not generating output files (ensure the targets will always run)
.PHONY: all debug clean

all: $(TARGETS)

2310depot: 2310depot.c
	$(CC) $(CFLAGS) 2310depot.c -lm -o 2310depot

# Clean up our directory - remove objects and binaries
clean:
	rm -f $(TARGETS) *.o