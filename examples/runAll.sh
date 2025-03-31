#!/bin/bash

set -e

# Iterate over each folder and execute the command
for dir in ./*; do
    if [ -d "$dir" ] && [ -f "$dir/buildAll.sh" ]; then
        echo "Entering $dir"
        pushd "$dir"
        ./buildAll.sh
        popd
    fi
done
