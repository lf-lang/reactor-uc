/**
 * **********
 * Copyright (c) 2019-2021, TU Dresden.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *************
 */
package org.lflang.generator.uc
import org.lflang.ir.InputPort
import org.lflang.ir.Instantiation
import org.lflang.ir.OutputPort
import org.lflang.ir.Port
import org.lflang.ir.Reactor

class UcPortGenerator(
    private val reactor: Reactor,
    private val connections: UcConnectionGenerator
) {


  private fun generateSelfStruct(input: InputPort): String {
    if (input.dataType.isArray) {
      return "LF_DEFINE_INPUT_ARRAY_STRUCT(${reactor.codeType}, ${input.lfName}, ${input.getEffects().size}, ${input.getObservers().size}, ${input.dataType.targetCode}, ${input.dataType.arrayLength}, ${connections.getNumConnectionsFromPort(null, input as Port)});"
    } else {
      return "LF_DEFINE_INPUT_STRUCT(${reactor.codeType}, ${input.lfName}, ${input.getEffects().size}, ${input.getObservers().size}, ${input.dataType.targetCode}, ${connections.getNumConnectionsFromPort(null, input as Port)});"
    }
  }

  private fun generateInputCtor(input: InputPort) =
      "LF_DEFINE_INPUT_CTOR(${reactor.codeType}, ${input.lfName}, ${input.getEffects().size}, ${input.getObservers().size}, ${input.dataType.targetCode}, ${connections.getNumConnectionsFromPort(null, input as Port)});"

  private fun generateSelfStruct(output: OutputPort): String {
    if (output.dataType.isArray) {
      return "LF_DEFINE_OUTPUT_ARRAY_STRUCT(${reactor.codeType}, ${output.lfName}, ${output.getSources().size}, ${output.dataType.targetCode}, ${output.dataType.arrayLength});"
    } else {
      return "LF_DEFINE_OUTPUT_STRUCT(${reactor.codeType}, ${output.lfName}, ${output.getSources().size}, ${output.dataType.targetCode});"
    }
  }

  private fun generateOutputCtor(output: OutputPort) =
      "LF_DEFINE_OUTPUT_CTOR(${reactor.codeType}, ${output.lfName}, ${output.getSources().size});"

  fun generateSelfStructs() =
      reactor.inputs.plus(reactor.outputs).joinToString(
          prefix = "// Port structs\n", separator = "\n", postfix = "\n") {
            when (it) {
              is InputPort -> generateSelfStruct(it)
              is OutputPort -> generateSelfStruct(it)
              else -> ""
            }
          }

  fun generateReactorStructFields() =
      reactor.inputs.plus(reactor.outputs).joinToString(
          prefix = "// Ports \n", separator = "\n", postfix = "\n") {
            "LF_PORT_INSTANCE(${reactor.codeType}, ${it.lfName}, ${it.width});"
          }

  fun generateCtors() =
      reactor.inputs.plus(reactor.outputs).joinToString(
          prefix = "// Port constructors\n", separator = "\n", postfix = "\n") {
            when (it) {
              is InputPort -> generateInputCtor(it)
              is OutputPort -> generateOutputCtor(it)
              else -> throw IllegalArgumentException("Error: Port was neither input nor output")
            }
          }

  private fun generateReactorCtorCode(input: InputPort) =
      "LF_INITIALIZE_INPUT(${reactor.codeType}, ${input.lfName}, ${input.width}, ${input.externalArgs});"

  private fun generateReactorCtorCode(output: OutputPort) =
      "LF_INITIALIZE_OUTPUT(${reactor.codeType}, ${output.lfName}, ${output.width}, ${output.externalArgs});"

  private fun generateReactorCtorCode(port: Port) =
      when (port) {
        is InputPort -> generateReactorCtorCode(port)
        is OutputPort -> generateReactorCtorCode(port)
        else -> throw IllegalArgumentException("Error: Port was neither input nor output")
      }

  fun generateReactorCtorCodes() =
      reactor.inputs.plus(reactor.outputs).joinToString(
          prefix = "// Initialize ports\n", separator = "\n", postfix = "\n") {
            generateReactorCtorCode(it)
          }

  fun generateDefineContainedOutputArgs(r: Instantiation) =
      r.reactor.outputs.joinToString(separator = "\n", prefix = "\n", postfix = "\n") {
        "LF_DEFINE_CHILD_OUTPUT_ARGS(${r.name}, ${it.lfName}, ${r.codeWidth}, ${it.width});"
      }

  fun generateDefineContainedInputArgs(r: Instantiation) =
      r.reactor.inputs.joinToString(separator = "\n", prefix = "\n", postfix = "\n") {
        "LF_DEFINE_CHILD_INPUT_ARGS(${r.name}, ${it.lfName}, ${r.codeWidth}, ${it.width});"
      }

  fun generateReactorCtorDefArguments() =
      reactor.outputs.joinToString(separator = "") {
        ", OutputExternalCtorArgs *${it.externalArgs}"
      } +
          reactor.inputs.joinToString(separator = "") {
            ", InputExternalCtorArgs *${it.externalArgs}"
          }

  fun generateReactorCtorDeclArguments(r: Instantiation) =
      r.reactor.outputs.plus(r.reactor.inputs).joinToString(separator = "") {
        ", _${r.name}_${it.lfName}_args[i]"
      }
}
