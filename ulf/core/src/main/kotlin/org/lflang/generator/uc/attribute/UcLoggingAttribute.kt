package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

data object UcLoggingLevel {
  /**
   * Silence, mapping to reactor-uc's own `LF_LOG_LEVEL_OFF`. Nothing above ERROR is left to
   * turn down, so this is the only level that suppresses a runtime diagnostic entirely --
   * needed where a program's expected stdout is compared byte for byte and the runtime
   * legitimately reports something whose text is not reproducible (a pointer, a physical
   * timestamp). `ulf/test/modal/cases/stop-tag-entry` is the one such case.
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
