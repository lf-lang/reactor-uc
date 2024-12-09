package org.lflang.generator.uc

import org.lflang.MessageReporter
import org.lflang.generator.PrependOperator
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
    private val headerFile = fileConfig.getReactorHeaderPath(federate.reactor).toUnixString()

    fun numBundles() = 1

    private val includeGuard = "LFC_GEN_FEDERATE_${federate.name.uppercase()}_H"
    private val topLevelCodeType = "Federate_${federate.name}"


    private fun generateFederateStruct() = with(PrependOperator) {
        """
            |typedef struct {
            |  Reactor super;
        ${" |  "..instances.generateReactorStructField(federate)}
        ${" |  "..connections.generateReactorStructFields()}
        ${" |  "..instances.generateReactorStructContainedInputFields(federate)}
        ${" |  "..instances.generateReactorStructContainedOutputFields(federate)}
            |  LF_FEDERATE_BOOKKEEPING_INSTANCES(${numBundles()});
            |} ${topLevelCodeType};
            |
            """.trimMargin()
    }

    private fun generateCtorDefinition() = with(PrependOperator) {
        """
            |LF_REACTOR_CTOR_SIGNATURE(${topLevelCodeType}) {
            |   LF_FEDERATE_CTOR_PREAMBLE();
            |   LF_REACTOR_CTOR(${topLevelCodeType});
        ${" |   "..instances.generateReactorCtorCode(federate)}
        ${" |   "..connections.generateReactorCtorCodes()}
            |}
            |
        """.trimMargin()
    }

    fun generateHeader() = with(PrependOperator) {
        """
            |#ifndef ${includeGuard}
            |#define ${includeGuard}
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(reactor).toUnixString()}"
            |
        ${" |"..connections.generateSelfStructs()}
        ${" |"..generateFederateStruct()}
            |//The reactor self struct
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

