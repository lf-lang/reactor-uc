package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.lf.*

class UcInstanceGenerator(
    private val variant: UcReactorVariant,
    private val parameters: UcParameterGenerator,
    private val ports: UcPortGenerator,
    private val connections: UcConnectionGenerator,
    private val reactions: UcReactionGenerator,
    private val fileConfig: UcFileConfig,
    private val messageReporter: MessageReporter
) {
  private val reactor = variant.reactor

  companion object {
    val Instantiation.codeTypeFederate
      get(): String = "${(eContainer() as Reactor).name}_${name}"

    val Instantiation.isAFederate
      get(): Boolean = this.eContainer() is Reactor && (this.eContainer() as Reactor).isFederated
  }

  fun generateIncludes(): String =
      reactor.allInstantiations
          .map { fileConfig.getReactorHeaderPath(variant.child(it)) }
          .distinct()
          .joinToString(prefix = "// Include instantiated reactors\n", separator = "\n") {
            """#include "${it.toUnixString()}" """
          }

  fun generateReactorStructContainedOutputFields(inst: Instantiation) =
      inst.reactor.allOutputs.joinToString(separator = "\n") {
        with(PrependOperator) {
          """|
            |LF_CHILD_OUTPUT_CONNECTIONS(${inst.name}, ${it.name}, ${variant.codeWidth(inst)}, ${variant.childPortWidth(inst, it)}, ${connections.getNumConnectionsFromPort(inst, it)});
            |LF_CHILD_OUTPUT_EFFECTS(${inst.name}, ${it.name}, ${variant.codeWidth(inst)}, ${variant.childPortWidth(inst, it)}, ${reactions.getParentReactionEffectsOfOutput(inst, it).size});
            |LF_CHILD_OUTPUT_OBSERVERS(${inst.name}, ${it.name}, ${variant.codeWidth(inst)}, ${variant.childPortWidth(inst, it)}, ${reactions.getParentReactionObserversOfOutput(inst, it).size});
        """
              .trimMargin()
        }
      }

  fun generateReactorStructContainedInputFields(inst: Instantiation) =
      inst.reactor.allInputs.joinToString(separator = "\n") {
        with(PrependOperator) {
          """|
            |LF_CHILD_INPUT_SOURCES(${inst.name}, ${it.name}, ${variant.codeWidth(inst)}, ${variant.childPortWidth(inst, it)}, ${reactions.getParentReactionSourcesOfInput(inst, it).size});
        """
              .trimMargin()
        }
      }

  fun generateReactorStructField(inst: Instantiation) =
      with(PrependOperator) {
        """|
           |LF_CHILD_REACTOR_INSTANCE(${variant.child(inst).codeType}, ${inst.name}, ${variant.codeWidth(inst)});
           |${generateReactorStructContainedOutputFields(inst)}
           |${generateReactorStructContainedInputFields(inst)}
            """
            .trimMargin()
      }

  fun generateReactorStructFields() =
      reactor.allInstantiations.joinToString(
          prefix = "// Child reactor fields\n", separator = "\n", postfix = "\n") {
            generateReactorStructField(it)
          }

  fun generateReactorCtorCode(inst: Instantiation) =
      with(PrependOperator) {
        """|
       ${" |"..ports.generateDefineContainedOutputArgs(inst)}
       ${" |"..ports.generateDefineContainedInputArgs(inst)}
           |${ if (parameters.generateReactorCtorDeclArguments(inst).isNotEmpty() || ports.generateReactorCtorDeclArguments(inst).isNotEmpty())
            "LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(${variant.child(inst).codeType}, ${inst.name}, ${variant.codeWidth(inst)} ${ports.generateReactorCtorDeclArguments(inst)} ${parameters.generateReactorCtorDeclArguments(inst)});"
        else "LF_INITIALIZE_CHILD_REACTOR(${variant.child(inst).codeType}, ${inst.name}, ${variant.codeWidth(inst)});"
        }
       """
            .trimMargin()
      }

  fun generateReactorCtorCodes() =
      reactor.allInstantiations.joinToString(separator = "\n") { generateReactorCtorCode(it) }
}
