#ifndef REACTOR_UC_LOGGING_H
#define REACTOR_UC_LOGGING_H

#define LF_LOG_LEVEL_OFF 0
#define LF_LOG_LEVEL_ERR 1
#define LF_LOG_LEVEL_WARN 2
#define LF_LOG_LEVEL_INFO 3
#define LF_LOG_LEVEL_DEBUG 4

#if defined(PLATFORM_POSIX)
#include "platform/posix/logging.h"
#elif defined(PLATFORM_RIOT)
#include "platform/riot/logging.h"
#elif defined(PLATFORM_ZEPHYR)
#include "platform/zephyr/logging.h"
#else
#error "NO PLATFORM SPECIFIED"
#endif

#if !defined(LF_LOG_LEVEL)
#error "LF_LOG_LEVEL must be defined"
#endif

#endif
