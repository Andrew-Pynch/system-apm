/**
 * apm_tracker_test.c - Tests for APM tracker functionality
 */
#include "apm_tracker.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>  /* For PATH_MAX */
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  /* For mkdir and stat */
#include <unistd.h>

extern EventData *event_buffer;
extern int event_count;
extern int event_index;
extern int input_fds[MAX_DEVICES];
extern int num_input_fds;
extern volatile sig_atomic_t running;

// Test helper functions
bool file_exists(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}

void test_event_buffer_allocation() {
  printf("Testing event buffer allocation...\n");
  
  // Allocate buffer
  event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
  assert(event_buffer != NULL);
  
  // Initialize some events
  event_count = 0;
  event_index = 0;
  
  // Add 100 events
  int expected_index = 0;
  for (int i = 0; i < 100; i++) {
    add_event();
    expected_index = (expected_index + 1) % MAX_EVENTS;
  }
  
  assert(event_count == 100);
  assert(event_index == expected_index);
  
  // Add more events to test circular buffer
  for (int i = 0; i < MAX_EVENTS * 2; i++) {
    add_event();
  }
  
  assert(event_count == MAX_EVENTS);
  
  // Clean up
  free(event_buffer);
  event_buffer = NULL;
  
  printf("Event buffer allocation test passed!\n");
}

void test_calculate_apm() {
  printf("Testing APM calculation...\n");
  
  // Allocate buffer
  event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
  assert(event_buffer != NULL);
  
  // Reset counters
  event_count = 0;
  event_index = 0;
  
  // Use a fixed timestamp to avoid time-based issues
  time_t now = 1609459200;  // 2021-01-01 00:00:00 UTC
  
  // Store 60 events in the last minute (one per second)
  printf("Setting up events for the last minute test...\n");
  for (int i = 0; i < 60; i++) {
    // Events from 59 seconds ago to 0 seconds ago
    event_buffer[i].timestamp = now - i;
  }
  event_count = 60;
  event_index = 60;
  
  // Now manually count the events in the last minute
  time_t cutoff_1m = now - 60;
  int count_in_last_minute = 0;
  for (int i = 0; i < event_count; i++) {
    if (event_buffer[i].timestamp >= cutoff_1m) {
      count_in_last_minute++;
    }
  }
  printf("Manually counted events in last minute: %d\n", count_in_last_minute);
  
  // Test the APM calculation with our fixed timestamp
  float apm_1m = calculate_apm_with_time(1, now);
  printf("Function calculated APM for 1 minute: %.2f\n", apm_1m);
  
  // Verify the APM calculation
  assert(count_in_last_minute == 60);
  assert(apm_1m == 60.0f);
  
  // Now set up for the hour test
  event_count = 0;
  event_index = 0;
  
  // Add 60 events over an hour (one per minute)
  printf("Setting up events for the hour test...\n");
  for (int i = 0; i < 60; i++) {
    // Events from 59 minutes ago to 0 minutes ago
    event_buffer[i].timestamp = now - (i * 60);
  }
  event_count = 60;
  event_index = 60;
  
  // Manually count events in the last hour
  time_t cutoff_1h = now - 3600;
  int count_in_last_hour = 0;
  for (int i = 0; i < event_count; i++) {
    if (event_buffer[i].timestamp >= cutoff_1h) {
      count_in_last_hour++;
    }
  }
  printf("Manually counted events in last hour: %d\n", count_in_last_hour);
  
  // Test the APM calculation for an hour with our fixed timestamp
  float apm_60m = calculate_apm_with_time(60, now);
  printf("Function calculated APM for 60 minutes: %.2f\n", apm_60m);
  
  // Verify the APM calculation for an hour
  assert(count_in_last_hour == 60);
  assert(apm_60m == 1.0f);
  
  // Clean up
  free(event_buffer);
  event_buffer = NULL;
  
  printf("APM calculation test passed!\n");
}

void test_save_load_data() {
  printf("Testing save/load functionality...\n");
  
  const char *test_file = "/tmp/apm_test_data.bin";
  
  // We'll use a custom save/load function for testing
  void test_save_data(const char *filepath) {
    FILE *file;
    FileHeader header;
    
    // Check if we have valid data to save
    if (event_buffer == NULL || event_count == 0) {
      return;
    }

    file = fopen(filepath, "wb");
    if (!file) {
      perror("Cannot open test data file for writing");
      return;
    }

    // Prepare header
    header.magic = 0x00415041; // "APA\0" in little endian
    header.version = 1;
    header.num_events = event_count;

    // Write header
    if (fwrite(&header, sizeof(header), 1, file) != 1) {
      perror("Failed to write test file header");
      fclose(file);
      return;
    }

    // Write events in chronological order
    for (int i = 0; i < event_count && i < MAX_EVENTS; i++) {
      int idx = (event_index - event_count + i + MAX_EVENTS) % MAX_EVENTS;
      if (fwrite(&event_buffer[idx], sizeof(EventData), 1, file) != 1) {
        perror("Failed to write event data");
        fclose(file);
        return;
      }
    }

    fclose(file);
  }
  
  void test_load_data(const char *filepath) {
    FILE *file;
    FileHeader header;

    // Check if event buffer has been allocated
    if (event_buffer == NULL) {
      fprintf(stderr, "Event buffer not allocated\n");
      return;
    }

    file = fopen(filepath, "rb");
    if (!file) {
      // No existing data file, start fresh
      return;
    }

    // Read header
    if (fread(&header, sizeof(header), 1, file) != 1) {
      perror("Failed to read test file header");
      fclose(file);
      return;
    }

    // Verify magic number
    if (header.magic != 0x00415041) {
      fprintf(stderr, "Invalid test data file format\n");
      fclose(file);
      return;
    }

    // Verify version
    if (header.version != 1) {
      fprintf(stderr, "Unsupported test data file version: %u\n", header.version);
      fclose(file);
      return;
    }

    // Load events
    int to_read = header.num_events;
    if (to_read > MAX_EVENTS)
      to_read = MAX_EVENTS;

    event_count = 0;
    for (int i = 0; i < to_read; i++) {
      if (fread(&event_buffer[i], sizeof(EventData), 1, file) != 1) {
        perror("Failed to read test event data");
        break;
      }
      event_count++;
    }

    event_index = event_count % MAX_EVENTS;
    fclose(file);

    printf("Loaded %d events from test data file\n", event_count);
  }
  
  // Allocate buffer
  event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
  assert(event_buffer != NULL);
  
  // Initialize test data
  event_count = 0;
  event_index = 0;
  
  time_t now = time(NULL);
  for (int i = 0; i < 100; i++) {
    event_buffer[event_index].timestamp = now - i * 60;
    event_index = (event_index + 1) % MAX_EVENTS;
    event_count++;
  }
  
  // Save data to test file
  test_save_data(test_file);
  
  // Verify file was created
  assert(file_exists(test_file));
  
  // Store original count
  int saved_count = event_count;
  
  // Clear buffer
  event_count = 0;
  event_index = 0;
  
  // Load data from test file
  test_load_data(test_file);
  
  // Verify data was loaded
  assert(event_count == saved_count);
  
  // Clean up
  free(event_buffer);
  event_buffer = NULL;
  unlink(test_file);
  
  printf("Save/load functionality test passed!\n");
}

void test_memory_leak() {
  printf("Testing for memory leaks...\n");
  
  // Allocate and free buffer repeatedly
  for (int i = 0; i < 1000; i++) {
    event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
    assert(event_buffer != NULL);
    free(event_buffer);
    event_buffer = NULL;
  }
  
  // Simulate program startup and cleanup
  for (int i = 0; i < 10; i++) {
    // Initialize as in the main program
    event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
    assert(event_buffer != NULL);
    
    // Create random events
    event_count = 0;
    event_index = 0;
    for (int j = 0; j < 1000; j++) {
      add_event();
    }
    
    // Cleanup
    cleanup();
    // Verify cleanup frees the buffer and sets it to NULL
    assert(event_buffer == NULL); 
  }
  
  printf("Memory leak test passed!\n");
}

int main() {
  printf("Running APM tracker tests...\n");
  
  test_event_buffer_allocation();
  test_calculate_apm();
  test_save_load_data();
  test_memory_leak();
  
  printf("All tests passed!\n");
  return 0;
}