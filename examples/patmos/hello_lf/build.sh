#!/bin/bash

rm -rf bin src-gen
make all
pasim ./bin/HelloLF