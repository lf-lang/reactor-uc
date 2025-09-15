package org.lflang.generator.uc

import org.lflang.MessageReporter
import org.lflang.generator.PrependOperator
import org.lflang.ir.Reactor
import org.lflang.ir.TriggerKind
import org.lflang.toUnixString

class UcReactorGenerator(
    private val reactor: Reactor,
    private val fileConfig: UcFileConfig,
    messageReporter: MessageReporter
) {

  private val headerFile = fileConfig.getReactorHeaderPath(reactor).toUnixString()

  private val hasStartup =
      reactor.reactions.any {
          it.triggers.any { it.kind == TriggerKind.STARTUP }
      }

    private val hasShutdown =
        reactor.reactions.any {
            it.triggers.any { it.kind == TriggerKind.SHUTDOWN }
        }

    private fun numTriggers(): Int {
    var res =
        reactor.actions.size +
            reactor.timers.size +
                reactor.inputs.sumOf { it.width } +
                reactor.outputs.sumOf { it.width }
        if (hasShutdown) res++
    if (hasStartup) res++
    return res
  }

  private val numChildren = reactor.childReactors.map { it.codeWidth }.sum()

  private val parameters = UcParameterGenerator(reactor)
  private val connections = UcConnectionGenerator(reactor, null, emptyList())
  private val state = UcStateGenerator(reactor)
  private val ports = UcPortGenerator(reactor, connections)
  private val timers = UcTimerGenerator(reactor)
  private val actions = UcActionGenerator(reactor)
  private val reactions = UcReactionGenerator(reactor)
  private val instances =
      UcInstanceGenerator(
          reactor, parameters, ports, connections, reactions, fileConfig, messageReporter)

  private fun takesExtraParameters(): Boolean =
      parameters.generateReactorCtorDefArguments().isNotEmpty() ||
          ports.generateReactorCtorDefArguments().isNotEmpty()

  private fun generateReactorCtorSignature(): String =
      if (takesExtraParameters())
          "LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(${reactor.codeType} ${ports.generateReactorCtorDefArguments()} ${parameters.generateReactorCtorDefArguments()} )"
      else "LF_REACTOR_CTOR_SIGNATURE(${reactor.codeType})"

  fun generateReactorPrivatePreamble() =
      reactor.preambles.joinToString(
          prefix = "// Private preambles\n", separator = "\n", postfix = "\n") {
            it.code
          }

  companion object {

  }

  fun getMaxNumPendingEvents(): Int {
    var numEvents = reactor.timers.count()
    for (action in reactor.actions) {
      numEvents += action.maxNumPendingEvents
    }
    numEvents += connections.getMaxNumPendingEvents()
    return numEvents
  }

  private fun generateReactorStruct() =
      with(PrependOperator) {
        """
            |typedef struct {
            |  Reactor super;
        ${" |  "..instances.generateReactorStructFields()}
        ${" |  "..reactions.generateReactorStructFields()}
        ${" |  "..timers.generateReactorStructFields()}
        ${" |  "..actions.generateReactorStructFields()}
        ${" |  "..connections.generateReactorStructFields()}
        ${" |  "..ports.generateReactorStructFields()}
        ${" |  "..state.generateReactorStructFields()}
        ${" |  "..parameters.generateReactorStructFields()}
            |  LF_REACTOR_BOOKKEEPING_INSTANCES(${reactor.reactions.size}, ${numTriggers()}, ${numChildren});
            |} ${reactor.codeType};
            |
            """
            .trimMargin()
      }

  private fun generateCtorDefinition() =
      with(PrependOperator) {
        """
            |${generateReactorCtorSignature()} {
            |   LF_REACTOR_CTOR_PREAMBLE();
            |   LF_REACTOR_CTOR(${reactor.codeType});
        ${" |   "..parameters.generateReactorCtorCodes()}
        ${" |   "..state.generateInitializeStateVars()}
        ${" |   "..instances.generateReactorCtorCodes()}
        ${" |   "..timers.generateReactorCtorCodes()}
        ${" |   "..actions.generateReactorCtorCodes()}
        ${" |   "..ports.generateReactorCtorCodes()}
        ${" |   "..connections.generateReactorCtorCodes()}
        ${" |   "..reactions.generateReactorCtorCodes()}
            |}
            |
        """
            .trimMargin()
      }

  fun generateHeader() =
      with(PrependOperator) {
        """
            |#ifndef ${reactor.includeGuard}
            |#define ${reactor.includeGuard}
            |
            |#include "reactor-uc/reactor-uc.h"
            |#include "_lf_preamble.h"
            |
        ${" |"..instances.generateIncludes()}
        ${" |"..reactions.generateSelfStructs()}
        ${" |"..timers.generateSelfStructs()}
        ${" |"..actions.generateSelfStructs()}
        ${" |"..ports.generateSelfStructs()}
        ${" |"..connections.generateSelfStructs()}
            |//The reactor self struct
        ${" |"..generateReactorStruct()}
            |
            |${generateReactorCtorSignature()};
            |
            |#endif // ${reactor.includeGuard}
        """
            .trimMargin()
      }

  fun generateSource() =
      with(PrependOperator) {
        """
            |#include "${headerFile}"
        ${" |"..generateReactorPrivatePreamble()}
        ${" |"..reactions.generateReactionBodies()}
        ${" |"..reactions.generateReactionDeadlineViolationHandlers()}
        ${" |"..reactions.generateReactionStpViolationHandlers()}
        ${" |"..reactions.generateReactionCtors()}
        ${" |"..actions.generateCtors()}
        ${" |"..timers.generateCtors()}
        ${" |"..ports.generateCtors()}
        ${" |"..connections.generateCtors()}
        ${" |"..generateCtorDefinition()}
            |
        """
            .trimMargin()
      }
}
