/*************
 * Copyright (c) 2021, TU Dresden.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.

 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************/

package org.lflang.generator.uc

import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.orZero
import org.lflang.isGeneric
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.lf.Timer
import org.lflang.priority

class UcTimerGenerator(private val reactor: Reactor) {
    companion object {
        // FIXME: Make nicer and also merge with the VarRef stuff in UcReactionGenerator
        val Timer.codeType
            get(): String = "${(eContainer() as Reactor).name}_Timer_${name}"
    }

    fun getEffects(timer: Timer) = reactor.reactions.filter {it.triggers.filter{ it.name== timer.name}.isNotEmpty()}

    fun generateSelfStructs(timer: Timer) = "DEFINE_TIMER_STRUCT(${timer.codeType}, ${getEffects(timer).size})"
    fun generateCtor(timer: Timer) = "DEFINE_TIMER_CTOR(${timer.codeType}, ${getEffects(timer).size})"

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
        reactor.timers.joinToString(separator = "\n", prefix = "// Timers\n", postfix = "\n") { "${it.codeType} ${it.name};" }

    fun generateReactorCtorCode(timer: Timer)  =  with(PrependOperator) {
        """
            |self->_triggers[trigger_idx++] = &self->${timer.name}.super.super;
            |${timer.codeType}_ctor(&self->${timer.name}, &self->super, ${timer.offset.toCCode()}, ${timer.period.orNever().toCCode()});
            |
            """.trimMargin()
    };
    fun generateReactorCtorCodes() = reactor.timers.joinToString(separator = "\n", prefix = "// Initialize Timers\n") { generateReactorCtorCode(it)}
}
