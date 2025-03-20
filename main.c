/**
 * main.c - Main entry point for APM (Actions Per Minute) tracking system
 * Provides real-time display of APM metrics
 */
#include "apm_tracker.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <limits.h>  /* For PATH_MAX */
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>  /* For mkdir and stat */
#include <termios.h>
#include <time.h>
#include <unistd.h>

// Define constants for the historical data graphs
#define HISTORY_SIZE 60  // Store 60 data points for the last 60 seconds/minutes
#define GRAPH_HEIGHT 8   // Height of the graph in rows
#define GRAPH_WIDTH 60   // Width of the graph in columns
#define GRAPH_CHARS " ▁▂▃▄▅▆▇█"  // Characters used for drawing the graph

// Structure to hold historical APM data
typedef struct {
    float data[HISTORY_SIZE];  // Circular buffer of values
    int index;                 // Current position in buffer
    int count;                 // Number of data points collected
    float max_value;           // Maximum value in the dataset
    char label[32];            // Label for this dataset
    time_t last_update;        // Last time this was updated
} APMHistory;

// External declarations for global variables defined in apm_tracker.c
extern EventData *event_buffer;
extern int event_count;
extern int input_fds[MAX_DEVICES];
extern int num_input_fds;
extern volatile sig_atomic_t running;

// ANSI color codes
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_RESET "\x1b[0m"

// Global variables for APM history
APMHistory apm_1m_history;
APMHistory apm_5m_history;
APMHistory apm_1h_history;
APMHistory apm_24h_history;
APMHistory apm_7d_history;
int show_graphs = 0;  // Global variable to control graph display

// Initialize APM history data structure
void init_apm_history(APMHistory *history, const char *label) {
    memset(history->data, 0, sizeof(history->data));
    history->index = 0;
    history->count = 0;
    history->max_value = 1.0f;  // Start with a reasonable default
    strncpy(history->label, label, sizeof(history->label) - 1);
    history->label[sizeof(history->label) - 1] = '\0';  // Ensure null termination
    history->last_update = 0;
}

// Update APM history with a new value
void update_apm_history(APMHistory *history, float value, time_t now, int update_interval) {
    // Only update if enough time has passed since the last update
    if (now - history->last_update >= update_interval) {
        history->data[history->index] = value;
        history->index = (history->index + 1) % HISTORY_SIZE;
        if (history->count < HISTORY_SIZE) {
            history->count++;
        }
        
        // Update the maximum value for scaling
        history->max_value = 1.0f;  // Minimum max value to avoid division by zero
        for (int i = 0; i < history->count; i++) {
            if (history->data[i] > history->max_value) {
                history->max_value = history->data[i];
            }
        }
        
        history->last_update = now;
    }
}

// Draw an ASCII graph of the APM history
void draw_apm_graph(WINDOW *win, int y, int x, APMHistory *history, int color_pair) {
    (void)win;  // Unused parameter, but keeping for future expansion
    if (history->count == 0) return;

    // Draw the label
    attron(COLOR_PAIR(color_pair) | A_BOLD);
    mvprintw(y, x, "%s", history->label);
    attroff(A_BOLD);
    
    // Draw the y-axis and max value
    mvprintw(y, x + strlen(history->label) + 1, "%.1f", history->max_value);
    
    // Draw the graph border
    for (int i = 0; i < GRAPH_WIDTH + 2; i++) {
        mvprintw(y + GRAPH_HEIGHT + 1, x + i, "-");
    }
    for (int i = 0; i <= GRAPH_HEIGHT + 1; i++) {
        mvprintw(y + i, x, "|");
        mvprintw(y + i, x + GRAPH_WIDTH + 1, "|");
    }
    
    // Draw the time markers on x-axis
    mvprintw(y + GRAPH_HEIGHT + 2, x, "0");
    mvprintw(y + GRAPH_HEIGHT + 2, x + GRAPH_WIDTH - 8, "-%d", HISTORY_SIZE);
    
    // Draw the graph data
    for (int i = 0; i < history->count && i < GRAPH_WIDTH; i++) {
        int data_idx = (history->index - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
        float value = history->data[data_idx];
        float normalized = value / (history->max_value > 0 ? history->max_value : 1.0f);
        
        // Map normalized value directly to character height
        int height_level = (int)(normalized * GRAPH_HEIGHT);
        
        if (height_level < 0) height_level = 0;
        if (height_level > GRAPH_HEIGHT) height_level = GRAPH_HEIGHT;
        
        // Draw vertical bar using block characters
        attron(COLOR_PAIR(color_pair));
        for (int j = 0; j < height_level; j++) {
            mvprintw(y + GRAPH_HEIGHT - j, x + 1 + i, "%s", "█");
        }
        attroff(COLOR_PAIR(color_pair));
    }
    
    attroff(COLOR_PAIR(color_pair));
}

// Function to update display
void update_display(int total_events, float apm_1m, float apm_5m, float apm_1h,
                    float apm_24h, float apm_7d) {
  // Use the global show_graphs variable
  // Get terminal dimensions
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);
  
  // Clear screen
  clear();

  // Print header
  attron(A_BOLD);
  mvprintw(0, 0, "APM Tracker - Real-time Actions Per Minute Monitor");
  attroff(A_BOLD);
  mvprintw(1, 0,
           "================================================================");

  // Print current APM values (left side)
  mvprintw(3, 0, "Total Events Recorded: %d", total_events);

  int value_y = 5;
  attron(COLOR_PAIR(1));
  mvprintw(value_y++, 0, "Last 1 minute:     %.2f APM", apm_1m);
  attroff(COLOR_PAIR(1));

  attron(COLOR_PAIR(2));
  mvprintw(value_y++, 0, "Last 5 minutes:    %.2f APM", apm_5m);
  attroff(COLOR_PAIR(2));

  attron(COLOR_PAIR(3));
  mvprintw(value_y++, 0, "Last 1 hour:       %.2f APM", apm_1h);
  attroff(COLOR_PAIR(3));

  attron(COLOR_PAIR(4));
  mvprintw(value_y++, 0, "Last 24 hours:     %.2f APM", apm_24h);
  attroff(COLOR_PAIR(4));

  attron(COLOR_PAIR(5));
  mvprintw(value_y++, 0, "Last 7 days:       %.2f APM", apm_7d);
  attroff(COLOR_PAIR(5));

  // Always update history data even if not showing graphs
  time_t now = time(NULL);
  update_apm_history(&apm_1m_history, apm_1m, now, 1);       // Update every second
  update_apm_history(&apm_5m_history, apm_5m, now, 5);       // Update every 5 seconds
  update_apm_history(&apm_1h_history, apm_1h, now, 60);      // Update every minute
  update_apm_history(&apm_24h_history, apm_24h, now, 300);   // Update every 5 minutes
  update_apm_history(&apm_7d_history, apm_7d, now, 900);     // Update every 15 minutes
  
  // Draw graphs if enabled and terminal is large enough
  if (show_graphs && max_x >= 100) { // Need at least 100 columns
    // Draw the current graph based on terminal size
    int graph_y = 14;
    int graph_spacing = GRAPH_HEIGHT + 5;
    
    // Draw graphs based on available vertical space
    if (max_y >= graph_y + graph_spacing) {
      attron(A_BOLD);
      mvprintw(graph_y - 2, 0, "APM Graphs (showing last %d data points)", HISTORY_SIZE);
      attroff(A_BOLD);
      
      draw_apm_graph(stdscr, graph_y, 2, &apm_1m_history, 1);
      
      if (max_y >= graph_y + graph_spacing * 2) {
        draw_apm_graph(stdscr, graph_y + graph_spacing, 2, &apm_5m_history, 2);
        
        if (max_y >= graph_y + graph_spacing * 3) {
          draw_apm_graph(stdscr, graph_y + graph_spacing * 2, 2, &apm_1h_history, 3);
          
          if (max_y >= graph_y + graph_spacing * 4) {
            draw_apm_graph(stdscr, graph_y + graph_spacing * 3, 2, &apm_24h_history, 4);
            
            if (max_y >= graph_y + graph_spacing * 5) {
              draw_apm_graph(stdscr, graph_y + graph_spacing * 4, 2, &apm_7d_history, 5);
            }
          }
        }
      }
    }
  } else if (show_graphs) {
    // User wants graphs but terminal is too narrow
    mvprintw(value_y + 2, 0, "Terminal too narrow for graphs. Please resize (need >= 100 cols)");
  } else {
    // Graphs are disabled
    mvprintw(value_y + 2, 0, "Graphs are disabled. Press 'g' to enable.");
  }

  // Print instructions at the bottom
  mvprintw(max_y - 2, 0,
           "----------------------------------------------------------------");
  mvprintw(max_y - 1, 0, "Press 'q' to quit, 'g' to toggle graphs, or run as a daemon with '-d'");

  // Refresh screen
  refresh();
}

int main(int argc, char *argv[]) {
  struct input_event events[64];
  int result, size;
  fd_set readfds;
  struct timeval tv;
  FILE *log_file;
  time_t last_save = 0;
  time_t last_stats = 0;
  time_t last_update = 0;
  int daemon_mode = 0;
  
  // show_graphs is already defined as a global variable

  // Check for command-line arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      daemon_mode = 1;
    } else if (strcmp(argv[i], "--clear") == 0) {
      // Allocate memory for event buffer if it hasn't been allocated yet
      if (event_buffer == NULL) {
        event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
        if (!event_buffer) {
          perror("Cannot allocate memory for event buffer");
          return EXIT_FAILURE;
        }
      }
      
      // Clear data and exit
      int result = clear_data();
      // Free memory before exiting
      if (event_buffer != NULL) {
        free(event_buffer);
        event_buffer = NULL;
      }
      return (result == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  }

  // Ensure log directory exists
  struct stat st = {0};
  if (stat("/var/log", &st) == -1) {
    if (mkdir("/var/log", 0755) != 0) {
      perror("Cannot create log directory");
      return EXIT_FAILURE;
    }
  }

  // Open log file
  log_file = fopen(LOG_FILE, "a");
  if (!log_file) {
    perror("Cannot open log file");
    return EXIT_FAILURE;
  }
  
  // Set line buffering for more timely log writes
  setvbuf(log_file, NULL, _IOLBF, 0);

  // Allocate memory for event buffer
  event_buffer = (EventData *)malloc(MAX_EVENTS * sizeof(EventData));
  if (!event_buffer) {
    perror("Cannot allocate memory for event buffer");
    fclose(log_file);
    return EXIT_FAILURE;
  }

  // Set up signal handlers
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  // Find input devices
  if (find_input_devices() <= 0) {
    fprintf(stderr, "No suitable input devices found\n");
    fprintf(log_file, "[%ld] ERROR: No suitable input devices found\n", time(NULL));
    fflush(log_file);
    free(event_buffer);
    event_buffer = NULL;  // Prevent use-after-free
    fclose(log_file);
    return EXIT_FAILURE;
  }

  // Load existing data
  load_data();

  fprintf(log_file, "[%ld] APM Tracker started\n", time(NULL));
  fflush(log_file);

  // Initialize APM history structures
  init_apm_history(&apm_1m_history, "1-Minute APM");
  init_apm_history(&apm_5m_history, "5-Minute APM");
  init_apm_history(&apm_1h_history, "1-Hour APM");
  init_apm_history(&apm_24h_history, "24-Hour APM");
  init_apm_history(&apm_7d_history, "7-Day APM");

  // Initialize ncurses if not in daemon mode
  if (!daemon_mode) {
    initscr();            // Start curses mode
    start_color();        // Enable colors
    cbreak();             // Line buffering disabled
    keypad(stdscr, TRUE); // Enable function keys
    noecho();             // Don't echo input
    curs_set(0);          // Hide cursor
    timeout(100);         // Non-blocking getch

    // Define color pairs
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK);

    // Initial display update
    float apm_1m = calculate_apm(1);
    float apm_5m = calculate_apm(5);
    float apm_1h = calculate_apm(60);
    float apm_24h = calculate_apm(24 * 60);
    float apm_7d = calculate_apm(7 * 24 * 60);
    update_display(event_count, apm_1m, apm_5m, apm_1h, apm_24h, apm_7d);
  } else {
    printf("APM Tracker started in daemon mode\n");
  }

  // Main event loop
  while (running) {
    FD_ZERO(&readfds);
    int max_fd = 0;

    // Add all input devices to select set
    for (int i = 0; i < num_input_fds; i++) {
      FD_SET(input_fds[i], &readfds);
      if (input_fds[i] > max_fd)
        max_fd = input_fds[i];
    }

    // Set timeout for select
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms

    result = select(max_fd + 1, &readfds, NULL, NULL, &tv);
    if (result == -1) {
      if (errno == EINTR)
        continue; // Interrupted by signal
      perror("select");
      break;
    }

    // Check for events on each device
    if (result > 0) {
      for (int i = 0; i < num_input_fds; i++) {
        if (FD_ISSET(input_fds[i], &readfds)) {
          size = read(input_fds[i], events, sizeof(events));
          if (size < 0) {
            if (errno != EAGAIN) {
              perror("read");
            }
            continue;
          }

          // Process events
          int num_events = size / sizeof(struct input_event);
          for (int j = 0; j < num_events; j++) {
            if ((events[j].type == EV_KEY && events[j].value == 1) ||
                (events[j].type == EV_KEY && events[j].code == BTN_LEFT &&
                 events[j].value == 1)) {
              // Key press or mouse click
              add_event();
            }
          }
        }
      }
    }

    // Check for user input in interactive mode
    if (!daemon_mode) {
      int ch = getch();
      if (ch == 'q' || ch == 'Q') {
        running = 0;
      } else if (ch == 'g' || ch == 'G') {
        show_graphs = !show_graphs;  // Toggle graph display
      }
    }

    // Periodic tasks
    time_t now = time(NULL);

    // Save data every 5 minutes
    if (now - last_save >= 300) {
      save_data();
      last_save = now;
    }

    // Log statistics every hour
    if (now - last_stats >= 3600) {
      float apm_1h = calculate_apm(60);
      float apm_24h = calculate_apm(24 * 60);
      float apm_7d = calculate_apm(7 * 24 * 60);

      fprintf(log_file, "[%ld] APM - 1h: %.2f, 24h: %.2f, 7d: %.2f\n", now,
              apm_1h, apm_24h, apm_7d);
      fflush(log_file);

      last_stats = now;
    }

    // Update display every second in interactive mode
    if (!daemon_mode && now - last_update >= 1) {
      float apm_1m = calculate_apm(1);
      float apm_5m = calculate_apm(5);
      float apm_1h = calculate_apm(60);
      float apm_24h = calculate_apm(24 * 60);
      float apm_7d = calculate_apm(7 * 24 * 60);
      // Pass updated APM values to display function
      update_display(event_count, apm_1m, apm_5m, apm_1h, apm_24h, apm_7d);
      last_update = now;
    }
  }

  // Clean up
  cleanup();
  fclose(log_file);

  if (!daemon_mode) {
    endwin(); // End curses mode
  }

  return EXIT_SUCCESS;
}
