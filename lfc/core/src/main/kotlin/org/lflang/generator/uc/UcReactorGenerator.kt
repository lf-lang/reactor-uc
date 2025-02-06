package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcActionGenerator.Companion.maxNumPendingEvents
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeWidth
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.lf.*

class UcReactorGenerator(private val reactor: Reactor, private val fileConfig: UcFileConfig, messageReporter: MessageReporter) {

    private val headerFile = fileConfig.getReactorHeaderPath(reactor).toUnixString()

    private val hasStartup = reactor.reactions.filter {
        it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.STARTUP }.isNotEmpty()
    }.isNotEmpty()
    private val hasShutdown = reactor.allReactions.filter {
        it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN }.isNotEmpty()
    }.isNotEmpty()

    private fun numTriggers(): Int {
        var res = reactor.allActions.size + reactor.allTimers.size + reactor.allInputs.map { it.width }
            .sum() + reactor.allOutputs.map { it.width }.sum()
        if (hasShutdown) res++;
        if (hasStartup) res++;
        return res;
    }

    private val numChildren = reactor.allInstantiations.map { it.codeWidth }.sum()

    private val parameters = UcParameterGenerator(reactor)
    private val connections = UcConnectionGenerator(reactor, null, emptyList())
    private val state = UcStateGenerator(reactor)
    private val ports = UcPortGenerator(reactor, connections)
    private val timers = UcTimerGenerator(reactor)
    private val actions = UcActionGenerator(reactor)
    private val reactions = UcReactionGenerator(reactor)
    private val instances =
        UcInstanceGenerator(reactor, parameters, ports, connections, reactions, fileConfig, messageReporter)


    private fun takesExtraParameters(): Boolean =
        parameters.generateReactorCtorDefArguments().isNotEmpty() || ports.generateReactorCtorDefArguments()
            .isNotEmpty()

    private fun generateReactorCtorSignature(): String =
        if (takesExtraParameters()) "LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(${reactor.codeType} ${ports.generateReactorCtorDefArguments()} ${parameters.generateReactorCtorDefArguments()} )"
        else "LF_REACTOR_CTOR_SIGNATURE(${reactor.codeType})"

    fun generateReactorPrivatePreamble() = reactor.allPreambles.joinToString(prefix= "// Private preambles\n", separator = "\n", postfix = "\n") { it.code.toText()}

    companion object {
        val Reactor.codeType
            get(): String = "Reactor_$name"

        val Reactor.includeGuard
            get(): String = "LFC_GEN_${name.uppercase()}_H"

        val Reactor.hasStartup
            get(): Boolean = allReactions.filter {
                it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.STARTUP }.isNotEmpty()
            }.isNotEmpty()

        val Reactor.hasShutdown
            get(): Boolean = allReactions.filter {
                it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN }.isNotEmpty()
            }.isNotEmpty()

        fun Reactor.getEffects(v: Variable) = allReactions.filter { it.triggers.filter { it.name == v.name }.isNotEmpty() }
        fun Reactor.getObservers(v: Variable) =
            allReactions.filter { it.sources.filter { it.name == v.name }.isNotEmpty() }

        fun Reactor.getSources(v: Variable) = allReactions.filter { it.effects.filter { it.name == v.name }.isNotEmpty() }
        fun Reactor.getEffects(v: BuiltinTrigger) =
            allReactions.filter { it.triggers.filter { it.name == v.literal }.isNotEmpty() }

        fun Reactor.getObservers(v: BuiltinTrigger) =
            allReactions.filter { it.sources.filter { it.name == v.literal }.isNotEmpty() }
    }

    fun getMaxNumPendingEvents(): Int {
        var numEvents = reactor.allTimers.count()
        for (action in reactor.allActions) {
            numEvents += action.maxNumPendingEvents
        }
        numEvents += connections.getMaxNumPendingEvents()
        return numEvents
    }

    private fun generateReactorStruct() = with(PrependOperator) {
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
            """.trimMargin()
    }

    private fun generateCtorDefinition() = with(PrependOperator) {
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
        """.trimMargin()
    }

    fun generateHeader() = with(PrependOperator) {
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
        """.trimMargin()
    }

    fun generateSource() = with(PrependOperator) {
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
        """.trimMargin()
    }

}
