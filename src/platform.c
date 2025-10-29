#ifdef PLATFORM_POSIX
#include "platform/posix/posix.c"
#elif defined(PLATFORM_RIOT)
#include "platform/riot/riot.c"
#elif defined(PLATFORM_ZEPHYR)
#include "platform/zephyr/zephyr.c"
#elif defined(PLATFORM_FLEXPRET)
#include "platform/flexpret/flexpret.c"
#elif defined(PLATFORM_PICO)
#include "platform/pico/pico.c"
#elif defined(PLATFORM_PATMOS)
#include "platform/patmos/patmos.c"
#elif defined(PLATFORM_ADUCM355)
#include "platform/aducm355/aducm355.c"
#else
#error "NO PLATFORM SPECIFIED"
#endif
