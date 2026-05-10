#!/bin/bash

set -e
if [ -z "$REACTOR_UC_PATH" ]; then
  echo "Error: REACTOR_UC_PATH is not set." >&2
  exit 1
fi
while getopts ":s" opt; do 
  case $opt in
    s) SELF_HOSTED=true;;
    :) echo "Option -$OPTARG requires an argument." >&2; exit 1;;
    \?) echo "Invalid option -$OPTARG" >&2; exit 1;;
  esac
done

if ! command -v pasim &> /dev/null; then
  echo "Error: pasim command not found. Please ensure it is installed and available in your PATH."
else
    # Iterate over each folder and execute the command
    for dir in ./*; do
      if [ -d "$dir" ]; then
      echo "Entering $dir"
      pushd "$dir"
      chmod +x build.sh
      if [ "$SELF_HOSTED" = true ]; then
        echo "Running build.sh for self-hosted runner"
        ./build.sh -f
      else
        echo "Running build.sh for non-self-hosted runner"
        if [ "$dir" = "./s4noc_fed" ]; then
          echo "Skipping $dir for non-self-hosted runner"
          popd
          continue
        fi
        ./build.sh -e
      fi
      popd
      fi
    done
fi