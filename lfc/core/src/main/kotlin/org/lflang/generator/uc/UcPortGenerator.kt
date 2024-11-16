/*************
 * Copyright (c) 2019-2021, TU Dresden.

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

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

class UcPortGenerator(private val reactor: Reactor, private val connections: UcConnectionGenerator) {
    val Output.connsPtr
        get(): String = "_conns_${this.name}"
    val Output.connsPtrSize
        get(): String = "${connsPtr}_size"
    val Output.parentEffectsPtr
        get(): String = "_effects_${this.name}"
    val Output.parentEffectsPtrSize
        get(): String = "${parentEffectsPtr}_size"
    val Input.parentSourcesPtr
        get(): String = "_sources_${this.name}"
    val Input.parentSourcesPtrSize
        get(): String = "${parentSourcesPtr}_size"

    fun containeOutputConnectionField(inst: Instantiation, out: Output) = "_conns_${inst.name}_${out.name}"
    fun containedOutputEffectsField(inst: Instantiation, out: Output) = "_effects_${inst.name}_${out.name}"
    fun containedInputSourcesFields(inst: Instantiation, inp: Input) = "_sources_${inst.name}_${inp.name}"

    fun getEffects(port: Input) = reactor.reactions.filter { it.triggers.filter { it.name == port.name }.isNotEmpty() }
    fun getSources(port: Output) = reactor.reactions.filter { it.effects.filter { it.name == port.name }.isNotEmpty() }

    fun generateSelfStruct(input: Input) = "DEFINE_INPUT_STRUCT(${reactor.codeType}, ${input.name}, ${getEffects(input).size}, ${input.type.toText()}, ${connections.getNumConnectionsFromPort(input as Port)});"
    fun generateInputCtor(input: Input) = "DEFINE_INPUT_CTOR(${reactor.codeType}, ${input.name}, ${getEffects(input).size}, ${input.type.toText()}, ${connections.getNumConnectionsFromPort(input as Port)});"
    fun generateSelfStruct(output: Output) = "DEFINE_OUTPUT_STRUCT(${reactor.codeType}, ${output.name}, ${getSources(output).size}, ${output.type.toText()});"
    fun generateOutputCtor(output: Output) = "DEFINE_OUTPUT_CTOR(${reactor.codeType}, ${output.name}, ${getSources(output).size});"

    fun generateSelfStructs() = reactor.inputs.plus(reactor.outputs).joinToString(prefix = "// Port structs\n", separator = "\n", postfix = "\n") {
        when (it) {
            is Input  -> generateSelfStruct(it)
            is Output -> generateSelfStruct(it)
            else      -> ""
        }
    }

    fun generateReactorStructFields() = reactor.inputs.plus(reactor.outputs).joinToString(prefix = "// Ports \n", separator = "\n", postfix = "\n") {
            "PORT_INSTANCE(${reactor.codeType}, ${it.name});"
        }

    fun generateCtors() = reactor.inputs.plus(reactor.outputs).joinToString(prefix = "// Port constructors\n", separator = "\n", postfix = "\n") {
        when (it) {
            is Input  -> generateInputCtor(it)
            is Output -> generateOutputCtor(it)
            else -> "" // FIXME: Runtime exception
        }
    }

    fun generateReactorCtorCode(input: Input) = "INITIALIZE_INPUT(${reactor.codeType}, ${input.name}, ${input.parentSourcesPtr}, ${input.parentSourcesPtrSize});"
    fun generateReactorCtorCode(output: Output) = "INITIALIZE_OUTPUT(${reactor.codeType}, ${output.name}, ${output.connsPtr}, ${output.connsPtrSize}, ${output.parentEffectsPtr}, ${output.parentEffectsPtrSize});"

    fun generateReactorCtorCode(port: Port) =
        when(port) {
            is Input -> generateReactorCtorCode(port)
            is Output -> generateReactorCtorCode(port)
            else -> "" // FIXME: Runtime exception
        }

    fun generateReactorCtorCodes() = reactor.inputs.plus(reactor.outputs).joinToString(prefix = "// Initialize ports\n", separator = "\n", postfix = "\n") { generateReactorCtorCode(it)}

    fun generateReactorCtorDefArguments() =
        reactor.outputs.joinToString() {", Connection **${it.connsPtr}, size_t ${it.connsPtrSize}, Reaction **${it.parentEffectsPtr}, size_t ${it.parentEffectsPtrSize}"} +
        reactor.inputs.joinToString() {", Connection **${it.parentSourcesPtr}, size_t ${it.parentSourcesPtrSize}"}

    fun generateReactorCtorDeclArguments(r: Instantiation) =
        r.reactor.outputs.joinToString() { with(PrependOperator) {
            """|,
               |self->${containeOutputConnectionField(r, it)}, 
               |sizeof(self->${containeOutputConnectionField(r,it)})/sizeof(self->${containeOutputConnectionField(r,it)}[0]),
               |self->${containedOutputEffectsField(r, it)},
               |sizeof(self->${containedOutputEffectsField(r,it)})/sizeof(self->${containedOutputEffectsField(r,it)}[0])
        """.trimMargin()
        }} +
                r.reactor.inputs.joinToString() { with(PrependOperator) {
                    """
                |,
                |self->${containedInputSourcesFields(r, it)},
                |sizeof(self->${containedInputSourcesFields(r,it)})/sizeof(self->${containedInputSourcesFields(r,it)}[0])
            """.trimMargin()
                }}
}

