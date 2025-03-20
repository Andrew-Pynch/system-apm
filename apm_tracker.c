/**
 * apm_tracker.c - Core functionality for APM (Actions Per Minute) tracking
 */
#include "apm_tracker.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <limits.h>  /* For PATH_MAX */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

// Global variables
EventData *event_buffer = NULL;
int event_count = 0;
int event_index = 0;
int input_fds[MAX_DEVICES];
int num_input_fds = 0;
volatile sig_atomic_t running = 1;

// Find all input devices we should monitor
int find_input_devices(void) {
  DIR *dir;
  struct dirent *entry;
  char device_path[PATH_MAX];
  int fd;

  // Close any previously open file descriptors
  for (int i = 0; i < num_input_fds; i++) {
    if (input_fds[i] >= 0) {
      close(input_fds[i]);
      input_fds[i] = -1;
    }
  }
  num_input_fds = 0;

  dir = opendir("/dev/input");
  if (!dir) {
    perror("Cannot open /dev/input");
    return -1;
  }

  // Initialize all file descriptors to -1
  for (int i = 0; i < MAX_DEVICES; i++) {
    input_fds[i] = -1;
  }

  while ((entry = readdir(dir)) != NULL && num_input_fds < MAX_DEVICES) {
    if (strncmp(entry->d_name, "event", 5) == 0) {
      // Use PATH_MAX to ensure enough space
      if (snprintf(device_path, sizeof(device_path), "/dev/input/%s",
               entry->d_name) >= (int)sizeof(device_path)) {
        fprintf(stderr, "Path truncated: %s\n", entry->d_name);
        continue;
      }
      fd = open(device_path, O_RDONLY | O_NONBLOCK);
      if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", device_path, strerror(errno));
        continue;
      }

      // Check if this device has keyboard or mouse capabilities
      unsigned long evbit[EV_MAX / 8 / sizeof(long) + 1] = {0};
      if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) {
        fprintf(stderr, "Cannot get device capabilities for %s: %s\n", 
                device_path, strerror(errno));
        close(fd);
        continue;
      }

      // Check for keyboard or mouse input capabilities
      if ((evbit[EV_KEY / 8 / sizeof(long)] &
           (1 << (EV_KEY % (8 * sizeof(long))))) ||
          (evbit[EV_REL / 8 / sizeof(long)] &
           (1 << (EV_REL % (8 * sizeof(long)))))) {
        input_fds[num_input_fds++] = fd;
        printf("Monitoring device: %s (fd: %d)\n", device_path, fd);
      } else {
        close(fd);
      }
    }
  }

  closedir(dir);
  
  if (num_input_fds == 0) {
    fprintf(stderr, "No input devices with keyboard/mouse capabilities found\n");
    return -1;
  }
  
  return num_input_fds;
}

// Handler for SIGINT and SIGTERM
void handle_signal(int sig) { 
  (void)sig;  // Mark parameter as intentionally unused
  running = 0; 
}

// Add an event to our buffer
void add_event(void) {
  time_t now = time(NULL);

  if (event_count < MAX_EVENTS) {
    event_buffer[event_index].timestamp = now;
    event_index = (event_index + 1) % MAX_EVENTS;
    event_count++;
  } else {
    // Replace oldest event
    event_buffer[event_index].timestamp = now;
    event_index = (event_index + 1) % MAX_EVENTS;
  }
}

// Calculate APM for the last N minutes
// For testing, allows overriding the current time
float calculate_apm_with_time(int minutes, time_t current_time) {
  if (event_count == 0 || event_buffer == NULL)
    return 0.0f;

  time_t cutoff = current_time - (minutes * 60);
  int count = 0;

  // Loop through all events to count ones in our time window
  if (event_count <= MAX_EVENTS) {
    // Buffer isn't full yet, so events are stored sequentially
    for (int i = 0; i < event_count; i++) {
      if (event_buffer[i].timestamp >= cutoff) {
        count++;
      }
    }
  } else {
    // Buffer is full and circular
    for (int i = 0; i < MAX_EVENTS; i++) {
      int idx = (event_index - MAX_EVENTS + i + MAX_EVENTS) % MAX_EVENTS;
      if (event_buffer[idx].timestamp >= cutoff) {
        count++;
      }
    }
  }

  return (float)count / minutes;
}

// Calculate APM for the last N minutes using current time
float calculate_apm(int minutes) {
  // Use the current time
  return calculate_apm_with_time(minutes, time(NULL));
}

// Save data to binary file
void save_data(void) {
  FILE *file;
  FileHeader header;
  struct stat st = {0};
  
  // Check if we have valid data to save
  if (event_buffer == NULL || event_count == 0) {
    return;
  }

  // Ensure directory exists
  if (stat("/var/lib/apm_tracker", &st) == -1) {
    if (mkdir("/var/lib/apm_tracker", 0755) != 0) {
      perror("Cannot create data directory");
      return;
    }
  }

  file = fopen(DATA_FILE, "wb");
  if (!file) {
    perror("Cannot open data file for writing");
    return;
  }

  // Prepare header
  header.magic = 0x00415041; // "APA\0" in little endian
  header.version = 1;
  header.num_events = event_count;

  // Write header
  if (fwrite(&header, sizeof(header), 1, file) != 1) {
    perror("Failed to write file header");
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

// Load data from binary file
void load_data(void) {
  FILE *file;
  FileHeader header;

  // Check if event buffer has been allocated
  if (event_buffer == NULL) {
    fprintf(stderr, "Event buffer not allocated\n");
    return;
  }

  file = fopen(DATA_FILE, "rb");
  if (!file) {
    // No existing data file, start fresh
    return;
  }

  // Read header
  if (fread(&header, sizeof(header), 1, file) != 1) {
    perror("Failed to read file header");
    fclose(file);
    return;
  }

  // Verify magic number
  if (header.magic != 0x00415041) {
    fprintf(stderr, "Invalid data file format\n");
    fclose(file);
    return;
  }

  // Verify version
  if (header.version != 1) {
    fprintf(stderr, "Unsupported data file version: %u\n", header.version);
    fclose(file);
    return;
  }

  // Load events
  int to_read = header.num_events;
  if (to_read > MAX_EVENTS)
    to_read = MAX_EVENTS;

  // Create temporary buffer to hold the events
  EventData *temp_buffer = malloc(to_read * sizeof(EventData));
  if (!temp_buffer) {
    perror("Failed to allocate memory for loading data");
    fclose(file);
    return;
  }

  // Read all events into temporary buffer
  int events_read = 0;
  for (int i = 0; i < to_read; i++) {
    if (fread(&temp_buffer[i], sizeof(EventData), 1, file) != 1) {
      perror("Failed to read event data");
      break;
    }
    events_read++;
  }

  // Now copy events to the circular buffer in the correct order
  event_count = events_read;
  
  // If buffer isn't full, just copy events sequentially
  if (event_count <= MAX_EVENTS) {
    for (int i = 0; i < event_count; i++) {
      event_buffer[i] = temp_buffer[i];
    }
    event_index = event_count; // Points to the next free slot
  } else {
    // If we have more events than MAX_EVENTS, use only the most recent MAX_EVENTS
    event_count = MAX_EVENTS;
    int start_idx = events_read - MAX_EVENTS;
    for (int i = 0; i < MAX_EVENTS; i++) {
      event_buffer[i] = temp_buffer[start_idx + i];
    }
    event_index = 0; // Circular buffer is full, start overwriting from beginning
  }
  
  free(temp_buffer);
  fclose(file);

  printf("Loaded %d events from data file\n", event_count);
}

// Clean up resources
void cleanup(void) {
  // Save current data
  save_data();

  // Close all input devices
  for (int i = 0; i < num_input_fds; i++) {
    close(input_fds[i]);
  }
  num_input_fds = 0;  // Reset counter

  // Free memory
  if (event_buffer != NULL) {
    free(event_buffer);
    event_buffer = NULL;  // Prevent use-after-free
  }

  printf("APM Tracker stopped\n");
}
