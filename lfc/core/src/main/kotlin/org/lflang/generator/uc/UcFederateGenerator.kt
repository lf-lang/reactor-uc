package org.lflang.generator.uc

import org.lflang.MessageReporter
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeTypeFederate
import org.lflang.lf.*
import org.lflang.reactor
import org.lflang.toUnixString

class UcFederateGenerator(private val federate: Instantiation, private val fileConfig: UcFileConfig, messageReporter: MessageReporter) {

    private val container = federate.eContainer() as Reactor
    private val reactor = federate.reactor
    private val connections = UcConnectionGenerator(container)
    private val parameters = UcParameterGenerator(container)
    private val ports = UcPortGenerator(container, connections)
    private val reactions = UcReactionGenerator(container)
    private val instances = UcInstanceGenerator(container, parameters, ports, connections, reactions, fileConfig, messageReporter)
    private val headerFile = "Federate.h"

    fun numBundles() = 1

    private val includeGuard = "LFC_GEN_FEDERATE_${federate.name.uppercase()}_H"


    private fun generateFederateStruct() = with(PrependOperator) {
        """
            |typedef struct {
            |  Reactor super;
        ${" |  "..instances.generateReactorStructField(federate)}
        ${" |  "..connections.generateReactorStructFields()}
        ${" |  "..instances.generateReactorStructContainedInputFields(federate)}
        ${" |  "..instances.generateReactorStructContainedOutputFields(federate)}
            |  LF_FEDERATE_BOOKKEEPING_INSTANCES(${numBundles()});
            |} ${federate.codeTypeFederate};
            |
            """.trimMargin()
    }

    private fun generateCtorDefinition() = with(PrependOperator) {
        """
            |${generateCtorDeclaration()} {
            |   LF_FEDERATE_CTOR_PREAMBLE();
            |   LF_REACTOR_CTOR(${federate.codeTypeFederate});
        ${" |   "..instances.generateReactorCtorCode(federate)}
        ${" |   "..connections.generateReactorCtorCodes()}
            |}
            |
        """.trimMargin()
    }

    private fun generateCtorDeclaration() = "LF_REACTOR_CTOR_SIGNATURE(${federate.codeTypeFederate})"

    fun generateHeader() = with(PrependOperator) {
        """
            |#ifndef ${includeGuard}
            |#define ${includeGuard}
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(reactor).toUnixString()}"
            |
        ${" |"..connections.generateSelfStructs()}
        ${" |"..generateFederateStruct()}
        ${" |"..generateCtorDeclaration()};
            |#endif // ${includeGuard}
        """.trimMargin()
    }

    fun generateSource() = with(PrependOperator) {
        """
            |#include "${headerFile}"
            |
        ${" |"..connections.generateCtors()}
        ${" |"..generateCtorDefinition()}
            |
        """.trimMargin()
    }
}

