package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orZero
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasStartup
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasShutdown
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

class UcActionGenerator(private val reactor: Reactor) {
    val Action.bufSize
        get(): Int = 12 // FIXME: This should be annotated in the LF code

    /** Returns the C Enum representing the type of action.*/
    val Action.actionType
        get(): String = if (isPhysical) "PHYSICAL_ACTION" else "LOGICAL_ACTION"


    // FIXME: These are quite general functions and should probably be refactored so we dont reimplement for each specific type
    fun getEffects(action: Action) = reactor.reactions.filter { it.triggers.filter { it.name == action.name }.isNotEmpty() }
    fun getSources(action: Action) = reactor.reactions.filter { it.effects.filter { it.name == action.name }.isNotEmpty() }
    fun getObservers(action: Action) = reactor.reactions.filter{it.sources.filter {it.name == action.name}.isNotEmpty()}

    fun getEffects(builtinTrigger: BuiltinTrigger) =
        reactor.reactions.filter { it.triggers.filter { it.name == builtinTrigger.literal}.isNotEmpty() }
    fun getObservers(builtinTrigger: BuiltinTrigger) =
        reactor.reactions.filter { it.sources.filter { it.name == builtinTrigger.literal}.isNotEmpty() }

    // FIXME: COde replication
    fun generateSelfStruct(action: Action) =
        if (action.type != null) "DEFINE_ACTION_STRUCT(${reactor.codeType}, ${action.name}, ${action.actionType}, ${getEffects(action).size}, ${getSources(action).size}, ${getObservers(action).size}, ${action.bufSize}, ${action.type.toText()});\n"
        else "DEFINE_ACTION_STRUCT_VOID(${reactor.codeType}, ${action.name}, ${action.actionType}, ${getEffects(action).size}, ${getSources(action).size}, ${getObservers(action).size}, ${action.bufSize});\n"

    // FIXME: Code replication
    fun generateCtor(action: Action) =
        if (action.type != null) "DEFINE_ACTION_CTOR(${reactor.codeType}, ${action.name}, ${action.actionType}, ${getEffects(action).size}, ${getSources(action).size}, ${getObservers(action).size}, ${action.bufSize}, ${action.type.toText()});\n"
        else "DEFINE_ACTION_CTOR_VOID(${reactor.codeType}, ${action.name}, ${action.actionType}, ${getEffects(action).size}, ${getSources(action).size}, ${action.bufSize});\n"

    fun generateCtor(builtin: BuiltinTrigger) = with(PrependOperator) {
        """
            |${if (builtin == BuiltinTrigger.STARTUP) "DEFINE_STARTUP_CTOR" else "DEFINE_SHUTDOWN_CTOR"}(${reactor.codeType});
            |
        """.trimMargin()
    }

    fun generateCtors(): String {
        var code = reactor.actions.joinToString(separator = "\n") { generateCtor(it) }
        if (reactor.hasStartup) code += generateCtor(BuiltinTrigger.STARTUP);
        if (reactor.hasShutdown) code += generateCtor(BuiltinTrigger.SHUTDOWN);
        return code;
    }

    fun generateSelfStruct(builtinTrigger: BuiltinTrigger) = with(PrependOperator) {
        """
            |${if (builtinTrigger == BuiltinTrigger.STARTUP) "DEFINE_STARTUP_STRUCT" else "DEFINE_SHUTDOWN_STRUCT"}(${reactor.codeType}, ${getEffects(builtinTrigger).size}, ${getObservers(builtinTrigger).size});
            """.trimMargin()
    };

    fun generateSelfStructs(): String {
        var code = reactor.actions.joinToString(separator = "\n") { generateSelfStruct(it) }
        if (reactor.hasStartup) {
           code += generateSelfStruct(BuiltinTrigger.STARTUP) ;
        }
        if (reactor.hasShutdown) {
            code += generateSelfStruct(BuiltinTrigger.SHUTDOWN) ;
        }
        return code;
    }

    fun generateReactorStructFields(): String {
        var code = reactor.actions.joinToString(prefix = "// Actions and builtin triggers\n", separator = "\n", postfix = "\n") { "ACTION_INSTANCE(${reactor.codeType}, ${it.name});" }
        if (reactor.hasStartup) code += "STARTUP_INSTANCE(${reactor.codeType});"
        if (reactor.hasShutdown) code += "SHUTDOWN_INSTANCE(${reactor.codeType});"
        return code;
    }

    fun generateReactorCtorCode(action: Action) = "INITIALIZE_ACTION(${reactor.codeType}, ${action.name}, ${action.minDelay.orZero().toCCode()});"
    fun generateReactorCtorCodeStartup() = "INITIALIZE_STARTUP(${reactor.codeType});"
    fun generateReactorCtorCodeShutdown() = "INITIALIZE_SHUTDOWN(${reactor.codeType});"

    fun generateReactorCtorCodes(): String {
        var code = reactor.actions.joinToString(prefix = "// Initialize actions and builtin triggers\n", separator = "\n", postfix = "\n") { generateReactorCtorCode(it)}
        if(reactor.hasStartup) code += "${generateReactorCtorCodeStartup()}\n"
        if(reactor.hasShutdown) code += "${generateReactorCtorCodeShutdown()}\n"
        return code;
    }
}