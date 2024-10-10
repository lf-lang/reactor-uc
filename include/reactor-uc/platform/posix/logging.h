#ifndef LF_PLATFORM_POSIX_LOGGING_H
#define LF_PLATFORM_POSIX_LOGGING_H

#include <syslog.h>

#define LF_ERR(...) syslog(LOG_ERR, __VA_ARGS__)
#define LF_WARN(...) syslog(LOG_WARNING, __VA_ARGS__)
#define LF_INFO(...) syslog(LOG_INFO, __VA_ARGS__)
#define LF_DEBUG(...) syslog(LOG_DEBUG, __VA_ARGS__)

#endif