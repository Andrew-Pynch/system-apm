/**
 * apm_stats.c - Utility to display APM statistics from saved data
 */
#include "apm_tracker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float calculate_apm_from_data(EventData *events, int count, int minutes) {
  if (count == 0)
    return 0.0f;

  time_t now = time(NULL);
  time_t cutoff = now - (minutes * 60);
  int action_count = 0;

  for (int i = 0; i < count; i++) {
    if (events[i].timestamp >= cutoff) {
      action_count++;
    }
  }

  return (float)action_count / minutes;
}

int main(int argc, char *argv[]) {
  FILE *file;
  FileHeader header;
  EventData *events = NULL;

  file = fopen(DATA_FILE, "rb");
  if (!file) {
    perror("Cannot open data file");
    return EXIT_FAILURE;
  }

  if (fread(&header, sizeof(header), 1, file) != 1) {
    perror("Cannot read header");
    fclose(file);
    return EXIT_FAILURE;
  }

  if (header.magic != 0x00415041) {
    fprintf(stderr, "Invalid data file format\n");
    fclose(file);
    return EXIT_FAILURE;
  }

  events = (EventData *)malloc(header.num_events * sizeof(EventData));
  if (!events) {
    perror("Cannot allocate memory");
    fclose(file);
    return EXIT_FAILURE;
  }

  if (fread(events, sizeof(EventData), header.num_events, file) !=
      header.num_events) {
    perror("Cannot read events");
    free(events);
    fclose(file);
    return EXIT_FAILURE;
  }

  fclose(file);

  // Calculate APM for different time windows
  float apm_1m = calculate_apm_from_data(events, header.num_events, 1);
  float apm_5m = calculate_apm_from_data(events, header.num_events, 5);
  float apm_15m = calculate_apm_from_data(events, header.num_events, 15);
  float apm_1h = calculate_apm_from_data(events, header.num_events, 60);
  float apm_24h = calculate_apm_from_data(events, header.num_events, 24 * 60);
  float apm_7d =
      calculate_apm_from_data(events, header.num_events, 7 * 24 * 60);

  // Print results
  printf("Actions Per Minute (APM) Statistics:\n");
  printf("--------------------------------\n");
  printf("Last 1 minute:    %.2f APM\n", apm_1m);
  printf("Last 5 minutes:   %.2f APM\n", apm_5m);
  printf("Last 15 minutes:  %.2f APM\n", apm_15m);
  printf("Last hour:        %.2f APM\n", apm_1h);
  printf("Last 24 hours:    %.2f APM\n", apm_24h);
  printf("Last 7 days:      %.2f APM\n", apm_7d);

  free(events);
  return EXIT_SUCCESS;
}
