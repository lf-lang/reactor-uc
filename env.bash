#!/usr/bin/env bash
# From https://stackoverflow.com/questions/59895/how-do-i-get-the-directory-where-a-bash-script-is-located-from-within-the-script 

curdir=$(pwd)
export REACTOR_UC_PATH=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
alias lfuc='${REACTOR_UC_PATH}/lfuc/bin/lfuc-dev'
cd $curdir