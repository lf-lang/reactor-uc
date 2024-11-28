package org.lflang.generator.uc

import org.lflang.MessageReporter
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcActionGenerator.Companion.maxNumPendingEvents
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.lf.*
import org.lflang.reactor
import org.lflang.toUnixString

class UcReactorGenerator(private val reactor: Reactor, fileConfig: UcFileConfig, messageReporter: MessageReporter) {

    private val headerFile = fileConfig.getReactorHeaderPath(reactor).toUnixString()

    private val hasStartup = reactor.reactions.filter {
        it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.STARTUP }.isNotEmpty()
    }.isNotEmpty()
    private val hasShutdown = reactor.reactions.filter {
        it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN }.isNotEmpty()
    }.isNotEmpty()

    private fun numTriggers(): Int {
        var res = reactor.actions.size + reactor.timers.size + reactor.inputs.map { it.width }
            .sum() + reactor.outputs.map { it.width }.sum()
        if (hasShutdown) res++;
        if (hasStartup) res++;
        return res;
    }

    private val numChildren = reactor.instantiations.map { it.width }.sum()

    private val parameters = UcParameterGenerator(reactor)
    private val connections = UcConnectionGenerator(reactor)
    private val state = UcStateGenerator(reactor)
    private val ports = UcPortGenerator(reactor, connections)
    private val timers = UcTimerGenerator(reactor)
    private val actions = UcActionGenerator(reactor)
    private val reactions = UcReactionGenerator(reactor)
    private val preambles = UcPreambleGenerator(reactor)
    private val instances =
        UcInstanceGenerator(reactor, parameters, ports, connections, reactions, fileConfig, messageReporter)


    private fun takesExtraParameters(): Boolean =
        parameters.generateReactorCtorDefArguments().isNotEmpty() || ports.generateReactorCtorDefArguments()
            .isNotEmpty()

    private fun generateReactorCtorSignature(): String =
        if (takesExtraParameters()) "REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(${reactor.codeType} ${ports.generateReactorCtorDefArguments()} ${parameters.generateReactorCtorDefArguments()} )"
        else "REACTOR_CTOR_SIGNATURE(${reactor.codeType})"


    companion object {
        val Reactor.codeType
            get(): String = "Reactor_$name"

        val Reactor.hasStartup
            get(): Boolean = reactions.filter {
                it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.STARTUP }.isNotEmpty()
            }.isNotEmpty()

        val Reactor.hasShutdown
            get(): Boolean = reactions.filter {
                it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN }.isNotEmpty()
            }.isNotEmpty()

        fun Reactor.getEffects(v: Variable) = reactions.filter { it.triggers.filter { it.name == v.name }.isNotEmpty() }
        fun Reactor.getObservers(v: Variable) =
            reactions.filter { it.sources.filter { it.name == v.name }.isNotEmpty() }

        fun Reactor.getSources(v: Variable) = reactions.filter { it.effects.filter { it.name == v.name }.isNotEmpty() }
        fun Reactor.getEffects(v: BuiltinTrigger) =
            reactions.filter { it.triggers.filter { it.name == v.literal }.isNotEmpty() }

        fun Reactor.getObservers(v: BuiltinTrigger) =
            reactions.filter { it.sources.filter { it.name == v.literal }.isNotEmpty() }

        fun Reactor.getEventQueueSize(): Int {
            var childrenEvents = 0
            for (child in this.instantiations) {
                childrenEvents += child.reactor.getEventQueueSize()
            }
            var currentReactorsEvents = 0
            for (timer in this.timers) {
                currentReactorsEvents += 1
            }
            for (action in this.actions) {
                currentReactorsEvents += action.maxNumPendingEvents
            }
            val ucConnections = UcConnectionGenerator(this)
            currentReactorsEvents += ucConnections.getMaxNumPendingEvents()
            return childrenEvents + currentReactorsEvents
        }

        fun Reactor.getReactionQueueSize(): Int {
            var res = 0
            for (child in instantiations) {
                res += child.reactor.getReactionQueueSize()
            }
            res += reactions.size
            return res
        }

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
            |  REACTOR_BOOKKEEPING_INSTANCES(${reactor.reactions.size}, ${numTriggers()}, ${numChildren});
            |} ${reactor.codeType};
            |
            """.trimMargin()
    }

    private fun generateCtorDefinition() = with(PrependOperator) {
        """
            |${generateReactorCtorSignature()} {
            |   REACTOR_CTOR_PREAMBLE();
            |   REACTOR_CTOR(${reactor.codeType});
        ${" |   "..parameters.generateReactorCtorCodes()}
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
            |#include "reactor-uc/reactor-uc.h"
            |
        ${" |"..preambles.generateReactorPreamble()}
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
        """.trimMargin()
    }

    fun generateSource() = with(PrependOperator) {
        """
            |#include "${headerFile}"
            |
        ${" |"..reactions.generateReactionBodies()}
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

