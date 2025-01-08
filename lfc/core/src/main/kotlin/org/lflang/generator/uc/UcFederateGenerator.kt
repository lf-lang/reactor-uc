package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.lf.*


class UcFederateGenerator(private val federate: UcFederate, private val fileConfig: UcFileConfig, messageReporter: MessageReporter) {

    private val container = federate.inst.eContainer() as Reactor
    private val reactor = federate.inst.reactor
    private val connections = UcConnectionGenerator(container, federate)
    private val parameters = UcParameterGenerator(container)
    private val ports = UcPortGenerator(container, connections)
    private val reactions = UcReactionGenerator(container)
    private val instances = UcInstanceGenerator(container, parameters, ports, connections, reactions, fileConfig, messageReporter)
    private val headerFile = "lf_federate.h"

    fun numBundles() = connections.getNumFederatedConnectionBundles()
    // FIXME: Calculate event queue size for federate...

    private val includeGuard = "LFC_GEN_FEDERATE_${federate.inst.name.uppercase()}_H"

    fun getMaxNumPendingEvents(): Int {
        return connections.getMaxNumPendingEvents()
    }

    private fun generateFederateStruct() = with(PrependOperator) {
        """
            |typedef struct {
            |  Reactor super;
        ${" |  "..instances.generateReactorStructField(federate.inst)}
        ${" |  "..connections.generateReactorStructFields()}
        ${" |  "..connections.generateFederateStructFields()}
            |  LF_FEDERATE_BOOKKEEPING_INSTANCES(${numBundles()});
            |} ${federate.codeType};
            |
            """.trimMargin()
    }

    private fun generateCtorDefinition() = with(PrependOperator) {
        """
            |${generateCtorDeclaration()} {
            |   LF_FEDERATE_CTOR_PREAMBLE();
            |   LF_REACTOR_CTOR(${federate.codeType});
        ${" |   "..instances.generateReactorCtorCode(federate.inst)}
        ${" |   "..connections.generateFederateCtorCodes()}
        ${" |   "..connections.generateReactorCtorCodes()}
            |}
            |
        """.trimMargin()
    }

    private fun generateCtorDeclaration() = "LF_REACTOR_CTOR_SIGNATURE(${federate.codeType})"

    fun generateHeader() = with(PrependOperator) {
        """
            |#ifndef ${includeGuard}
            |#define ${includeGuard}
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(reactor).toUnixString()}"
            |
            |#include "reactor-uc/platform/posix/tcp_ip_channel.h"
        ${" |"..connections.generateFederatedSelfStructs()}
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
        ${" |"..connections.generateFederatedCtors()}
        ${" |"..connections.generateCtors()}
        ${" |"..generateCtorDefinition()}
            |
        """.trimMargin()
    }
}

