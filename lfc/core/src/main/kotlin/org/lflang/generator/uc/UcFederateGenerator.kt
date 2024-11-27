package org.lflang.generator.uc

import org.lflang.MessageReporter
import org.lflang.generator.PrependOperator
import org.lflang.lf.*
import org.lflang.reactor
import org.lflang.toUnixString

class UcFederateGenerator(private val federate: Instantiation, fileConfig: UcFileConfig, messageReporter: MessageReporter) {

    private val container = federate.eContainer() as Reactor
    private val reactor = federate.reactor
    private val connections = UcConnectionGenerator(container)
    private val parameters = UcParameterGenerator(container)
    private val ports = UcPortGenerator(container, connections)
    private val reactions = UcReactionGenerator(container)
    private val instances = UcInstanceGenerator(container, parameters, ports, connections, reactions, fileConfig, messageReporter)
    private val headerFile = fileConfig.getReactorHeaderPath(federate.reactor).toUnixString()

    fun numBundles() = 1

    private val Reactor.codeType
        get(): String = "federate_${federate.name}"

    private fun generateFederateStruct() = with(PrependOperator) {
        """
            |typedef struct {
            |  Reactor super;
        ${" |  "..instances.generateReactorStructField(federate)}
        ${" |  "..connections.generateReactorStructFields()}
            |  FEDERATE_BOOKKEEPING_INSTANCES(${numBundles()});
            |} ${reactor.codeType};
            |
            """.trimMargin()
    }

    private fun generateCtorDefinition() = with(PrependOperator) {
        """
            |REACTOR_CTOR_SIGNATURE(${reactor.codeType}) {
            |   FEDERATE_CTOR_PREAMBLE();
            |   REACTOR_CTOR(${reactor.codeType});
        ${" |   "..instances.generateReactorCtorCode(federate)}
        ${" |   "..connections.generateReactorCtorCodes()}
            |}
            |
        """.trimMargin()
    }

    fun generateHeader() = with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
            |
        ${" |"..instances.generateIncludes()}
        ${" |"..reactions.generateSelfStructs()}
        ${" |"..ports.generateSelfStructs()}
        ${" |"..connections.generateSelfStructs()}
            |//The reactor self struct
            |
        """.trimMargin()
    }

    fun generateSource() = with(PrependOperator) {
        """
            |#include "${headerFile}"
            |
        ${" |"..reactions.generateReactionBodies()}
        ${" |"..reactions.generateReactionCtors()}
        ${" |"..ports.generateCtors()}
        ${" |"..connections.generateCtors()}
        ${" |"..generateCtorDefinition()}
            |
        """.trimMargin()
    }
}

