#include "reactor-uc/logging.h"

#include <stdarg.h>
#include <stdio.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

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
    printf(ANSI_COLOR_RED);
    break;
  case LF_LOG_LEVEL_WARN:
    printf(ANSI_COLOR_YELLOW);
    break;
  case LF_LOG_LEVEL_INFO:
    printf(ANSI_COLOR_GREEN);
    break;
  case LF_LOG_LEVEL_DEBUG:
    printf(ANSI_COLOR_BLUE);
    break;
  default:
    break;
  }
#endif

  printf("[%s] [%s] ", level_str, module);
  vprintf(fmt, args);
  printf("\n");
#ifdef LF_COLORIZE_LOGS
  printf(ANSI_COLOR_RESET);
#endif
  va_end(args);
}