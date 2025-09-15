package org.lflang.generator.uc

import org.lflang.ir.Reactor
import org.lflang.ir.Timer

class UcTimerGenerator(private val reactor: Reactor) {
  private fun generateSelfStructs(timer: Timer) =
      "LF_DEFINE_TIMER_STRUCT(${reactor.codeType}, ${timer.lfName}, ${timer.getEffects().size}, ${timer.getObservers().size});"

  private fun generateCtor(timer: Timer) =
      "LF_DEFINE_TIMER_CTOR(${reactor.codeType}, ${timer.lfName}, ${timer.getEffects().size}, ${timer.getObservers().size});"

  fun generateCtors() =
      reactor.timers.joinToString(
          separator = "\n", prefix = "// Timer constructors \n", postfix = "\n") {
            generateCtor(it)
          }

  fun generateSelfStructs() =
      reactor.timers.joinToString(
          separator = "\n", prefix = "// Timer structs \n", postfix = "\n") {
            generateSelfStructs(it)
          }

  fun generateReactorStructFields() =
      reactor.timers.joinToString(separator = "\n", prefix = "// Timers\n", postfix = "\n") {
        "LF_TIMER_INSTANCE(${reactor.codeType}, ${it.lfName});"
      }

  private fun generateReactorCtorCode(timer: Timer) =
      "LF_INITIALIZE_TIMER(${reactor.codeType}, ${timer.lfName}, ${timer.offset.toCCode()}, ${timer.period.toCCode()});"

  fun generateReactorCtorCodes() =
      reactor.timers.joinToString(separator = "\n", prefix = "// Initialize Timers\n") {
        generateReactorCtorCode(it)
      }
}
