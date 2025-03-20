/**
 * apm_tracker.h - Header for APM (Actions Per Minute) tracking system
 */
#ifndef APM_TRACKER_H
#define APM_TRACKER_H

#include <signal.h>
#include <stdint.h>
#include <time.h>

#define MAX_EVENTS                                                             \
  7 * 24 * 60 * 60 // Max events for 7 days (assuming 1 event per second)
#define DATA_FILE "/var/lib/apm_tracker/apm_data.bin"
#define LOG_FILE "/var/log/apm_tracker.log"
#define MAX_DEVICES 32

// Custom binary format header
typedef struct {
  uint32_t magic;      // Magic number: "APM\0"
  uint32_t version;    // Version of the format
  uint32_t num_events; // Number of events in file
} FileHeader;

// Event data structure
typedef struct {
  time_t timestamp; // Time when event occurred
} EventData;

// Function declarations
void cleanup(void);
void handle_signal(int sig);
int find_input_devices(void);
void save_data(void);
void load_data(void);
void add_event(void);
float calculate_apm(int minutes);
float calculate_apm_with_time(int minutes, time_t current_time);
int clear_data(void);

#endif /* APM_TRACKER_H */
