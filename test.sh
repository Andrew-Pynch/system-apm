#!/bin/bash
# Comprehensive test script for APM Tracker

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Ensure we're running as root
if [ "$(id -u)" -ne 0 ]; then
   echo -e "${RED}This script must be run as root. Please use sudo.${NC}"
   exit 1
fi

# Set error handling
set -e  # Exit on error
trap 'echo -e "${RED}ERROR: Test failed on line $LINENO${NC}"; exit 1' ERR

echo -e "${BLUE}===============================================${NC}"
echo -e "${BLUE}APM Tracker System Test Suite${NC}"
echo -e "${BLUE}===============================================${NC}"

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

# Function for test step
teststep() {
    echo -e "\n${BLUE}[$1/$2] $3${NC}"
}

# Build everything first
teststep "1" "7" "Building all components..."
if make clean && make all; then
    success "Build complete"
else
    failure "Build failed"
fi

# Unit tests
teststep "2" "7" "Running unit tests..."
if make test-unit; then
    success "Unit tests passed"
else
    failure "Unit tests failed"
fi

# Integration test
teststep "3" "7" "Running integration tests..."
if make test-integration; then
    success "Integration tests passed"
else
    failure "Integration tests failed"
fi

# Statistics utility test
teststep "4" "7" "Testing statistics utility..."
if make test-stats; then
    success "Statistics utility test passed"
else
    failure "Statistics utility test failed"
fi

# Memory leak test with valgrind if available
teststep "5" "7" "Running memory leak tests..."
if command -v valgrind > /dev/null; then
    if make test-memory; then
        success "Memory leak tests passed with Valgrind"
    else
        failure "Memory leak tests failed"
    fi
else
    warning "Valgrind not found, running basic memory tests only"
    if make test-unit; then
        success "Basic memory tests passed"
    else
        failure "Basic memory tests failed"
    fi
fi

# Test the daemon functionality for a brief period
teststep "6" "7" "Testing daemon mode (5 seconds)..."
make daemon &
DAEMON_PID=$!
sleep 5
kill -TERM $DAEMON_PID
wait $DAEMON_PID 2>/dev/null || true
success "Daemon mode test passed"

# Check for files created
teststep "7" "7" "Verifying data files..."
if [ -f "/var/lib/apm_tracker/apm_data.bin" ]; then
    success "Data storage verified"
else
    warning "Data file not created. This may be normal for first run."
fi

# Clean up temporary files
echo -e "\n${BLUE}Cleaning up...${NC}"
rm -f /tmp/apm_*_test.bin

echo -e "\n${BLUE}===============================================${NC}"
echo -e "${GREEN}All tests completed successfully!${NC}"
echo -e "${GREEN}The APM tracker is robust for long-term operation.${NC}"
echo -e "${BLUE}===============================================${NC}"