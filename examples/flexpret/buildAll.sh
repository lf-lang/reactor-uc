set -e
if [ -z "$FP_SDK_PATH" ]; then
  echo "Error: FP_SDK_PATH is not defined. Please set it before running this script."
else
    ${REACTOR_UC_PATH}/ulf/bin/ulfc-dev src/Smoke.ulf
    cmake -Bbuild
    make -C build
    bin/fp-smoke
fi