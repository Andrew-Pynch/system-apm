#!/bin/bash
#
# APM Tracker Update Script
# This script updates the APM tracker service and i3 bumblebee-status module
#

set -e  # Exit on any error

# Show what we're doing
echo "============================================"
echo "APM Tracker Update Script"
echo "============================================"

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

# Step 1: Rebuild the C application
echo "Rebuilding APM tracker..."
cd "$SCRIPT_DIR"
make clean
make

# Step 2: Install the service files
echo "Installing service files..."
cp apm_tracker "$BIN_DIR/"
cp apm_stats "$BIN_DIR/"
cp apm_tracker.service "$SERVICE_DIR/"

# Step 3: Update bumblebee-status module
echo "Updating bumblebee-status module..."
cp apm_tracker.py "$BUMBLEBEE_MODULE_DIR/"

# Step 4: Update file permissions
echo "Setting file permissions..."
chmod 755 "$BIN_DIR/apm_tracker"
chmod 755 "$BIN_DIR/apm_stats"
chmod 644 "$SERVICE_DIR/apm_tracker.service"
chmod 644 "$BUMBLEBEE_MODULE_DIR/apm_tracker.py"

# Step 5: Restart the service
echo "Restarting APM tracker service..."
systemctl daemon-reload
systemctl restart apm_tracker.service

echo "============================================"
echo "Update complete!"
echo ""
echo "Service status:"
systemctl status apm_tracker.service --no-pager
echo ""
echo "To reload i3 and apply the bumblebee-status changes, run:"
echo "i3-msg restart"
echo "============================================"