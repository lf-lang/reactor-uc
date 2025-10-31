package org.lflang.generator.uc2

import org.lflang.generator.PrependOperator
import org.lflang.ir.Reactor
import org.lflang.ir.ReactorInstantiation
import org.lflang.toUnixString

class UcInstanceGenerator(
    private val reactor: Reactor,
    private val parameters: UcParameterGenerator,
    private val ports: UcPortGenerator,
    private val connections: UcLocalConnectionGenerator,
    private val reactions: UcReactionGenerator,
    private val fileConfig: UcFileConfig,
) {
  fun generateIncludes(): String =
      reactor.childReactors
          .map { fileConfig.getReactorHeaderPath(it.reactor) }
          .distinct()
          .joinToString(prefix = "// Include instantiated reactors\n", separator = "\n") {
            """#include "${it.toUnixString()}" """
          }

  fun generateReactorStructContainedOutputFields(inst: ReactorInstantiation) =
      inst.reactor.outputs.joinToString(separator = "\n") {
        with(PrependOperator) {
          """|
            |LF_CHILD_OUTPUT_CONNECTIONS(${inst.name}, ${it.lfName}, ${inst.codeWidth}, ${it.width}, ${connections.getNumConnectionsFromPort(inst, it)});
            |LF_CHILD_OUTPUT_EFFECTS(${inst.name}, ${it.lfName}, ${inst.codeWidth}, ${it.width}, ${reactions.getParentReactionEffectsOfOutput(inst, it).size});
            |LF_CHILD_OUTPUT_OBSERVERS(${inst.name}, ${it.lfName}, ${inst.codeWidth}, ${it.width}, ${reactions.getParentReactionObserversOfOutput(inst, it).size});
        """
              .trimMargin()
        }
      }

  fun generateReactorStructContainedInputFields(inst: ReactorInstantiation) =
      inst.reactor.inputs.joinToString(separator = "\n") {
        with(PrependOperator) {
          """|
            |LF_CHILD_INPUT_SOURCES(${inst.name}, ${it.lfName}, ${inst.codeWidth}, ${it.width}, ${reactions.getParentReactionSourcesOfInput(inst, it).size});
        """
              .trimMargin()
        }
      }

  fun generateReactorStructField(inst: ReactorInstantiation) =
      with(PrependOperator) {
        """|
           |LF_CHILD_REACTOR_INSTANCE(${inst.reactor.codeType}, ${inst.name}, ${inst.codeWidth});
           |${generateReactorStructContainedOutputFields(inst)}
           |${generateReactorStructContainedInputFields(inst)}
            """
            .trimMargin()
      }

  fun generateReactorStructFields() =
      reactor.childReactors.joinToString(
          prefix = "// Child reactor fields\n", separator = "\n", postfix = "\n") {
            generateReactorStructField(it)
          }

  fun generateReactorCtorCode(inst: ReactorInstantiation) =
      with(PrependOperator) {
        """|
       ${" |"..ports.generateDefineContainedOutputArgs(inst)}
       ${" |"..ports.generateDefineContainedInputArgs(inst)}
           |${ if (parameters.generateReactorCtorDeclArguments(inst).isNotEmpty() || ports.generateReactorCtorDeclArguments(inst).isNotEmpty())
            "LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(${inst.reactor.codeType}, ${inst.name}, ${inst.codeWidth} ${ports.generateReactorCtorDeclArguments(inst)} ${parameters.generateReactorCtorDeclArguments(inst)});"
        else "LF_INITIALIZE_CHILD_REACTOR(${inst.reactor.codeType}, ${inst.name}, ${inst.codeWidth});"
        }
       """
            .trimMargin()
      }

  fun generateReactorCtorCodes() =
      reactor.childReactors.joinToString(separator = "\n") { generateReactorCtorCode(it) }
}
