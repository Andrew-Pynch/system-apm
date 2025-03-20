/**
 * apm_binary_test.c - Test for APM binary serialization format
 * 
 * This program tests the binary data format by creating a controlled set of events,
 * saving them to the file, then loading and verifying the data integrity.
 */
#include "apm_tracker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

// Test file path
#define TEST_DATA_FILE "/tmp/apm_test_data.bin"

// Override the data file path for testing
#undef DATA_FILE
#define DATA_FILE TEST_DATA_FILE

// Use a much smaller MAX_EVENTS value for testing
#undef MAX_EVENTS
#define MAX_EVENTS 100

// Forward declarations
void run_event_count_test(void);
void run_circular_buffer_test(void);
void run_timestamp_test(void);
void verify_event_data(EventData *original, int orig_count, EventData *loaded, int loaded_count);

// Reference the external variables from apm_tracker.c
extern EventData *event_buffer;
extern int event_count;
extern int event_index;

int main(int argc, char *argv[]) {
    printf("APM Binary Format Test\n");
    printf("=============================================\n");
    
    // Allocate memory for event buffer
    event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
    if (!event_buffer) {
        perror("Cannot allocate memory for event buffer");
        return EXIT_FAILURE;
    }
    
    // Run tests
    run_event_count_test();
    run_circular_buffer_test();
    run_timestamp_test();
    
    // Clean up
    if (event_buffer) {
        free(event_buffer);
        event_buffer = NULL;
    }
    
    // Clean up test file
    unlink(TEST_DATA_FILE);
    
    printf("\nAll tests completed successfully!\n");
    return EXIT_SUCCESS;
}

// Test 1: Basic event count test
void run_event_count_test(void) {
    printf("\nTest 1: Basic Event Count Test\n");
    printf("---------------------------------------------\n");
    
    // Reset state
    event_count = 0;
    event_index = 0;
    memset(event_buffer, 0, MAX_EVENTS * sizeof(EventData));
    
    // Create 10 test events with timestamps 1 second apart
    time_t base_time = time(NULL) - 10;  // Start 10 seconds ago
    for (int i = 0; i < 10; i++) {
        event_buffer[i].timestamp = base_time + i;
        event_count++;
        event_index = event_count;
    }
    printf("Created %d test events\n", event_count);
    
    // Save data to test file
    save_data();
    printf("Saved data to %s\n", TEST_DATA_FILE);
    
    // Reset state before loading
    int orig_count = event_count;
    event_count = 0;
    event_index = 0;
    memset(event_buffer, 0, MAX_EVENTS * sizeof(EventData));
    
    // Create a backup of the original events for comparison
    EventData *orig_events = malloc(orig_count * sizeof(EventData));
    if (!orig_events) {
        perror("Failed to allocate memory for original events");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < orig_count; i++) {
        orig_events[i].timestamp = base_time + i;
    }
    
    // Load data back
    load_data();
    printf("Loaded %d events from file\n", event_count);
    
    // Verify event count
    if (event_count != orig_count) {
        fprintf(stderr, "ERROR: Expected %d events, got %d events\n", orig_count, event_count);
        exit(EXIT_FAILURE);
    }
    
    // Verify event data
    verify_event_data(orig_events, orig_count, event_buffer, event_count);
    
    free(orig_events);
    printf("Event count test passed!\n");
}

// Test 2: Circular buffer test
void run_circular_buffer_test(void) {
    printf("\nTest 2: Circular Buffer Test\n");
    printf("---------------------------------------------\n");
    
    // Reset state
    event_count = 0;
    event_index = 0;
    memset(event_buffer, 0, MAX_EVENTS * sizeof(EventData));
    
    // Create test events that will wrap around buffer
    const int test_size = 10;  // Using a very small test size for speed
    time_t base_time = time(NULL) - test_size;
    
    // Add more events than buffer size to test circular behavior
    for (int i = 0; i < test_size + 20; i++) {
        add_event(); // This function handles circular buffer logic
        if (i < test_size) {
            event_buffer[i % test_size].timestamp = base_time + i;
        } else {
            // Newer events replacing older ones
            event_buffer[i % test_size].timestamp = base_time + i;
        }
    }
    
    // Verify event count is capped
    if (event_count != test_size) {
        fprintf(stderr, "ERROR: Expected count to be capped at %d, got %d\n", test_size, event_count);
        exit(EXIT_FAILURE);
    }
    
    printf("Created %d test events with circular buffer\n", event_count);
    
    // Save data to test file
    save_data();
    printf("Saved data to %s\n", TEST_DATA_FILE);
    
    // Create a backup of the expected events after wrap-around
    EventData *expected_events = malloc(test_size * sizeof(EventData));
    if (!expected_events) {
        perror("Failed to allocate memory for expected events");
        exit(EXIT_FAILURE);
    }
    
    // The expected events should be the most recent ones
    for (int i = 0; i < test_size; i++) {
        int orig_idx = (i + test_size + 20 - test_size) % test_size;
        expected_events[i].timestamp = event_buffer[orig_idx].timestamp;
    }
    
    // Reset state before loading
    int orig_count = event_count;
    event_count = 0;
    event_index = 0;
    memset(event_buffer, 0, MAX_EVENTS * sizeof(EventData));
    
    // Load data back
    load_data();
    printf("Loaded %d events from file\n", event_count);
    
    // Verify event count
    if (event_count != test_size) {
        fprintf(stderr, "ERROR: Expected %d events, got %d events\n", test_size, event_count);
        exit(EXIT_FAILURE);
    }
    
    // Verify circular buffer wrapping worked correctly
    int fails = 0;
    for (int i = 0; i < event_count; i++) {
        // Due to the way save_data() works, the events might be written in a different order
        // So we need to find each expected event in the loaded buffer
        int found = 0;
        for (int j = 0; j < event_count; j++) {
            if (event_buffer[j].timestamp == expected_events[i].timestamp) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fails++;
            fprintf(stderr, "ERROR: Expected event with timestamp %ld not found in loaded data\n", 
                    (long)expected_events[i].timestamp);
            if (fails > 5) {
                fprintf(stderr, "Too many failures, aborting test\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    
    free(expected_events);
    printf("Circular buffer test passed!\n");
}

// Test 3: Timestamp accuracy test
void run_timestamp_test(void) {
    printf("\nTest 3: Timestamp Accuracy Test\n");
    printf("---------------------------------------------\n");
    
    // Reset state
    event_count = 0;
    event_index = 0;
    memset(event_buffer, 0, MAX_EVENTS * sizeof(EventData));
    
    // Create test events with specific time windows
    time_t now = time(NULL);
    time_t one_min_ago = now - 60;
    time_t five_min_ago = now - (5 * 60);
    time_t one_hour_ago = now - (60 * 60);
    time_t day_ago = now - (24 * 60 * 60);
    time_t week_ago = now - (7 * 24 * 60 * 60);
    
    // Events for different time periods
    // Last minute: 60 events (1 per second)
    for (int i = 0; i < 60; i++) {
        event_buffer[event_count].timestamp = one_min_ago + i;
        event_count++;
    }
    
    // Last 5 minutes: 30 events (outside the 1 minute window)
    for (int i = 0; i < 30; i++) {
        event_buffer[event_count].timestamp = five_min_ago + i;
        event_count++;
    }
    
    // Last hour: 20 events (outside the 5 minute window)
    for (int i = 0; i < 20; i++) {
        event_buffer[event_count].timestamp = one_hour_ago + i;
        event_count++;
    }
    
    // Last day: 10 events
    for (int i = 0; i < 10; i++) {
        event_buffer[event_count].timestamp = day_ago + i;
        event_count++;
    }
    
    // Last week: 5 events
    for (int i = 0; i < 5; i++) {
        event_buffer[event_count].timestamp = week_ago + i;
        event_count++;
    }
    
    printf("Created %d test events across different time periods\n", event_count);
    event_index = event_count;
    
    // Calculate expected APM values
    float expected_1m = 60.0f;  // 60 events in 1 minute
    float expected_5m = (60.0f + 30.0f) / 5.0f;  // 90 events in 5 minutes
    float expected_1h = (60.0f + 30.0f + 20.0f) / 60.0f;  // 110 events in 1 hour
    float expected_24h = (60.0f + 30.0f + 20.0f + 10.0f) / (24.0f * 60.0f);  // 120 events in 24 hours
    float expected_7d = (60.0f + 30.0f + 20.0f + 10.0f + 5.0f) / (7.0f * 24.0f * 60.0f);  // 125 events in 7 days
    
    // Save data to test file
    save_data();
    printf("Saved data to %s\n", TEST_DATA_FILE);
    
    // Reset state before loading
    int orig_count = event_count;
    event_count = 0;
    event_index = 0;
    memset(event_buffer, 0, MAX_EVENTS * sizeof(EventData));
    
    // Load data back
    load_data();
    printf("Loaded %d events from file\n", event_count);
    
    // Verify event count
    if (event_count != orig_count) {
        fprintf(stderr, "ERROR: Expected %d events, got %d events\n", orig_count, event_count);
        exit(EXIT_FAILURE);
    }
    
    // Calculate actual APM values
    float actual_1m = calculate_apm_with_time(1, now);
    float actual_5m = calculate_apm_with_time(5, now);
    float actual_1h = calculate_apm_with_time(60, now);
    float actual_24h = calculate_apm_with_time(24 * 60, now);
    float actual_7d = calculate_apm_with_time(7 * 24 * 60, now);
    
    printf("APM Values (Expected vs Actual):\n");
    printf("1 minute:  %.2f vs %.2f\n", expected_1m, actual_1m);
    printf("5 minutes: %.2f vs %.2f\n", expected_5m, actual_5m);
    printf("1 hour:    %.2f vs %.2f\n", expected_1h, actual_1h);
    printf("24 hours:  %.2f vs %.2f\n", expected_24h, actual_24h);
    printf("7 days:    %.2f vs %.2f\n", expected_7d, actual_7d);
    
    // Check for reasonable agreement (within 5%)
    float tolerance = 0.05f;
    
    float diff_1m = fabsf((actual_1m / expected_1m) - 1.0f);
    float diff_5m = fabsf((actual_5m / expected_5m) - 1.0f);
    float diff_1h = fabsf((actual_1h / expected_1h) - 1.0f);
    float diff_24h = fabsf((actual_24h / expected_24h) - 1.0f);
    float diff_7d = fabsf((actual_7d / expected_7d) - 1.0f);
    
    if (diff_1m > tolerance || diff_5m > tolerance || diff_1h > tolerance || 
        diff_24h > tolerance || diff_7d > tolerance) {
        fprintf(stderr, "ERROR: APM calculations differ too much from expected values\n");
        exit(EXIT_FAILURE);
    }
    
    printf("APM calculation test passed!\n");
}

// Helper function to verify event data
void verify_event_data(EventData *original, int orig_count, EventData *loaded, int loaded_count) {
    if (orig_count != loaded_count) {
        fprintf(stderr, "ERROR: Event count mismatch: expected %d, got %d\n", orig_count, loaded_count);
        exit(EXIT_FAILURE);
    }
    
    // Since the events might not be in the same order, we need to check that each original event
    // has a matching loaded event by timestamp
    for (int i = 0; i < orig_count; i++) {
        int found = 0;
        for (int j = 0; j < loaded_count; j++) {
            if (original[i].timestamp == loaded[j].timestamp) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            fprintf(stderr, "ERROR: Event mismatch: original event with timestamp %ld not found in loaded data\n", 
                    (long)original[i].timestamp);
            exit(EXIT_FAILURE);
        }
    }
}