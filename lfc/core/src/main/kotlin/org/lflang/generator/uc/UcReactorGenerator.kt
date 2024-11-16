package org.lflang.generator.uc

import org.lflang.MessageReporter
import org.lflang.generator.PrependOperator
import org.lflang.lf.BuiltinTrigger
import org.lflang.lf.BuiltinTriggerRef
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.priority
import org.lflang.toUnixString

class UcReactorGenerator(private val reactor: Reactor, fileConfig: UcFileConfig, messageReporter: MessageReporter) {

    private val headerFile = fileConfig.getReactorHeaderPath(reactor).toUnixString()

    private val hasStartup = reactor.reactions.filter {it.triggers.filter {it is BuiltinTriggerRef && it.type == BuiltinTrigger.STARTUP}.isNotEmpty()}.isNotEmpty()
    private val hasShutdown = reactor.reactions.filter {it.triggers.filter {it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN}.isNotEmpty()}.isNotEmpty()

    private fun numTriggers(): Int {
        var res = reactor.actions.size + reactor.timers.size + reactor.inputs.size;
        if (hasShutdown) res++;
        if (hasStartup) res++;
        return res;
    }
    private val numChildren = reactor.instantiations.size;

    private val parameters = UcParameterGenerator(reactor)
    private val connections = UcConnectionGenerator(reactor)
    private val state = UcStateGenerator(reactor)
    private val ports = UcPortGenerator(reactor, connections)
    private val timers = UcTimerGenerator(reactor)
    private val actions = UcActionGenerator(reactor)
    private val reactions = UcReactionGenerator(reactor, ports)
    private val preambles = UcPreambleGenerator(reactor)
    private val instances = UcInstanceGenerator(reactor, parameters, ports, connections, reactions, fileConfig, messageReporter)


    private fun takesExtraParameters(): Boolean = parameters.generateReactorCtorDefArguments().isNotEmpty() || ports.generateReactorCtorDefArguments().isNotEmpty()
    fun generateReactorCtorSignature(): String =
        if (takesExtraParameters())
           "REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(${reactor.codeType} ${parameters.generateReactorCtorDefArguments()} ${ports.generateReactorCtorDefArguments()})"
        else
            "REACTOR_CTOR_SIGNATURE(${reactor.codeType})"


    companion object {
        val Reactor.codeType
            get(): String = "Reactor_$name"
    }

    fun generateReactorStruct() = with(PrependOperator) {
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
            |   REACTOR_BOOKKEEPING_INSTANCES(${reactor.reactions.size}, ${numTriggers()}, ${numChildren});
            |} ${reactor.codeType};
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
            | // The reactor self struct
        ${" |"..generateReactorStruct()}
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
        """.trimMargin()
    }

    fun generateCtorDefinition() = with(PrependOperator) {
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
        """.trimMargin()
    }
}

