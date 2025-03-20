# APM Tracker

A lightweight system-wide Actions Per Minute (APM) tracker for Linux systems that monitors keyboard and mouse activity across the entire system.

## Features

- Tracks mouse clicks and keyboard presses across the entire system
- Maintains rolling 7-day statistics in an efficient binary format
- Runs as either an interactive application with real-time display or as a daemon
- Uses minimal system resources (CPU, memory, disk I/O)
- Automatically starts on system boot via systemd
- Includes utility for viewing current APM statistics from command line
- Bumblebee-status bar integration for i3wm
- Comprehensive test suite to ensure long-term stability
- Memory-safe implementation with protection against resource leaks

## Requirements

- Linux system with input event subsystem (evdev)
- Root access (required to monitor global input events)
- GCC compiler
- NCurses library for interactive display
- Systemd (for auto-start functionality)
- Bumblebee-status (optional, for i3wm integration)
- Valgrind (optional, for memory leak testing)

## Setup Guide

### Installing Dependencies

```bash
# For Debian/Ubuntu-based distributions:
sudo apt-get install build-essential libncurses5-dev valgrind

# For Fedora/RHEL-based distributions:
sudo dnf install gcc make ncurses-devel valgrind

# For Arch Linux:
sudo pacman -S base-devel ncurses valgrind
```

### Cloning and Building

```bash
# Clone the repository
git clone https://github.com/yourusername/apm-tracker.git
cd apm-tracker

# Build the application
make
```

### Running Tests (Recommended)

Before installation, it's recommended to run the tests to ensure everything works correctly:

```bash
# Run all tests (requires sudo as it needs to access input devices)
sudo ./test.sh
```

### Installation

```bash
# Install as a system service
sudo make install
sudo systemctl enable --now apm_tracker.service
```

## Usage

### Interactive Mode

To run with the live interactive display:

```bash
sudo make run
# OR
sudo apm
```

This will show a real-time display of your APM statistics with the following data:
- Current APM for the last 1 minute
- Current APM for the last 5 minutes
- Current APM for the last hour
- Current APM for the last 24 hours
- Current APM for the last 7 days

Press 'q' to quit.

### Daemon Mode

To run as a background daemon:

```bash
sudo make daemon
```

### Viewing Statistics

To view your current APM statistics at any time:

```bash
sudo apm_stats
```

## i3wm Setup with Bumblebee-Status

### Installing Bumblebee-Status

If you don't have Bumblebee-Status installed:

```bash
# Clone the Bumblebee-Status repository
git clone https://github.com/tobi-wan-kenobi/bumblebee-status.git ~/bumblebee-status
```

### Installing the APM Module

```bash
# Copy the module to your bumblebee-status modules directory
sudo cp apm_tracker.py ~/bumblebee-status/bumblebee_status/modules/contrib/
```

### Configuring i3

Edit your i3 config file (usually at `~/.config/i3/config`) and add:

```
bar {
    status_command ~/bumblebee-status/bumblebee-status \
                -m cpu memory battery date time apm_tracker \
                -p apm_tracker.interval="hour" apm_tracker.prefix="APM" \
                -t gruvbox-powerline
}
```

Reload i3 configuration with `$mod+Shift+r` or run:

```bash
i3-msg restart
```

### Bumblebee-Status APM Module Configuration

The module supports the following parameters:

- `apm_tracker.interval`: Time period to display (options: "minute", "hour", "day", "week")
- `apm_tracker.prefix`: Text label for the module (defaults to "APM")
- `apm_tracker.refresh`: Refresh interval in seconds (defaults to 1)

Example configuration:
```
-p apm_tracker.interval="minute" apm_tracker.prefix="🖱️" apm_tracker.refresh="5"
```

## Shell Scripts

The project includes several shell scripts to simplify usage and management:

### `apm` 

A wrapper script to launch the APM Tracker in interactive mode with proper permissions:

```bash
#!/bin/bash
#
# apm - Shortcut to launch APM Tracker in interactive mode
#

if [ "$EUID" -ne 0 ]; then
    echo "This program requires root permissions to access input devices."
    exec sudo /usr/local/bin/apm_tracker
else
    exec /usr/local/bin/apm_tracker
fi
```

### `update_apm_tracker.sh`

This script automates the process of updating the APM tracker after making code changes:

```bash
#!/bin/bash
#
# APM Tracker Update Script
# This script updates the APM tracker service and i3 bumblebee-status module
#

set -e  # Exit on any error

# Variables
BUMBLEBEE_MODULE_DIR="/home/andrew/bumblebee-status/bumblebee_status/modules/contrib"
SERVICE_DIR="/etc/systemd/system"
BIN_DIR="/usr/local/bin"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
  echo "Please run as root (sudo)"
  exit 1
fi

# 1. Rebuild the C application
# 2. Install the service files
# 3. Update bumblebee-status module
# 4. Set file permissions
# 5. Restart the service
```

### `test.sh`

A comprehensive test script that verifies the correctness and robustness of the application:

```bash
#!/bin/bash
# Comprehensive test script for APM Tracker

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function for successful test
success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Function for warnings
warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Function for failed test
failure() {
    echo -e "${RED}✗ $1${NC}"
    exit 1
}

# Tests compilation, unit tests, integration tests, stats utility, 
# memory leaks, daemon mode, and data file verification
```

## Updating the APM Tracker

### Option 1: Using the Update Script (Recommended)

The included update script automatically rebuilds, installs, and restarts the service:

```bash
sudo ./update_apm_tracker.sh
```

This script:
1. Rebuilds the C application from source
2. Installs the service files to the proper system directories
3. Updates the Bumblebee-Status module
4. Sets the correct file permissions
5. Restarts the service

You may need to edit the script to match your Bumblebee-Status installation path:
```bash
# Edit this line in update_apm_tracker.sh to match your system
BUMBLEBEE_MODULE_DIR="/home/yourusername/bumblebee-status/bumblebee_status/modules/contrib"
```

### Option 2: Manual Updates

To manually update after making changes:

```bash
# Rebuild and install
make clean
make
sudo make install

# Restart the service
sudo systemctl restart apm_tracker.service

# If you modified the Bumblebee-Status module
sudo cp apm_tracker.py ~/bumblebee-status/bumblebee_status/modules/contrib/
i3-msg restart  # Reload i3 to apply the changes
```

## Project Structure

| File                   | Description                                                 |
|------------------------|-------------------------------------------------------------|
| `apm_tracker.c`        | Core functionality for event tracking and data management   |
| `apm_tracker.h`        | Header file with data structures and function declarations  |
| `main.c`               | Main application with interactive display                   |
| `apm_stats.c`          | Utility for displaying APM statistics                       |
| `apm_tracker.py`       | Bumblebee-Status module                                     |
| `apm_tracker.service`  | Systemd service file                                        |
| `apm`                  | Bash script to launch the tracker in interactive mode       |
| `update_apm_tracker.sh`| Script to update and restart the service                    |
| `test.sh`              | Comprehensive test suite for the application                |
| `apm_tracker_test.c`   | Unit tests for core functionality                           |
| `apm_integrated_test.c`| Integration tests for event tracking and storage            |
| `Makefile`             | Build system configuration                                  |

## Testing

### Running the Test Suite

The included test suite verifies that the application works correctly and is robust for long-term operation:

```bash
# Run the full test suite (with colored output)
sudo ./test.sh
```

Individual test targets are also available:

```bash
# Run only unit tests
sudo make test-unit

# Run only integration tests
sudo make test-integration

# Test the stats utility
sudo make test-stats

# Test for memory leaks using Valgrind
sudo make test-memory

# Test the application with Valgrind leak detection
sudo make test-valgrind
```

### Test Coverage

The test suite covers:
1. Event buffer allocation and management
2. APM calculation accuracy
3. Data saving and loading
4. Memory leak detection
5. Circular buffer functionality
6. File handling and error conditions
7. Daemon mode functionality

## Files and Directories

- Binary data is stored in `/var/lib/apm_tracker/apm_data.bin`
- Logs are written to `/var/log/apm_tracker.log`
- Binaries are installed to `/usr/local/bin/`
- Service file is installed to `/etc/systemd/system/`

## Technical Details

### Event Tracking

The APM tracker monitors all input devices with keyboard or mouse functionality through Linux's `/dev/input/eventX` devices. It records timestamps for key presses and mouse clicks in a circular buffer that stores up to 7 days of activity.

### Data Storage

Events are stored in a custom binary format:
- File header with magic number, format version, and event count
- Fixed-size event records with timestamps
- Data is saved every 5 minutes to minimize I/O
- Circular buffer implementation ensures bounded memory usage

### APM Calculation

APM is calculated on-demand for different time periods:
- Last minute (real-time responsiveness)
- Last 5 minutes (short-term trend)
- Last hour (medium-term activity)
- Last 24 hours (daily pattern)
- Last 7 days (weekly pattern)

### Memory Safety

The application includes several safeguards against memory leaks and resource exhaustion:
- Proper cleanup of file descriptors and allocated memory
- Bounds checking on all buffer operations
- Graceful handling of system resource limitations
- Comprehensive error handling for fault tolerance

## Troubleshooting

- **No input events detected**: Ensure you're running as root (sudo)
- **Service fails to start**: Check `/var/log/apm_tracker.log` for specific errors
- **Bumblebee module not working**: Verify the module path and permissions in `~/bumblebee-status/bumblebee_status/modules/contrib/`
- **High CPU usage**: Check for device hotplug events causing repeated input device scanning
- **Missing data file**: Ensure `/var/lib/apm_tracker/` directory exists and is writable

## For Language Models

If you're a language model working with this codebase, here are some pointers:

1. **Core Logic**: The core event tracking logic is in `apm_tracker.c` and the header definitions are in `apm_tracker.h`

2. **Memory Management**: Pay special attention to the `cleanup()` function which handles resource deallocation

3. **Critical Functions**:
   - `find_input_devices()`: Detects and opens input devices
   - `add_event()`: Adds an event to the circular buffer
   - `calculate_apm()`: Calculates APM for a specified time window
   - `save_data()` and `load_data()`: Manage persistent storage

4. **Testing**: The `apm_tracker_test.c` and test automation in `test.sh` ensure the code remains reliable

5. **Common Modifications**: Users typically want to:
   - Modify the time periods for APM calculation
   - Change the data storage location or format
   - Adjust the bumblebee-status module appearance

6. **Safety Considerations**: The code requires root access to monitor input devices, ensure modifications maintain security

## Uninstalling

To completely remove the APM tracker:

```bash
# Stop and disable the service
sudo systemctl stop apm_tracker.service
sudo systemctl disable apm_tracker.service

# Remove installed files
sudo make uninstall

# Remove data files (optional)
sudo rm -rf /var/lib/apm_tracker
sudo rm -f /var/log/apm_tracker.log

# Remove Bumblebee-Status module (if installed)
sudo rm ~/bumblebee-status/bumblebee_status/modules/contrib/apm_tracker.py
```

## License

This software is released under the MIT License. See the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.