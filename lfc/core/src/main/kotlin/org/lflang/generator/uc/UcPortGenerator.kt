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
import org.lflang.generator.uc.UcReactorGenerator.Companion.getEffects
import org.lflang.generator.uc.UcReactorGenerator.Companion.getObservers
import org.lflang.generator.uc.UcReactorGenerator.Companion.getSources
import org.lflang.lf.*

class UcPortGenerator(private val reactor: Reactor, private val connections: UcConnectionGenerator) {
    val Port.external_args
        get(): String = "_${name}_external"

    private fun generateSelfStruct(input: Input) = "DEFINE_INPUT_STRUCT(${reactor.codeType}, ${input.name}, ${reactor.getEffects(input).size}, ${reactor.getObservers(input).size}, ${input.type.toText()}, ${connections.getNumConnectionsFromPort(input as Port)});"
    private fun generateInputCtor(input: Input) = "DEFINE_INPUT_CTOR(${reactor.codeType}, ${input.name}, ${reactor.getEffects(input).size}, ${reactor.getObservers(input).size}, ${input.type.toText()}, ${connections.getNumConnectionsFromPort(input as Port)});"
    private fun generateSelfStruct(output: Output) = "DEFINE_OUTPUT_STRUCT(${reactor.codeType}, ${output.name}, ${reactor.getSources(output).size}, ${output.type.toText()});"
    private fun generateOutputCtor(output: Output) = "DEFINE_OUTPUT_CTOR(${reactor.codeType}, ${output.name}, ${reactor.getSources(output).size});"

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

    private fun generateReactorCtorCode(input: Input) = "INITIALIZE_INPUT(${reactor.codeType}, ${input.name}, ${input.external_args});"
    private fun generateReactorCtorCode(output: Output) = "INITIALIZE_OUTPUT(${reactor.codeType}, ${output.name}, ${output.external_args});"

    private fun generateReactorCtorCode(port: Port) =
        when(port) {
            is Input -> generateReactorCtorCode(port)
            is Output -> generateReactorCtorCode(port)
            else -> "" // FIXME: Runtime exception
        }

    fun generateReactorCtorCodes() = reactor.inputs.plus(reactor.outputs).joinToString(prefix = "// Initialize ports\n", separator = "\n", postfix = "\n") { generateReactorCtorCode(it)}

    fun generateDefineContainedOutputArgs(r: Instantiation) =
        r.reactor.outputs.joinToString(separator = "\n", prefix = "\n", postfix = "\n") {
            "DEFINE_CONTAINED_OUTPUT_ARGS(${r.name}, ${it.name});"
        }
    fun generateDefineContainedInputArgs(r: Instantiation) =
        r.reactor.inputs.joinToString(separator = "\n", prefix = "\n", postfix = "\n") {
            "DEFINE_CONTAINED_INPUT_ARGS(${r.name}, ${it.name});"
        }

    fun generateReactorCtorDefArguments() =
        reactor.outputs.joinToString(separator = "") {", OutputExternalCtorArgs ${it.external_args}"} +
        reactor.inputs.joinToString(separator = "") {", InputExternalCtorArgs ${it.external_args}"}

    fun generateReactorCtorDeclArguments(r: Instantiation) =
        r.reactor.outputs.plus(r.reactor.inputs).joinToString(separator = "") {", _${r.name}_${it.name}_args"}
}

