#!/bin/bash

set -e

PROFILE="local"

print_usage() {
  cat <<'EOF'
Usage: ./buildAll.sh [-s | -g]

Profiles:
  default (local)   Use FPGA mode (-f) for local runs.
  -s                GitHub self-hosted runner profile (uses -f).
  -g                GitHub non-self-hosted runner profile (uses -e, skips s4noc_fed).

Options:
  -h                Show this help message.
EOF
}

while getopts ":swgh" opt; do
  case $opt in
    s) PROFILE="github-self-hosted";;
    g) PROFILE="github-non-self-hosted";;
    h) print_usage; exit 0;;
    :) echo "Option -$OPTARG requires an argument." >&2; exit 1;;
    \?) echo "Invalid option -$OPTARG" >&2; exit 1;;
  esac
done

if [ -z "$REACTOR_UC_PATH" ]; then
  echo "Error: REACTOR_UC_PATH is not set." >&2
  exit 1
elif ! command -v pasim &> /dev/null; then
  echo "Error: pasim command not found. Please ensure it is installed and available in your PATH."
  exit 1
else
  # Iterate over each folder and execute the command
  for dir in ./*; do
    if [ -d "$dir" ]; then
      echo "Entering $dir"
      pushd "$dir"
      chmod +x build.sh

      case "$PROFILE" in
        local)
          echo "Running build.sh for local profile (FPGA mode)"
          ./build.sh
          ;;
        github-self-hosted)
          echo "Running build.sh for GitHub self-hosted profile (FPGA mode)"
        export COM_PORT=/dev/ttyS0
          ./build.sh -f
          ;;
        github-non-self-hosted)
          echo "Running build.sh for GitHub non-self-hosted profile (emulation mode)"
          if [ "$dir" = "./s4noc_fed" || "$dir" = "./s4noc_fed_3" ]; then // Skip long applications in non-self-hosted profile
            echo "Skipping $dir for GitHub non-self-hosted profile"
            popd
            continue
          fi
          ./build.sh -e
          ;;
        *)
          echo "Unknown profile: $PROFILE" >&2
          popd
          exit 1
          ;;
      esac

      popd
    fi
  done
fi
