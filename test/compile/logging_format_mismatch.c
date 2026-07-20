#include "reactor-uc/logging.h"

void logging_format_mismatch_probe(void) {
  log_message(LF_LOG_LEVEL_INFO, "TEST", "%d", "not an integer");
}
