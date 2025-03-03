package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.lf.*
import org.lflang.target.TargetConfig

class UcFederateGenerator(
    private val currentFederate: UcFederate,
    private val otherFederates: List<UcFederate>,
    private val fileConfig: UcFileConfig,
    messageReporter: MessageReporter,
    targetConfig: TargetConfig
) {

  private val container = currentFederate.inst.eContainer() as Reactor
  private val reactor = currentFederate.inst.reactor
  private val connections = UcConnectionGenerator(container, currentFederate, otherFederates)
  private val parameters = UcParameterGenerator(container, currentFederate)
  private val ports = UcPortGenerator(container, connections)
  private val reactions = UcReactionGenerator(container)
  private val instances =
      UcInstanceGenerator(
          container, parameters, ports, connections, reactions, fileConfig, messageReporter)
  private val clockSync = UcClockSyncGenerator(currentFederate, connections, targetConfig)
  private val startupCooordinator = UcStartupCoordinatorGenerator(currentFederate, connections)
  private val headerFile = "lf_federate.h"
  private val includeGuard = "LFC_GEN_FEDERATE_${currentFederate.inst.name.uppercase()}_H"

  init {
    if (!connections.areFederatesFullyConnected()) {
      messageReporter.nowhere().error("Federates are not fully connected!")
    }
  }

  fun getMaxNumPendingEvents(): Int {
    return connections.getMaxNumPendingEvents()
  }

  private fun generateFederateStruct() =
      with(PrependOperator) {
        """
            |typedef struct {
            |  Reactor super;
        ${" |  "..instances.generateReactorStructField(currentFederate.inst)}
        ${" |  "..connections.generateReactorStructFields()}
        ${" |  "..connections.generateFederateStructFields()}
            |  // Startup and clock sync objects. 
        ${" |  "..startupCooordinator.generateFederateStructField()}
        ${" |  "..clockSync.generateFederateStructField()}
            |  LF_FEDERATE_BOOKKEEPING_INSTANCES(${connections.getNumFederatedConnectionBundles()});
            |} ${currentFederate.codeType};
            |
            """
            .trimMargin()
      }

  private fun generateCtorDefinition() =
      with(PrependOperator) {
        """
            |${generateCtorDeclaration()} {
            |   LF_FEDERATE_CTOR_PREAMBLE();
            |   LF_REACTOR_CTOR(${currentFederate.codeType});
        ${" |   "..instances.generateReactorCtorCode(currentFederate.inst)}
        ${" |   "..connections.generateFederateCtorCodes()}
        ${" |   "..connections.generateReactorCtorCodes()}
        ${" |   "..clockSync.generateFederateCtorCode()}
        ${" |   "..startupCooordinator.generateFederateCtorCode()}
            |}
            |
        """
            .trimMargin()
      }

  private fun generateCtorDeclaration() = "LF_REACTOR_CTOR_SIGNATURE(${currentFederate.codeType})"

  fun generateHeader() =
      with(PrependOperator) {
        """
            |#ifndef ${includeGuard}
            |#define ${includeGuard}
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(reactor).toUnixString()}"
        ${" |"..connections.generateNetworkChannelIncludes()}
            |
        ${" |"..startupCooordinator.generateSelfStruct()}
        ${" |"..clockSync.generateSelfStruct()}
        ${" |"..connections.generateFederatedSelfStructs()}
        ${" |"..connections.generateSelfStructs()}
        ${" |"..generateFederateStruct()}
        ${" |"..generateCtorDeclaration()};
            |#endif // ${includeGuard}
        """
            .trimMargin()
      }

  fun generateSource() =
      with(PrependOperator) {
        """
            |#include "${headerFile}"
            |
        ${" |"..startupCooordinator.generateCtor()}
        ${" |"..clockSync.generateCtor()}
        ${" |"..connections.generateFederatedCtors()}
        ${" |"..connections.generateCtors()}
        ${" |"..generateCtorDefinition()}
            |
        """
            .trimMargin()
      }
}
