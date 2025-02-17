#ifndef REACTOR_UC_LOGGING_H
#define REACTOR_UC_LOGGING_H
#include <sys/types.h>
#include <inttypes.h>

// The different verbosity levels supported
#define LF_LOG_LEVEL_OFF 0
#define LF_LOG_LEVEL_ERROR 1
#define LF_LOG_LEVEL_WARN 2
#define LF_LOG_LEVEL_INFO 3
#define LF_LOG_LEVEL_LOG 4
#define LF_LOG_LEVEL_DEBUG 5

// Add color codes to the output
#ifndef LF_COLORIZE_LOGS
#define LF_COLORIZE_LOGS 1
#endif

// Add timestamp to the logs
#if !defined(LF_TIMESTAMP_LOGS) && !defined(PLATFORM_FLEXPRET)
#define LF_TIMESTAMP_LOGS 1
#else
#undef LF_TIMESTAMP_LOGS
#define LF_TIMESTAMP_LOGS 0
#endif

// The default log level for any unspecified module
#ifndef LF_LOG_LEVEL_ALL
#ifndef NDEBUG
#define LF_LOG_LEVEL_ALL LF_LOG_LEVEL_DEBUG
#else
#define LF_LOG_LEVEL_ALL LF_LOG_LEVEL_ERROR
#endif
#endif

// Define the log level for each module. If not defined, use LF_LOG_LEVEL_ALL
// or set to LF_LOG_LEVEL_ERROR if LF_LOG_LEVEL_ALL is not defined.
#ifndef LF_LOG_LEVEL_ENV
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_ENV LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_ENV LF_LOG_LEVEL_ERROR
#endif
#endif

#ifndef LF_LOG_LEVEL_SCHED
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_SCHED LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_SCHED LF_LOG_LEVEL_ERROR
#endif
#endif

#ifndef LF_LOG_LEVEL_QUEUE
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_QUEUE LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_QUEUE LF_LOG_LEVEL_ERROR
#endif
#endif

#ifndef LF_LOG_LEVEL_FED
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_FED LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_FED LF_LOG_LEVEL_ERROR
#endif
#endif

#ifndef LF_LOG_LEVEL_TRIG
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_TRIG LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_TRIG LF_LOG_LEVEL_ERROR
#endif
#endif

#ifndef LF_LOG_LEVEL_PLATFORM
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_PLATFORM LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_PLATFORM LF_LOG_LEVEL_ERROR
#endif
#endif

#ifndef LF_LOG_LEVEL_CONN
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_CONN LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_CONN LF_LOG_LEVEL_ERROR
#endif
#endif

#ifndef LF_LOG_LEVEL_NET
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_NET LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_NET LF_LOG_LEVEL_ERROR
#endif
#endif

// The user can disable all logging by defining LF_LOG_DISABLE
#if defined(LF_LOG_DISABLE)
#define LF_LOG(level, module, fmt, ...)
#define LF_ERR(module, fmt, ...)
#define LF_WARN(module, fmt, ...)
#define LF_INFO(module, fmt, ...)
#define LF_DEBUG(module, fmt, ...)
#else
// Each call to LF_LOG is expanded to a this piece of code where the verbosity
// level of the message is compared to the log level of the module. This adds
// some overhead even if verbosity is turned down.
#define LF_LOG(level, module, fmt, ...)                                                                                \
  do {                                                                                                                 \
    if (level <= LF_LOG_LEVEL_##module) {                                                                              \
      log_message(level, #module, fmt, ##__VA_ARGS__);                                                                 \
    }                                                                                                                  \
  } while (0)

#define LF_ERR(module, fmt, ...) LF_LOG(LF_LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__)
#define LF_WARN(module, fmt, ...) LF_LOG(LF_LOG_LEVEL_WARN, module, fmt, ##__VA_ARGS__)
#define LF_INFO(module, fmt, ...) LF_LOG(LF_LOG_LEVEL_INFO, module, fmt, ##__VA_ARGS__)
#define LF_DEBUG(module, fmt, ...) LF_LOG(LF_LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__)

void log_message(int level, const char *module, const char *fmt, ...);
#endif

#endif
