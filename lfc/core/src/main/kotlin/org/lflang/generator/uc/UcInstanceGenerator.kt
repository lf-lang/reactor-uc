package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator.rangeTo
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.Instantiation
import org.lflang.lf.Output
import org.lflang.lf.Port
import org.lflang.lf.Reactor

class UcInstanceGenerator(
    private val reactor: Reactor,
    private val parameters: UcParameterGenerator,
    private val connections: UcConnectionGenerator,
    private val fileConfig: UcFileConfig,
    private val messageReporter: MessageReporter
) {
    fun generateIncludes(): String =
        reactor.instantiations.map { fileConfig.getReactorHeaderPath(it.reactor) }
            .distinct()
            .joinToString(separator = "\n") { """#include "${it.toUnixString()}" """ }

    fun generateReactorStructContainedOutputFields(inst: Instantiation) = inst.reactor.outputs.joinToString(separator = "\n") {
        "CONTAINED_OUTPUT_CONNECTIONS(${inst.name}, ${it.name}, ${connections.getNumConnectionsFromPort(it as Port)});"
    }


    fun generateReactorStructFields() = reactor.instantiations.joinToString(prefix = "// Child reactor fields\n", separator = "\n", postfix = "\n") {
        """|
           |CHILD_REACTOR_INSTANCE(${it.reactor.codeType}, ${it.name});
           |${generateReactorStructContainedOutputFields(it)}
            """.trimMargin()
    }

    fun generateReactorCtorCodes() = reactor.instantiations.    joinToString(separator = "\n") {
        if (parameters.generateReactorCtorDeclArguments(it).isNotEmpty() || connections.generateReactorCtorDeclArguments(it).isNotEmpty())
            "INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(${it.reactor.codeType}, ${it.name} ${connections.generateReactorCtorDeclArguments(it)} ${parameters.generateReactorCtorDeclArguments(it)});"
        else
            "INITIALIZE_CHILD_REACTOR(${it.reactor.codeType}, ${it.name});"
    }
}