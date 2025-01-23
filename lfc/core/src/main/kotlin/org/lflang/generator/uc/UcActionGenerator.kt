package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orZero
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasStartup
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasShutdown
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.generator.uc.UcReactorGenerator.Companion.getEffects
import org.lflang.generator.uc.UcReactorGenerator.Companion.getObservers
import org.lflang.generator.uc.UcReactorGenerator.Companion.getSources
import org.lflang.AttributeUtils.getMaxNumberOfPendingEvents
import org.lflang.lf.*

class UcActionGenerator(private val reactor: Reactor) {

    companion object {
        public val Action.maxNumPendingEvents
            get(): Int {
                val num = getMaxNumberOfPendingEvents(this)
                return if (num > 0) num else 1
            }
    }

    /** Returns the C Enum representing the type of action.*/
    private val Action.actionType
        get(): String = if (isPhysical) "PHYSICAL_ACTION" else "LOGICAL_ACTION"

    private fun generateSelfStruct(action: Action) = with(PrependOperator) {
        """
            |LF_DEFINE_ACTION_STRUCT${if (action.type == null) "_VOID" else ""}(${reactor.codeType}, ${action.name}, ${action.actionType}, ${reactor.getEffects(action).size}, ${reactor.getSources(action).size}, ${reactor.getObservers(action).size}, ${action.maxNumPendingEvents} ${if (action.type != null) ", ${action.type.toText()}" else ""});
            |
        """.trimMargin()
    }

    private fun generateCtor(action: Action) = with(PrependOperator) {
        """
            |LF_DEFINE_ACTION_CTOR${if (action.type == null) "_VOID" else ""}(${reactor.codeType}, ${action.name}, ${action.actionType}, ${reactor.getEffects(action).size}, ${reactor.getSources(action).size}, ${reactor.getObservers(action).size}, ${action.maxNumPendingEvents} ${if (action.type != null) ", ${action.type.toText()}" else ""});
            |
        """.trimMargin()
    }

    private fun generateCtor(builtin: BuiltinTrigger) =
        (if (builtin == BuiltinTrigger.STARTUP) "LF_DEFINE_STARTUP_CTOR" else "LF_DEFINE_SHUTDOWN_CTOR") +
                "(${reactor.codeType});\n"


    fun generateCtors(): String {
        var code = reactor.allActions.joinToString(separator = "\n") { generateCtor(it) }
        if (reactor.hasStartup) code += generateCtor(BuiltinTrigger.STARTUP);
        if (reactor.hasShutdown) code += generateCtor(BuiltinTrigger.SHUTDOWN);
        return code;
    }

    private fun generateSelfStruct(builtin: BuiltinTrigger) =
        (if (builtin == BuiltinTrigger.STARTUP) "LF_DEFINE_STARTUP_STRUCT" else "LF_DEFINE_SHUTDOWN_STRUCT") +
                "(${reactor.codeType}, ${reactor.getEffects(builtin).size}, ${reactor.getObservers(builtin).size});\n"

    fun generateSelfStructs(): String {
        var code = reactor.allActions.joinToString(separator = "\n") { generateSelfStruct(it) }
        if (reactor.hasStartup) {
           code += generateSelfStruct(BuiltinTrigger.STARTUP) ;
        }
        if (reactor.hasShutdown) {
            code += generateSelfStruct(BuiltinTrigger.SHUTDOWN) ;
        }
        return code;
    }

    fun generateReactorStructFields(): String {
        var code = reactor.allActions.joinToString(prefix = "// Actions and builtin triggers\n", separator = "\n", postfix = "\n") { "LF_ACTION_INSTANCE(${reactor.codeType}, ${it.name});" }
        if (reactor.hasStartup) code += "LF_STARTUP_INSTANCE(${reactor.codeType});"
        if (reactor.hasShutdown) code += "LF_SHUTDOWN_INSTANCE(${reactor.codeType});"
        return code;
    }

    private fun generateReactorCtorCode(action: Action) = "LF_INITIALIZE_ACTION(${reactor.codeType}, ${action.name}, ${action.minDelay.orZero().toCCode()}, ${action.minSpacing.orZero().toCCode()});"
    private fun generateReactorCtorCodeStartup() = "LF_INITIALIZE_STARTUP(${reactor.codeType});"
    private fun generateReactorCtorCodeShutdown() = "LF_INITIALIZE_SHUTDOWN(${reactor.codeType});"

    fun generateReactorCtorCodes(): String {
        var code = reactor.allActions.joinToString(prefix = "// Initialize actions and builtin triggers\n", separator = "\n", postfix = "\n") { generateReactorCtorCode(it)}
        if(reactor.hasStartup) code += "${generateReactorCtorCodeStartup()}\n"
        if(reactor.hasShutdown) code += "${generateReactorCtorCodeShutdown()}\n"
        return code;
    }
}