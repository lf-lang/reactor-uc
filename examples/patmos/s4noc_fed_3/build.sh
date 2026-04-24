#!/bin/bash
# Build script for S4NOC Federated 3-Core Example

START_TIME="$(date '+%Y-%m-%d %H:%M:%S %Z')"
echo "[build.sh] Start time: $START_TIME"

# Source shared build helpers
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
. "$SCRIPT_DIR/../build-helpers.sh"

# Parse command-line arguments
parse_build_args "$@"

# Clean existing builds
clean_federates sender repeater receiver
cleanup_intermediates

# Build federates
build_federates sender repeater receiver

# Build and link main executable
build_and_link

# Interactive execution menu
run_interactive_menu "bin" "s4noc_fed_3.elf"

END_TIME="$(date '+%Y-%m-%d %H:%M:%S %Z')"
echo "[build.sh] End time:   $END_TIME"