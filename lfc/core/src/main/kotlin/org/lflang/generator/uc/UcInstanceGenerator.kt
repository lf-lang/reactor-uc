package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*
import org.lflang.target.property.type.BuildTypeType.BuildType

class UcInstanceGenerator(
    private val reactor: Reactor,
    private val parameters: UcParameterGenerator,
    private val ports: UcPortGenerator,
    private val connections: UcConnectionGenerator,
    private val reactions: UcReactionGenerator,
    private val fileConfig: UcFileConfig,
    private val messageReporter: MessageReporter
) {
    companion object {
        val Instantiation.width
            get(): Int = widthSpec?.getWidth()?:1
        val Instantiation.codeWidth
            get(): Int = if (this.isAFederate) 1 else width
        val Instantiation.codeTypeFederate
            get(): String = "${(eContainer() as Reactor).name}_${name}"
        val Instantiation.isAFederate
            get(): Boolean = this.eContainer() is Reactor && (this.eContainer() as Reactor).isFederated
    }

    fun generateIncludes(): String =
        reactor.allInstantiations.map { fileConfig.getReactorHeaderPath(it.reactor) }
            .distinct()
            .joinToString(prefix="// Include instantiated reactors\n", separator = "\n") { """#include "${it.toUnixString()}" """ }

    fun generateReactorStructContainedOutputFields(inst: Instantiation) = inst.reactor.allOutputs.joinToString(separator = "\n") { with (PrependOperator) {
        """|
            |LF_CHILD_OUTPUT_CONNECTIONS(${inst.name}, ${it.name}, ${inst.codeWidth}, ${it.width}, ${connections.getNumConnectionsFromPort(inst, it)});
            |LF_CHILD_OUTPUT_EFFECTS(${inst.name}, ${it.name}, ${inst.codeWidth}, ${it.width}, ${reactions.getParentReactionEffectsOfOutput(inst, it).size});
            |LF_CHILD_OUTPUT_OBSERVERS(${inst.name}, ${it.name}, ${inst.codeWidth}, ${it.width}, ${reactions.getParentReactionObserversOfOutput(inst, it).size});
        """.trimMargin()
    }}

    fun generateReactorStructContainedInputFields(inst: Instantiation) = inst.reactor.allInputs.joinToString(separator = "\n") { with (PrependOperator) {
        """|
            |LF_CHILD_INPUT_SOURCES(${inst.name}, ${it.name}, ${inst.codeWidth}, ${it.width}, ${reactions.getParentReactionSourcesOfInput(inst, it).size});
        """.trimMargin()
    }}

    fun generateReactorStructField(inst: Instantiation) = with(PrependOperator) {
        """|
           |LF_CHILD_REACTOR_INSTANCE(${inst.reactor.codeType}, ${inst.name}, ${inst.codeWidth});
           |${generateReactorStructContainedOutputFields(inst)}
           |${generateReactorStructContainedInputFields(inst)}
            """.trimMargin()
    }

    fun generateReactorStructFields() = reactor.allInstantiations.joinToString(prefix = "// Child reactor fields\n", separator = "\n", postfix = "\n") {generateReactorStructField(it)}

    fun generateReactorCtorCode(inst: Instantiation) = with(PrependOperator) {
        """|
       ${" |"..ports.generateDefineContainedOutputArgs(inst)}
       ${" |"..ports.generateDefineContainedInputArgs(inst)}
           |${ if (parameters.generateReactorCtorDeclArguments(inst).isNotEmpty() || ports.generateReactorCtorDeclArguments(inst).isNotEmpty())
            "LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(${inst.reactor.codeType}, ${inst.name}, ${inst.codeWidth} ${ports.generateReactorCtorDeclArguments(inst)} ${parameters.generateReactorCtorDeclArguments(inst)});"
        else "LF_INITIALIZE_CHILD_REACTOR(${inst.reactor.codeType}, ${inst.name}, ${inst.codeWidth});"
        }
       """.trimMargin()
    }

    fun generateReactorCtorCodes() = reactor.allInstantiations.joinToString(separator = "\n") { generateReactorCtorCode(it)}
}