#include "reactor-uc/logging.h"
#include "reactor-uc/tag.h"

void logging_format_valid_probe(tag_t tag) {
  log_message(LF_LOG_LEVEL_INFO, "TEST", "tag=" PRINTF_TAG, tag.time, tag.microstep);
}
