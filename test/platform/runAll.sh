#!/bin/bash

set -e

# Iterate over each folder and execute the command
for dir in ./*; do
    if [ -d $dir ]; then
        echo "Entering $dir"
            pushd $dir
            ./runAll.sh
            popd
    fi
done
