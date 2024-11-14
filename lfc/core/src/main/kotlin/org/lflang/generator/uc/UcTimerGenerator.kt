package org.lflang.generator.uc

import org.lflang.generator.orNever
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.Reactor
import org.lflang.lf.Timer

class UcTimerGenerator(private val reactor: Reactor) {
    fun getEffects(timer: Timer) = reactor.reactions.filter {it.triggers.filter{ it.name== timer.name}.isNotEmpty()}
    fun generateSelfStructs(timer: Timer) = "DEFINE_TIMER_STRUCT(${reactor.codeType}, ${timer.name}, ${getEffects(timer).size})"
    fun generateCtor(timer: Timer) = "DEFINE_TIMER_CTOR(${reactor.codeType}, ${timer.name}, ${getEffects(timer).size})"

    fun generateCtors() = reactor.timers.joinToString(
    separator = "\n",
    prefix = "// Timer constructors \n",
    postfix = "\n"
    ) { generateCtor(it) };

    fun generateSelfStructs() =
        reactor.timers.joinToString(
            separator = "\n",
            prefix = "// Timer structs \n",
            postfix = "\n"
        ) { generateSelfStructs(it) };

    fun generateReactorStructFields() =
        reactor.timers.joinToString(separator = "\n", prefix = "// Timers\n", postfix = "\n") { "TIMER_INSTANCE(${reactor.codeType}, ${it.name});" }

    fun generateReactorCtorCode(timer: Timer) = "INITIALIZE_TIMER(${reactor.codeType}, ${timer.name}, ${timer.offset.toCCode()}, ${timer.period.orNever().toCCode()});"
    fun generateReactorCtorCodes() = reactor.timers.joinToString(separator = "\n", prefix = "// Initialize Timers\n") { generateReactorCtorCode(it)}
}
