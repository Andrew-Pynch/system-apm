CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm -lncurses

.PHONY: all clean install run test daemon test-stats test-unit test-memory test-valgrind test-integration test-binary

all: apm_tracker apm_stats apm_tracker_test apm_integrated_test apm_binary_test

apm_tracker: main.o apm_tracker.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

apm_stats: apm_stats.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

apm_tracker_test: apm_tracker_test.o apm_tracker.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	
apm_integrated_test: apm_integrated_test.o apm_tracker.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

apm_binary_test: apm_binary_test.o apm_tracker.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

main.o: main.c apm_tracker.h
	$(CC) $(CFLAGS) -c $<

apm_tracker.o: apm_tracker.c apm_tracker.h
	$(CC) $(CFLAGS) -c $<

apm_stats.o: apm_stats.c apm_tracker.h
	$(CC) $(CFLAGS) -c $<

apm_tracker_test.o: apm_tracker_test.c apm_tracker.h
	$(CC) $(CFLAGS) -c $<
	
apm_integrated_test.o: apm_integrated_test.c apm_tracker.h
	$(CC) $(CFLAGS) -c $<

apm_binary_test.o: apm_binary_test.c apm_tracker.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f apm_tracker apm_stats apm_tracker_test apm_integrated_test apm_binary_test *.o
	rm -f /tmp/apm_test_data.bin
	rm -f /tmp/apm_integrated_test.bin

install: all
	mkdir -p /var/lib/apm_tracker
	install -m 755 apm_tracker /usr/local/bin/
	install -m 755 apm_stats /usr/local/bin/
	install -m 755 apm /usr/local/bin/
	install -m 644 apm_tracker.service /etc/systemd/system/
	systemctl daemon-reload
	@echo "To start the service, run: systemctl enable --now apm_tracker.service"
	@echo "To view live APM stats, run the 'apm' command in any terminal"

uninstall:
	systemctl stop apm_tracker.service || true
	systemctl disable apm_tracker.service || true
	rm -f /usr/local/bin/apm_tracker
	rm -f /usr/local/bin/apm_stats
	rm -f /usr/local/bin/apm
	rm -f /etc/systemd/system/apm_tracker.service
	systemctl daemon-reload

# Run in interactive mode with live APM display
run: apm_tracker
	@echo "Running in interactive mode. Press 'q' to quit."
	@sudo ./apm_tracker

# Run in daemon mode
daemon: apm_tracker
	@echo "Running in daemon mode."
	@sudo ./apm_tracker -d

# Test that the stats utility works
test-stats: apm_stats
	@echo "Testing stats utility..."
	@sudo ./apm_stats

# Run unit tests
test-unit: apm_tracker_test
	@echo "Running unit tests..."
	@sudo ./apm_tracker_test

# Run integration test
test-integration: apm_integrated_test
	@echo "Running integration tests..."
	@sudo ./apm_integrated_test

# Test for memory leaks with Valgrind (if available)
test-memory: apm_tracker_test
	@if command -v valgrind > /dev/null; then \
		echo "Running memory leak tests with Valgrind..."; \
		sudo valgrind --leak-check=full --error-exitcode=1 ./apm_tracker_test; \
	else \
		echo "Valgrind not found. Running standard memory tests..."; \
		sudo ./apm_tracker_test; \
	fi

# Test binary serialization format
test-binary: apm_binary_test
	@echo "Running binary serialization tests..."
	@sudo ./apm_binary_test

# Run all tests
test: test-unit test-integration test-stats test-binary
	@echo "All tests completed successfully!"

# Run with Valgrind to check for memory leaks
test-valgrind: apm_tracker
	@if command -v valgrind > /dev/null; then \
		echo "Running with Valgrind for leak detection (press Ctrl+C to exit)..."; \
		sudo valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./apm_tracker; \
	else \
		echo "Valgrind not found. Please install it to test for memory leaks."; \
	fi
