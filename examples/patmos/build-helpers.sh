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
# Sets ACTION and options
parse_build_args() {
  ACTION=""
  DELETE_BC=false
  
  local usage_msg="Usage: ${0##*/} [-e] [-f] [-h] [-d]
  -e    Run in emulation mode
  -f    Run in FPGA mode
  -h    Show this help message
  -d    Delete all .bc files in REACTOR_UC_PATH/src first"

  while getopts ":efhd" opt; do 
    case $opt in
      f) ACTION="f";;
      e) ACTION="e";;
      h) echo "$usage_msg"; exit 0;;
      d) DELETE_BC=true;;
      :) echo "Option -$OPTARG requires an argument." >&2; exit 1;;
      \?) echo "Invalid option -$OPTARG" >&2; exit 1;;
    esac
  done

  if [ "$DELETE_BC" = true ]; then
    echo "Deleting all .bc files in $REACTOR_UC_PATH/src..."
    rm -f "$REACTOR_UC_PATH"/src/*.bc
    rm -f "$REACTOR_UC_PATH"/src/environments/*.bc
    rm -f "$REACTOR_UC_PATH"/src/platform/*.bc
    rm -f "$REACTOR_UC_PATH"/src/schedulers/*.bc
  fi

  detect_execution_tool
}


# Interactive execution menu for Patmos examples
# Usage: run_interactive_menu <binary_dir> <binary_name>
run_interactive_menu() {
  local bin_dir="${1:-.}"
  local bin_name="${2:-executable.elf}"

  local choice="$ACTION"
  if [ -z "$choice" ]; then
    read -n 1 -t 5 -p "Choose action: [e]mulate or [f]pga? (default: $DEF_TOOL) " choice
    choice=${choice:-$DEF_TOOL}
    echo ""
  fi
  
  if [[ "$choice" == "e" ]]; then
    echo "Running emulator..."
    patemu "$bin_dir/$bin_name"
  elif [[ "$choice" == "f" ]]; then
    echo "Preparing FPGA programming..."
    run_fpga_programming "$bin_dir" "$bin_name"
  else
    echo "Invalid action: $choice"
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

# ============================================================================
# Federated Build Helpers
# ============================================================================

# Check if federate scaffold exists, attempt generation if missing
# Usage: check_and_generate_federate_scaffold <lf_main> <num_federates>
check_and_generate_federate_scaffold() {
  local lf_main=$1
  local num_federates=${2:-2}
  local missing=false

  for i in $(seq 1 $num_federates); do
    if [ ! -d "$lf_main/r$i" ]; then
      missing=true
      break
    fi
  done

  if [ "$missing" = true ]; then
    echo "Federate scaffold missing, attempting template generation..."
    $REACTOR_UC_PATH/lfc/bin/lfc-dev --gen-fed-templates src/$lf_main.lf || { 
      echo "Error: failed to generate federate templates for $lf_main." >&2
      exit 1
    }
  fi

  missing=false
  for i in $(seq 1 $num_federates); do
    if [ ! -d "$lf_main/r$i" ]; then
      missing=true
      break
    fi
  done

  if [ "$missing" = true ]; then
    echo "Error: federate scaffold is unavailable for $lf_main." >&2
    exit 1
  fi
}

# Clean generated outputs in federate directories (but keep src-gen if FORCE_REGEN_SRC_GEN=0)
# Usage: cleanup_generated_federate_outputs <lf_main>
cleanup_generated_federate_outputs() {
  local lf_main=$1
  for fed_dir in "$lf_main"/r*; do
    if [ -d "$fed_dir" ]; then
      rm -rf "$fed_dir/bin" "$fed_dir/obj"
      # Only remove src-gen if FORCE_REGEN_SRC_GEN was used
      if [ "${FORCE_REGEN_SRC_GEN:-0}" = "1" ]; then
        rm -rf "$fed_dir/src-gen"
      fi
    fi
  done
}

# Build a single federate (first federate vs. others handled differently)
# Usage: build_single_federate <lf_main> <federate_num> <num_federates> [sed_script]
# The sed_script is optional and applied only for federates 2+
build_single_federate() {
  local lf_main=$1
  local federate_num=$2
  local num_federates=$3
  local sed_script_base=$4  # Optional base for sed replacement

  pushd ./$lf_main/r$federate_num > /dev/null
    echo "Building federate $lf_main/r$federate_num..."
    ./run_lfc.sh || { echo "Error: lfc failed for federate r$federate_num" >&2; exit 1; }

    if [ $federate_num -gt 1 ] && [ -n "$sed_script_base" ]; then
      local reactor_path=$(pwd)/src-gen/$lf_main/r$federate_num
      # Apply federate-specific transformations
      eval "$sed_script_base" || { echo "Error: sed transformations failed for r$federate_num" >&2; exit 1; }
    fi

    if [ $federate_num -eq 1 ]; then
      # r1: simple build
      make all || { echo "Error: failed to build federate r1" >&2; exit 1; }
    else
      # r2+: build with federate-specific objects
      local reactor_path=$(pwd)/src-gen/$lf_main/r$federate_num
      make all OBJECTS="$reactor_path/lf_federate.bc $reactor_path/$lf_main/Dst.bc $reactor_path/lf_start.bc" || { 
        echo "Error: failed to build federate r$federate_num" >&2
        exit 1
      }
    fi
  popd > /dev/null
}

# Collect archives from all federate builds into a single string
# Usage: A_FILES=$(collect_federate_archives <lf_main> <num_federates>)
collect_federate_archives() {
  local lf_main=$1
  local num_federates=${2:-2}
  local archives=""

  for i in $(seq 1 $num_federates); do
    archives="$archives ./$lf_main/r$i/bin/$lf_main.a"
  done

  echo "$archives"
}

# Link the main executable from collected archives
# Usage: link_main_executable <cc_compiler> <main_source> <archives_string> <output_binary>
link_main_executable() {
  local cc=$1
  local main_src=$2
  local archives=$3
  local output=$4

  echo "Linking main executable: $output"
  $cc -O2 -Wall -Wextra "$main_src" $archives -o "$output" || {
    echo "Error: linking failed" >&2
    exit 1
  }
}
