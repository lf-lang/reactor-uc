package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

data object UcLoggingLevel {
  /**
   * Silence, mapping to reactor-uc's `LF_LOG_LEVEL_OFF`. The only level that suppresses a runtime
   * diagnostic outright, which a program needs when it legitimately provokes one whose text is not
   * reproducible. See `test/lf/src/modal/ModalStopTagEntry.ulf`.
   */
  const val OFF = "OFF"
  const val ERROR = "ERROR"
  const val WARN = "WARN"
  const val INFO = "INFO"
  const val LOG = "LOG"
  const val DEBUG = "DEBUG"
}

class UcLoggingLevelAttribute(var level: String = UcLoggingLevel.INFO) {
  // Secondary constructor that initializes the data class from an Attribute object
  constructor(inst: Reactor) : this() {
    level = getLogLevelFromNode(inst)
  }

  fun getLogLevelFromNode(inst: Reactor): String {
    return when (AttributeUtils.getLoggingAttrValue(inst).uppercase()) {
      "OFF" -> UcLoggingLevel.OFF
      "ERROR" -> UcLoggingLevel.ERROR
      "WARN" -> UcLoggingLevel.WARN
      "INFO" -> UcLoggingLevel.INFO
      "LOG" -> UcLoggingLevel.LOG
      "DEBUG" -> UcLoggingLevel.DEBUG
      else -> UcLoggingLevel.INFO
    }
  }

  fun setLevel(inst: Reactor) {
    level = getLogLevelFromNode(inst)
  }
}
