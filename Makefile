CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2
LDFLAGS := -pthread

COMMON_SRCS := auth.c inventory.c supply.c distribution.c reports.c logging.c util.c menu.c concurrency.c

.PHONY: all run run-server run-client stress tsan-server clean help

all: msms msms_server msms_client

msms: main.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

msms_server: server.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

msms_client: client.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

run: msms
	./msms

run-server: msms_server
	./msms_server

run-client: msms_client
	./msms_client

# Requires msms_server already running in another terminal (make run-server).
stress: msms_server stress_test.py
	python3 stress_test.py

# Debug build of the server instrumented with ThreadSanitizer, for
# catching data races directly instead of inferring them from output.
tsan-server: server.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -fsanitize=thread -g -O0 -o msms_server_tsan $^

clean:
	rm -f msms msms_server msms_client msms_server_tsan
	rm -rf data

help:
	@echo "make            - build msms, msms_server, msms_client"
	@echo "make run        - build (if needed) and run the standalone single-user CLI"
	@echo "make run-server - build (if needed) and run the multi-client server"
	@echo "make run-client - build (if needed) and run one client (run in a 2nd terminal)"
	@echo "make stress     - run the concurrency stress test against a running server"
	@echo "make tsan-server - build a ThreadSanitizer-instrumented server for race detection"
	@echo "make clean      - remove built binaries and the data/ directory"