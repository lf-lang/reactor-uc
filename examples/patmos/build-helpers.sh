#!/bin/bash
# Shared build helpers for S4NOC federated examples
# Source this file in your build.sh to use these functions

# Build federate components
# Usage: build_federates <component1> <component2> ...
build_federates() {
  for component in "$@"; do
    echo "Building $component federate..."
    make -C "$component" || exit 1
  done
}

# Clean federate components
# Usage: clean_federates <component1> <component2> ...
clean_federates() {
  for component in "$@"; do
    echo "Cleaning $component..."
    make clean -C "$component" 2>/dev/null || true
  done
}

# Standard cleanup for reactor-uc intermediate files
cleanup_intermediates() {
  # Compiler cannot detect changes inside the equivalent folders, so we need to remove them to force regeneration
  echo "Cleaning intermediate reactor-uc files..."
  if [ -n "$REACTOR_UC_PATH" ]; then
    rm -f "$REACTOR_UC_PATH"/src/scheduler.bc \
          "$REACTOR_UC_PATH"/src/platform.bc \
          "$REACTOR_UC_PATH"/src/network_channel.bc \
          "$REACTOR_UC_PATH"/src/environment.bc
  fi
}

# Full build and link
# Usage: build_and_link
build_and_link() {
  echo "Building main executable..."
  make all || exit 1
}

# Detect execution environment (Patmos-specific)
# Sets DEF_TOOL to "e" (emulate) or "f" (fpga)
detect_execution_tool() {
  if jtagconfig 2>/dev/null | grep -q "USB-Blaster"; then
    DEF_TOOL="f"
    echo "USB-Blaster detected - FPGA programming available"
  else
    DEF_TOOL="e"
    echo "USB-Blaster not detected - emulation mode"
  fi
}

# Parse common build arguments
# Usage: parse_build_args "$@"
# Sets DEF_TOOL to "e" or "f" based on command line flags
parse_build_args() {
  local usage_msg="Usage: ${0##*/} [-e] [-f] [-h] [-d]
  -e    Set default action to emulate
  -f    Set default action to FPGA
  -h    Show this help message
  -d    Delete all .bc files in REACTOR_UC_PATH/src"

  detect_execution_tool

  while getopts ":efhd" opt; do 
    case $opt in
      f) DEF_TOOL=f;;
      e) DEF_TOOL=e;;
      h) echo "$usage_msg"; exit 0;;
      d) rm -f "$REACTOR_UC_PATH"/src/*.bc;;
      :) echo "Option -$OPTARG requires an argument." >&2; exit 1;;
      \?) echo "Invalid option -$OPTARG" >&2; exit 1;;
    esac
  done
}


# Interactive execution menu for Patmos examples
# Usage: run_interactive_menu <binary_dir> <binary_name>
run_interactive_menu() {
  local bin_dir="${1:-.}"
  local bin_name="${2:-executable.elf}"

  read -n 1 -t 5 -p "Choose action: [e]mulate or [f]pga? (default: $DEF_TOOL) " action
  action=${action:-$DEF_TOOL}
  
  if [[ "$action" == "e" ]]; then
    echo ""
    echo "Running emulator..."
    patemu "$bin_dir/$bin_name"
  elif [[ "$action" == "f" ]]; then
    echo ""
    echo "Preparing FPGA programming..."
    run_fpga_programming "$bin_dir" "$bin_name"
  else
    echo ""
    echo "Invalid action: $action"
    exit 1
  fi
}

# FPGA programming with retry logic
# Usage: run_fpga_programming <binary_dir> <binary_name>
run_fpga_programming() {
  local bin_dir="${1:-.}"
  local bin_name="${2:-executable.elf}"
  local bin_path="$bin_dir/$bin_name"
  local t_crest_path="$HOME/t-crest/patmos/tmp"
  
  rm -f "$t_crest_path/$bin_name"
  mv "$bin_path" "$t_crest_path/$bin_name"
  
  local RETRIES=5
  local DELAY=10
  local attempt=0
  
  echo "Attempting FPGA programming (max $RETRIES attempts, $DELAY sec delay)..."
  
  while [ $attempt -lt $RETRIES ]; do
    ((attempt++))
    echo "Attempt $attempt/$RETRIES..."
    app_name="${bin_name%.elf}"
    make -C "$HOME/t-crest/patmos" APP="$app_name" config download && {
      echo "FPGA programming successful!"
      return 0
    }
    [ $attempt -lt $RETRIES ] && sleep $DELAY
  done
  
  echo "FPGA programming failed after $RETRIES attempts"
  return 1
}
