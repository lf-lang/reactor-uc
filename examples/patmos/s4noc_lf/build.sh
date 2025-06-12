#!/bin/bash

rm -rf bin src-gen
make all
patemu ./bin/s4noc