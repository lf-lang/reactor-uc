#include "reactor-uc/logging.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/environment.h"

#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void log_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  pthread_mutex_lock(&log_lock);
  Platform_vprintf(fmt, args);
  pthread_mutex_unlock(&log_lock);

  va_end(args);
}

void log_message(int level, const char *module, const char *fmt, ...) {
  const char *level_str;
  switch (level) {
  case LF_LOG_LEVEL_ERROR:
    level_str = "ERROR";
    break;
  case LF_LOG_LEVEL_WARN:
    level_str = "WARN";
    break;
  case LF_LOG_LEVEL_INFO:
    level_str = "INFO";
    break;
  case LF_LOG_LEVEL_DEBUG:
    level_str = "DEBUG";
    break;
  default:
    level_str = "UNKNOWN";
    break;
  }

  pthread_mutex_lock(&log_lock);

  va_list args;
  va_start(args, fmt);

#if LF_COLORIZE_LOGS == 1
  switch (level) {
  case LF_LOG_LEVEL_ERROR:
    log_printf(ANSI_COLOR_RED);
    break;
  case LF_LOG_LEVEL_WARN:
    log_printf(ANSI_COLOR_YELLOW);
    break;
  case LF_LOG_LEVEL_INFO:
    log_printf(ANSI_COLOR_GREEN);
    break;
  case LF_LOG_LEVEL_DEBUG:
    log_printf(ANSI_COLOR_BLUE);
    break;
  default:
    break;
  }
#endif

#if LF_TIMESTAMP_LOGS == 1
  instant_t timestamp = 0;
  if (_lf_environment) {
    timestamp = _lf_environment->platform->get_physical_time(_lf_environment->platform);
  }
#if defined(PLATFORM_PATMOS)
  log_printf("(" PRINTF_TIME ") [%s] [%s] [%d] ", timestamp, level_str, module, get_cpuid());
#else
  log_printf("(" PRINTF_TIME ") [%s] [%s] ", timestamp, level_str, module);
#endif
#else

  log_printf("[%s] [%s] ", level_str, module);
#endif

  Platform_vprintf(fmt, args);
#if LF_COLORIZE_LOGS == 1
  log_printf(ANSI_COLOR_RESET);
#endif

#if defined LF_LOG_CARRIAGE_RETURN
  log_printf("\r\n");
#else
  log_printf("\n");
#endif

  pthread_mutex_unlock(&log_lock);

  va_end(args);
}
