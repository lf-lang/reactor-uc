
#!/bin/bash
# Build script for S4NOC Federated 2-Federate Example

# Source shared build helpers
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
. "$SCRIPT_DIR/../build-helpers.sh"

LF_MAIN=s4noc_fed
BIN_DIR=bin

# Parse command line arguments
parse_build_args "$@"

# Clean and build
clean_federates sender receiver
cleanup_intermediates
build_federates sender receiver

echo "Linking sender and receiver to create executable"
build_and_link

# Interactive execution
run_interactive_menu "$BIN_DIR" "$LF_MAIN.elf"

