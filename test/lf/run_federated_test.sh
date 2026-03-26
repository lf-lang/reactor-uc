#!/usr/bin/env bash

set -m
shopt -s huponexit

cleanup() {
  if [ "${EXITED_SUCCESSFULLY}" ] ; then
    exit 0
  else
    printf "Killing federate(s): %s.\n" "${pids[*]}"
    kill "${pids[@]}" || true
    exit 1
  fi
}

trap 'cleanup; exit' EXIT

pids=()
fed_index=0

for federate in "$@"
do
  fed_index=$((fed_index + 1))
  echo "#### Launching federate ${fed_index}: ${federate}"
  "${federate}" &
  pids+=($!)
done

for pid in "${pids[@]}"
do
  wait "${pid}" || exit $?
done

EXITED_SUCCESSFULLY=true
