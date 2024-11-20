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
    }

    fun generateIncludes(): String =
        reactor.instantiations.map { fileConfig.getReactorHeaderPath(it.reactor) }
            .distinct()
            .joinToString(separator = "\n") { """#include "${it.toUnixString()}" """ }

    fun generateReactorStructContainedOutputFields(inst: Instantiation) = inst.reactor.outputs.joinToString(separator = "\n") { with (PrependOperator) {
        """|
            |CHILD_OUTPUT_CONNECTIONS(${inst.name}, ${it.name}, ${inst.width}, ${it.width}, ${connections.getNumConnectionsFromPort(inst, it)});
            |CHILD_OUTPUT_EFFECTS(${inst.name}, ${it.name}, ${inst.width}, ${it.width}, ${reactions.getParentReactionEffectsOfOutput(inst, it).size});
            |CHILD_OUTPUT_OBSERVERS(${inst.name}, ${it.name}, ${inst.width}, ${it.width}, ${reactions.getParentReactionObserversOfOutput(inst, it).size});
        """.trimMargin()
    }}

    fun generateReactorStructContainedInputFields(inst: Instantiation) = inst.reactor.inputs.joinToString(separator = "\n") { with (PrependOperator) {
        """|
            |CHILD_INPUT_SOURCES(${inst.name}, ${it.name}, ${inst.width}, ${it.width}, ${reactions.getParentReactionSourcesOfInput(inst, it).size});
        """.trimMargin()
    }}

    fun generateReactorStructField(inst: Instantiation) = with(PrependOperator) {
        """|
           |CHILD_REACTOR_INSTANCE(${inst.reactor.codeType}, ${inst.name}, ${inst.width});
           |${generateReactorStructContainedOutputFields(inst)}
           |${generateReactorStructContainedInputFields(inst)}
            """.trimMargin()
    }

    fun generateReactorStructFields() = reactor.instantiations.joinToString(prefix = "// Child reactor fields\n", separator = "\n", postfix = "\n") {generateReactorStructField(it)}

    fun generateReactorCtorCode(inst: Instantiation) = with(PrependOperator) {
        """|
       ${" |"..ports.generateDefineContainedOutputArgs(inst)}
       ${" |"..ports.generateDefineContainedInputArgs(inst)}
           |${ if (parameters.generateReactorCtorDeclArguments(inst).isNotEmpty() || ports.generateReactorCtorDeclArguments(inst).isNotEmpty())
            "INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(${inst.reactor.codeType}, ${inst.name}, ${inst.width} ${ports.generateReactorCtorDeclArguments(inst)} ${parameters.generateReactorCtorDeclArguments(inst)});"
        else "INITIALIZE_CHILD_REACTOR(${inst.reactor.codeType}, ${inst.name}, ${inst.width});"
        }
       """.trimMargin()
    }

    fun generateReactorCtorCodes() = reactor.instantiations.joinToString(separator = "\n") { generateReactorCtorCode(it)}
}