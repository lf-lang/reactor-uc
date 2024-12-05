#!/usr/bin/env zsh

# Get the directory of the current script
script_dir=$(dirname "$0:A")

export REACTOR_UC_PATH=$script_dir
alias lfcg='${REACTOR_UC_PATH}/lfc/bin/lfc-dev'