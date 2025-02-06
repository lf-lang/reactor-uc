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

import org.lflang.*
import org.lflang.ast.ASTUtils
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeWidth
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.generator.uc.UcReactorGenerator.Companion.getEffects
import org.lflang.generator.uc.UcReactorGenerator.Companion.getObservers
import org.lflang.generator.uc.UcReactorGenerator.Companion.getSources
import org.lflang.lf.*

class UcPortGenerator(
    private val reactor: Reactor,
    private val connections: UcConnectionGenerator
) {
  val Port.external_args
    get(): String = "_${name}_external"

    public companion object {
        val Port.width
            get(): Int = widthSpec?.getWidth()?:1
        val Port.maxWait
            get(): TimeValue {
                val parent = this.eContainer() as Reactor
                val effects = mutableListOf<Reaction>()
                for (r in parent.allReactions) {
                    if (r.sources.map{it.variable}.filterIsInstance<Port>().contains(this)) {
                        effects.add(r)
                        continue
                    }
                    if (r.triggers.filterIsInstance<VarRef>().map{it.variable}.filterIsInstance<Port>().contains(this)) {
                        effects.add(r)
                    }
                }

                var minMaxWait = TimeValue.MAX_VALUE
                for (e in effects) {
                    if (e.maxWait != null) {
                        val proposedMaxWait = ASTUtils.getLiteralTimeValue(e.maxWait.value!!)
                        if (proposedMaxWait < minMaxWait) {
                            minMaxWait = proposedMaxWait
                        }
                    } else {
                        minMaxWait = TimeValue.ZERO
                        break;
                    }
                }
                return minMaxWait
            }

    val Type.isArray
      get(): Boolean = cStyleArraySpec != null

    val Type.arrayLength
      get(): Int = cStyleArraySpec.length
  }

  private fun generateSelfStruct(input: Input): String {
    if (input.type.isArray) {
      return "LF_DEFINE_INPUT_ARRAY_STRUCT(${reactor.codeType}, ${input.name}, ${reactor.getEffects(input).size}, ${reactor.getObservers(input).size}, ${input.type.id}, ${input.type.arrayLength}, ${connections.getNumConnectionsFromPort(null, input as Port)});"
    } else {
      return "LF_DEFINE_INPUT_STRUCT(${reactor.codeType}, ${input.name}, ${reactor.getEffects(input).size}, ${reactor.getObservers(input).size}, ${input.type.toText()}, ${connections.getNumConnectionsFromPort(null, input as Port)});"
    }
  }

  private fun generateInputCtor(input: Input) =
      "LF_DEFINE_INPUT_CTOR(${reactor.codeType}, ${input.name}, ${reactor.getEffects(input).size}, ${reactor.getObservers(input).size}, ${input.type.toText()}, ${connections.getNumConnectionsFromPort(null, input as Port)});"

  private fun generateSelfStruct(output: Output): String {
    if (output.type.isArray) {
      return "LF_DEFINE_OUTPUT_ARRAY_STRUCT(${reactor.codeType}, ${output.name}, ${reactor.getSources(output).size}, ${output.type.id}, ${output.type.arrayLength});"
    } else {
      return "LF_DEFINE_OUTPUT_STRUCT(${reactor.codeType}, ${output.name}, ${reactor.getSources(output).size}, ${output.type.toText()});"
    }
  }

  private fun generateOutputCtor(output: Output) =
      "LF_DEFINE_OUTPUT_CTOR(${reactor.codeType}, ${output.name}, ${reactor.getSources(output).size});"

  fun generateSelfStructs() =
      reactor.allInputs.plus(reactor.allOutputs).joinToString(
          prefix = "// Port structs\n", separator = "\n", postfix = "\n") {
            when (it) {
              is Input -> generateSelfStruct(it)
              is Output -> generateSelfStruct(it)
              else -> ""
            }
          }

  fun generateReactorStructFields() =
      reactor.allInputs.plus(reactor.allOutputs).joinToString(
          prefix = "// Ports \n", separator = "\n", postfix = "\n") {
            "LF_PORT_INSTANCE(${reactor.codeType}, ${it.name}, ${it.width});"
          }

  fun generateCtors() =
      reactor.allInputs.plus(reactor.allOutputs).joinToString(
          prefix = "// Port constructors\n", separator = "\n", postfix = "\n") {
            when (it) {
              is Input -> generateInputCtor(it)
              is Output -> generateOutputCtor(it)
              else -> throw IllegalArgumentException("Error: Port was neither input nor output")
            }
          }

  private fun generateReactorCtorCode(input: Input) =
      "LF_INITIALIZE_INPUT(${reactor.codeType}, ${input.name}, ${input.width}, ${input.external_args});"

  private fun generateReactorCtorCode(output: Output) =
      "LF_INITIALIZE_OUTPUT(${reactor.codeType}, ${output.name}, ${output.width}, ${output.external_args});"

  private fun generateReactorCtorCode(port: Port) =
      when (port) {
        is Input -> generateReactorCtorCode(port)
        is Output -> generateReactorCtorCode(port)
        else -> throw IllegalArgumentException("Error: Port was neither input nor output")
      }

  fun generateReactorCtorCodes() =
      reactor.allInputs.plus(reactor.allOutputs).joinToString(
          prefix = "// Initialize ports\n", separator = "\n", postfix = "\n") {
            generateReactorCtorCode(it)
          }

  fun generateDefineContainedOutputArgs(r: Instantiation) =
      r.reactor.allOutputs.joinToString(separator = "\n", prefix = "\n", postfix = "\n") {
        "LF_DEFINE_CHILD_OUTPUT_ARGS(${r.name}, ${it.name}, ${r.codeWidth}, ${it.width});"
      }

  fun generateDefineContainedInputArgs(r: Instantiation) =
      r.reactor.allInputs.joinToString(separator = "\n", prefix = "\n", postfix = "\n") {
        "LF_DEFINE_CHILD_INPUT_ARGS(${r.name}, ${it.name}, ${r.codeWidth}, ${it.width});"
      }

  fun generateReactorCtorDefArguments() =
      reactor.allOutputs.joinToString(separator = "") {
        ", OutputExternalCtorArgs *${it.external_args}"
      } +
          reactor.allInputs.joinToString(separator = "") {
            ", InputExternalCtorArgs *${it.external_args}"
          }

  fun generateReactorCtorDeclArguments(r: Instantiation) =
      r.reactor.allOutputs.plus(r.reactor.allInputs).joinToString(separator = "") {
        ", _${r.name}_${it.name}_args[i]"
      }
}
