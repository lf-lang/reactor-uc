set -e
${REACTOR_UC_PATH}/lfc/bin/lfc-dev src/Smoke.lf
cmake -Bbuild
make -C build
bin/fp-smoke