set -e
if [ -z "$FP_SDK_PATH" ]; then
  echo "Error: FP_SDK_PATH is not defined. Please set it before running this script."
else
    ${REACTOR_UC_PATH}/lfc/bin/lfc-dev src/Smoke.lf
    cmake -Bbuild
    make -C build
    bin/fp-smoke
fi