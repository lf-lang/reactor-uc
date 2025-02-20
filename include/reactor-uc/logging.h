#ifndef REACTOR_UC_LOGGING_H
#define REACTOR_UC_LOGGING_H
#include <sys/types.h>
#include <inttypes.h>

// The log levels
/** Logging is disabled.*/
#define LF_LOG_LEVEL_OFF 0
/** Only log error messages.*/
#define LF_LOG_LEVEL_ERROR 1
/** Log warnings and above.*/
#define LF_LOG_LEVEL_WARN 2
/** Log info and above. This is the default level.*/
#define LF_LOG_LEVEL_INFO 3
/** Log messages and above..*/
#define LF_LOG_LEVEL_LOG 4
/** All log messages enabled.*/
#define LF_LOG_LEVEL_DEBUG 5

/** Enable colorizing the logging. */
#ifndef LF_COLORIZE_LOGS
#define LF_COLORIZE_LOGS 1
#endif

/** Add timestamps to each log entry. */
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

/** The log level of the ENV module.*/
#ifndef LF_LOG_LEVEL_ENV
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_ENV LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_ENV LF_LOG_LEVEL_ERROR
#endif
#endif

/** The log level of the scheduler.*/
#ifndef LF_LOG_LEVEL_SCHED
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_SCHED LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_SCHED LF_LOG_LEVEL_ERROR
#endif
#endif

/** The log level of the reaction and event queues.*/
#ifndef LF_LOG_LEVEL_QUEUE
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_QUEUE LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_QUEUE LF_LOG_LEVEL_ERROR
#endif
#endif

/** The log level of the federated infrastructure.*/
#ifndef LF_LOG_LEVEL_FED
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_FED LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_FED LF_LOG_LEVEL_ERROR
#endif
#endif

/** The log level of the triggers.*/
#ifndef LF_LOG_LEVEL_TRIG
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_TRIG LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_TRIG LF_LOG_LEVEL_ERROR
#endif
#endif

/** The log level of the platform implementations.*/
#ifndef LF_LOG_LEVEL_PLATFORM
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_PLATFORM LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_PLATFORM LF_LOG_LEVEL_ERROR
#endif
#endif

/** The log level of the LF connections.*/
#ifndef LF_LOG_LEVEL_CONN
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_CONN LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_CONN LF_LOG_LEVEL_ERROR
#endif
#endif

/** The log level of the network channels.*/
#ifndef LF_LOG_LEVEL_NET
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_NET LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_NET LF_LOG_LEVEL_ERROR
#endif
#endif

/** The log level of the clock synchronization.*/
#ifndef LF_LOG_LEVEL_CLOCK_SYNC
#ifdef LF_LOG_LEVEL_ALL
#define LF_LOG_LEVEL_CLOCK_SYNC LF_LOG_LEVEL_ALL
#else
#define LF_LOG_LEVEL_CLOCK_SYNC LF_LOG_LEVEL_ERROR
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

/**
 * @brief Output an error message for the specified module.
 * @param module The name of the module.
 * @param fmt The format string.
 */
#define LF_ERR(module, fmt, ...) LF_LOG(LF_LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__)

/**
 * @brief Output an error message for the specified module.
 * @param module The name of the module.
 * @param fmt The format string.
 */
#define LF_WARN(module, fmt, ...) LF_LOG(LF_LOG_LEVEL_WARN, module, fmt, ##__VA_ARGS__)

/**
 * @brief Output an error message for the specified module.
 * @param module The name of the module.
 * @param fmt The format string.
 */
#define LF_INFO(module, fmt, ...) LF_LOG(LF_LOG_LEVEL_INFO, module, fmt, ##__VA_ARGS__)

/**
 * @brief Output an error message for the specified module.
 * @param module The name of the module.
 * @param fmt The format string.
 */
#define LF_DEBUG(module, fmt, ...) LF_LOG(LF_LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__)

void log_message(int level, const char *module, const char *fmt, ...);
#endif

#endif
