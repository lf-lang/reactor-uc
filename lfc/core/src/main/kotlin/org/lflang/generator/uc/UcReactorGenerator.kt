package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcActionGenerator.Companion.maxNumPendingEvents
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeWidth
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.lf.*

class UcReactorGenerator(
    private val reactor: Reactor,
    private val fileConfig: UcFileConfig,
    messageReporter: MessageReporter
) {

  private val headerFile = fileConfig.getReactorHeaderPath(reactor).toUnixString()

  private val hasStartup =
      reactor.reactions
          .filter {
            it.triggers
                .filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.STARTUP }
                .isNotEmpty()
          }
          .isNotEmpty()
  private val hasShutdown =
      reactor.allReactions
          .filter {
            it.triggers
                .filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN }
                .isNotEmpty()
          }
          .isNotEmpty()

  private fun numTriggers(): Int {
    var res =
        reactor.allActions.size +
            reactor.allTimers.size +
            reactor.allInputs.map { it.width }.sum() +
            reactor.allOutputs.map { it.width }.sum()
    if (hasShutdown) res++
    if (hasStartup) res++
    return res
  }

  private val numChildren = reactor.allInstantiations.map { it.codeWidth }.sum()

  private val enclaveInsts =
      if (reactor.isEnclaved) reactor.allInstantiations.filterNot { it.reactor.isEnclaved }
      else emptyList()

  private val enclaveReactorDefs = enclaveInsts.map { it.reactor }.distinct()

  private val enclaves =
      enclaveInsts
          .map { inst -> (0 until inst.width).toList().map { idx -> UcEnclave(inst, idx) } }
          .flatten()

  private val parameters = UcParameterGenerator(reactor)
  private val connections = UcConnectionGenerator(reactor, null, enclaves)
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
      reactor.allPreambles.joinToString(
          prefix = "// Private preambles\n", separator = "\n", postfix = "\n") {
            it.code.toText()
          }

  // Given the reactor definition of an enclave. Find the number of events within it.
  private fun getNumEventsInEnclave(enclave: Instantiation): Int {
    var numEvents = 0
    fun getNumEventsInner(r: Reactor): Pair<Int, Boolean> {
      var ret = 0
      var hasStartup = r.hasStartup
      for (inst in r.allInstantiations) {
        val res = getNumEventsInner(inst.reactor)
        ret += res.first
        hasStartup = hasStartup || res.second
      }
      ret += r.allTimers.size
      ret += r.allActions.map { it.maxNumPendingEvents }.sum()
      val connections = UcConnectionGenerator(r, null, enclaves)
      ret += connections.getNumEvents()
      return Pair(ret, hasStartup)
    }
    val ret = getNumEventsInner(enclave.reactor)
    numEvents += ret.first
    if (ret.second) numEvents += 1

    // Get worst-case number of events due to enclaved connections. Need to check all enclave
    // instantiations
    val enclaveNodes = enclaves.filter { it.inst == enclave }
    var maxConnEvents = 0

    for (enclaveNode in enclaveNodes) {
      val connEvent = connections.getNumEvents(enclaveNode)
      if (connEvent > maxConnEvents) {
        maxConnEvents = connEvent
      }
    }
    numEvents += maxConnEvents
    return numEvents
  }

  // Get the numer of reactions
  private fun getNumReactionsInEnclave(enclave: Reactor): Int {
    var ret = 0
    fun getNumReactionsInner(r: Reactor): Int {
      var ret = 0
      for (inst in r.allInstantiations) {
        ret += getNumReactionsInner(inst.reactor)
      }
      ret += r.allReactions.size
      return ret
    }
    for (inst in enclave.allInstantiations) {
      ret += getNumReactionsInner(inst.reactor)
    }
    ret += enclave.allReactions.size
    return ret
  }

  fun generateEnclaveStructDeclaration() =
      enclaveInsts.joinToString(
          prefix = "// Enclave structs \n", separator = "\n", postfix = "\n") {
            "LF_DEFINE_ENCLAVE_ENVIRONMENT_STRUCT(${it.reactor.codeType}, ${it.name},${reactor.name}, ${getNumEventsInEnclave(it)}, ${getNumReactionsInEnclave(it.reactor)});" // FIXME: How to get
          }

  fun generateEnclaveCtorDefinition() =
      enclaveInsts.joinToString(prefix = "// Enclave ctors \n", separator = "\n", postfix = "\n") {
        "LF_DEFINE_ENCLAVE_ENVIRONMENT_CTOR(${it.reactor.codeType}, ${it.name}, ${reactor.name});"
      }

  companion object {
    val Reactor.codeType
      get(): String = "Reactor_$name"

    val Reactor.includeGuard
      get(): String = "LFC_GEN_${name.uppercase()}_H"

    val Reactor.hasStartup
      get(): Boolean =
          allReactions
              .filter {
                it.triggers
                    .filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.STARTUP }
                    .isNotEmpty()
              }
              .isNotEmpty()

    val Reactor.hasShutdown
      get(): Boolean =
          allReactions
              .filter {
                it.triggers
                    .filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN }
                    .isNotEmpty()
              }
              .isNotEmpty()

    val Reactor.containsEnclaves
      get(): Boolean {
        if (this.isEnclaved) return true
        for (child in allInstantiations.map { it.reactor }.distinct()) {
          if (child.isEnclaved) return true
        }
        return false
      }

    fun Reactor.getEffects(v: Variable) =
        allReactions.filter { it.triggers.filter { it.name == v.name }.isNotEmpty() }

    fun Reactor.getObservers(v: Variable) =
        allReactions.filter { it.sources.filter { it.name == v.name }.isNotEmpty() }

    fun Reactor.getSources(v: Variable) =
        allReactions.filter { it.effects.filter { it.name == v.name }.isNotEmpty() }

    fun Reactor.getEffects(v: BuiltinTrigger) =
        allReactions.filter { it.triggers.filter { it.name == v.literal }.isNotEmpty() }

    fun Reactor.getObservers(v: BuiltinTrigger) =
        allReactions.filter { it.sources.filter { it.name == v.literal }.isNotEmpty() }

    fun Reactor.hasPhysicalActions(): Boolean {
      for (inst in allInstantiations) {
        if (inst.reactor.hasPhysicalActions()) return true
      }
      return allActions.filter { it.isPhysical }.isNotEmpty()
    }
  }

  fun getMaxNumPendingEvents(): Int {
    var numEvents = reactor.allTimers.count()
    for (action in reactor.allActions) {
      numEvents += action.maxNumPendingEvents
    }
    numEvents += connections.getNumEvents()
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
            |  LF_REACTOR_BOOKKEEPING_INSTANCES(${reactor.allReactions.size}, ${numTriggers()}, ${numChildren});
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
        ${" |"..generateEnclaveStructDeclaration()}
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
        ${" |"..generateEnclaveCtorDefinition()}
        ${" |"..generateCtorDefinition()}
            |
        """
            .trimMargin()
      }
}
