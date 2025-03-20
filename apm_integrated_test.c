/**
 * apm_integrated_test.c - Integration tests for APM tracker
 */
#include "apm_tracker.h"
#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern EventData *event_buffer;
extern int event_count;
extern int event_index;

// Test integrated functionality (events tracked and stored correctly)
int main(void) {
  const char *test_file = "/tmp/apm_integrated_test.bin";
  
  printf("Running APM tracker integration test...\n");
  
  // Allocate memory for event buffer
  event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
  if (!event_buffer) {
    perror("Cannot allocate memory for event buffer");
    return EXIT_FAILURE;
  }
  
  // Override data file for testing
  #define TEST_DATA_FILE test_file
  
  // Initialize counters
  event_count = 0;
  event_index = 0;
  
  // Simulate some events
  printf("Adding test events...\n");
  for (int i = 0; i < 100; i++) {
    add_event();
  }
  
  // Verify events were added
  printf("Verifying events were added...\n");
  assert(event_count > 0);
  
  // Calculate APM
  float apm_1m = calculate_apm(1);
  printf("Current APM (1 minute): %.2f\n", apm_1m);
  
  // Save data to test file
  FILE *file;
  FileHeader header;
  
  // Ensure test directory exists
  struct stat st = {0};
  char test_dir[PATH_MAX];
  snprintf(test_dir, sizeof(test_dir), "/tmp");
  if (stat(test_dir, &st) == -1) {
    mkdir(test_dir, 0755);
  }
  
  file = fopen(test_file, "wb");
  if (!file) {
    perror("Cannot open test data file for writing");
    free(event_buffer);
    return EXIT_FAILURE;
  }
  
  // Prepare header
  header.magic = 0x00415041; // "APA\0" in little endian
  header.version = 1;
  header.num_events = event_count;
  
  // Write header
  if (fwrite(&header, sizeof(header), 1, file) != 1) {
    perror("Failed to write test file header");
    fclose(file);
    free(event_buffer);
    return EXIT_FAILURE;
  }
  
  // Write events
  for (int i = 0; i < event_count && i < MAX_EVENTS; i++) {
    int idx = (event_index - event_count + i + MAX_EVENTS) % MAX_EVENTS;
    if (fwrite(&event_buffer[idx], sizeof(EventData), 1, file) != 1) {
      perror("Failed to write event data");
      fclose(file);
      free(event_buffer);
      return EXIT_FAILURE;
    }
  }
  
  fclose(file);
  printf("Saved events to test file: %s\n", test_file);
  
  // Clear buffer and reload
  printf("Clearing buffer and reloading from file...\n");
  event_count = 0;
  event_index = 0;
  
  file = fopen(test_file, "rb");
  if (!file) {
    perror("Cannot open test data file for reading");
    free(event_buffer);
    return EXIT_FAILURE;
  }
  
  // Read header
  if (fread(&header, sizeof(header), 1, file) != 1) {
    perror("Failed to read test file header");
    fclose(file);
    free(event_buffer);
    return EXIT_FAILURE;
  }
  
  // Verify magic number
  if (header.magic != 0x00415041) {
    fprintf(stderr, "Invalid test data file format\n");
    fclose(file);
    free(event_buffer);
    return EXIT_FAILURE;
  }
  
  // Load events
  int to_read = header.num_events;
  if (to_read > MAX_EVENTS)
    to_read = MAX_EVENTS;
  
  for (int i = 0; i < to_read; i++) {
    if (fread(&event_buffer[i], sizeof(EventData), 1, file) != 1) {
      perror("Failed to read test event data");
      break;
    }
    event_count++;
  }
  
  event_index = event_count % MAX_EVENTS;
  fclose(file);
  
  printf("Loaded %d events from test file\n", event_count);
  
  // Verify loaded data
  assert(event_count == 100);
  
  // Calculate APM again
  apm_1m = calculate_apm(1);
  printf("APM after reload (1 minute): %.2f\n", apm_1m);
  
  // Clean up
  free(event_buffer);
  unlink(test_file);
  
  printf("APM tracker integration test passed!\n");
  return EXIT_SUCCESS;
}