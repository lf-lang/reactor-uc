package org.lflang.generator.uc

import org.lflang.allTimers
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.generator.uc.UcReactorGenerator.Companion.getEffects
import org.lflang.generator.uc.UcReactorGenerator.Companion.getObservers
import org.lflang.lf.Reactor
import org.lflang.lf.Timer

class UcTimerGenerator(private val reactor: Reactor) {
    private fun generateSelfStructs(timer: Timer) = "LF_DEFINE_TIMER_STRUCT(${reactor.codeType}, ${timer.name}, ${reactor.getEffects(timer).size}, ${reactor.getObservers(timer).size});"
    private fun generateCtor(timer: Timer) = "LF_DEFINE_TIMER_CTOR(${reactor.codeType}, ${timer.name}, ${reactor.getEffects(timer).size}, ${reactor.getObservers(timer).size});"

    fun generateCtors() = reactor.allTimers.joinToString(
    separator = "\n",
    prefix = "// Timer constructors \n",
    postfix = "\n"
    ) { generateCtor(it) };

    fun generateSelfStructs() =
        reactor.allTimers.joinToString(
            separator = "\n",
            prefix = "// Timer structs \n",
            postfix = "\n"
        ) { generateSelfStructs(it) };

    fun generateReactorStructFields() =
        reactor.allTimers.joinToString(separator = "\n", prefix = "// Timers\n", postfix = "\n") { "LF_TIMER_INSTANCE(${reactor.codeType}, ${it.name});" }

    private fun generateReactorCtorCode(timer: Timer) = "LF_INITIALIZE_TIMER(${reactor.codeType}, ${timer.name}, ${timer.offset.toCCode()}, ${timer.period.orNever().toCCode()});"
    fun generateReactorCtorCodes() = reactor.allTimers.joinToString(separator = "\n", prefix = "// Initialize Timers\n") { generateReactorCtorCode(it)}
}
