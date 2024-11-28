#include "reactor-uc/logging.h"
#include "reactor-uc/platform.h"

#include <stdarg.h>
#include <stdio.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

void log_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Platform_vprintf(fmt, args);
  va_end(args);
}

void log_message(int level, const char *module, const char *fmt, ...) {
  const char *level_str;
  switch (level) {
  case LF_LOG_LEVEL_ERR:
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

  va_list args;
  va_start(args, fmt);

#ifdef LF_COLORIZE_LOGS
  switch (level) {
  case LF_LOG_LEVEL_ERR:
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
  log_printf("[%s] [%s] ", level_str, module);
  Platform_vprintf(fmt, args);
#ifdef LF_COLORIZE_LOGS
  log_printf(ANSI_COLOR_RESET);
#endif
  log_printf("\n");
  va_end(args);
}