package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orZero
import org.lflang.generator.uc.UcTimerGenerator.Companion.codeType
import org.lflang.lf.*

class UcActionGenerator(private val reactor: Reactor) {
    val BuiltinTrigger.codeType
        get(): String =
            if (this == BuiltinTrigger.STARTUP) {
                "${reactor.name}_Startup"
            } else if (this == BuiltinTrigger.SHUTDOWN) {
                "${reactor.name}_Shutdown"
            } else {
                assert(false)
                ""
            }
    val BuiltinTrigger.codeName
        get(): String =
            if (this == BuiltinTrigger.STARTUP) {
                "startup"
            } else if (this == BuiltinTrigger.SHUTDOWN) {
                "shutdown"
            } else {
                unreachable()
            }

    val Action.codeType
        get(): String = "${reactor.name}_Action_$name"

    val Action.bufSize
        get(): Int = 1 // FIXME: This is a parameter/annotation

    private val hasStartup = reactor.reactions.filter {
        it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.STARTUP }.isNotEmpty()
    }.isNotEmpty()
    private val hasShutdown = reactor.reactions.filter {
        it.triggers.filter { it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN }.isNotEmpty()
    }.isNotEmpty()

    fun getEffects(action: Action) = reactor.reactions.filter { it.triggers.filter { it.name == action.name }.isNotEmpty() }
    fun getSources(action: Action) = reactor.reactions.filter { it.effects.filter { it.name == action.name }.isNotEmpty() }

    fun getEffects(builtinTrigger: BuiltinTrigger) =
        reactor.reactions.filter { it.triggers.filter { it.name == builtinTrigger.literal}.isNotEmpty() }

    fun generateSelfStruct(action: Action) = "DEFINE_ACTION_STRUCT(${action.codeType}, ${action.isPhysical}, ${getEffects(action).size}, ${getSources(action).size}, ${action.type.code.toText()}, ${action.bufSize})"
    fun generateCtor(action: Action) = "DEFINE_ACTION_CTOR(${action.codeType}, ${action.isPhysical}, ${getEffects(action).size}, ${getSources(action).size}, ${action.type.code.toText()}, ${action.bufSize})"

    fun generateCtor(builtin: BuiltinTrigger) = with(PrependOperator) {
        """
            |${if (builtin == BuiltinTrigger.STARTUP) "DEFINE_STARTUP_CTOR" else "DEFINE_SHUTDOWN_CTOR"}(${builtin.codeType}, ${getEffects(builtin).size})
        """.trimMargin()
    }

    fun generateCtors(): String {
        var code = reactor.actions.joinToString(separator = "\n") { generateCtor(it) }
        if (hasStartup) code += generateCtor(BuiltinTrigger.STARTUP);
        if (hasShutdown) code += generateCtor(BuiltinTrigger.SHUTDOWN);
        return code;
    }

    fun generateSelfStruct(builtinTrigger: BuiltinTrigger) = with(PrependOperator) {
        """
            |${if (builtinTrigger == BuiltinTrigger.STARTUP) "DEFINE_STARTUP_STRUCT" else "DEFINE_SHUTDOWN_STRUCT"}(${builtinTrigger.codeType}, ${getEffects(builtinTrigger).size})
            """.trimMargin()
    };

    fun generateSelfStructs(): String {
        var code = reactor.actions.joinToString(separator = "\n") { generateSelfStruct(it) }

        if (hasStartup) {
           code += generateSelfStruct(BuiltinTrigger.STARTUP) ;
        }
        if (hasShutdown) {
            code += generateSelfStruct(BuiltinTrigger.SHUTDOWN) ;
        }
        return code;
    }

    fun generateReactorStructFields(): String {
        var code = reactor.actions.joinToString(prefix = "// Actions and builtin triggers\n", separator = "\n", postfix = "\n") { "${it.codeType} ${it.name};" }
        if (hasStartup) code += "${BuiltinTrigger.STARTUP.codeType} startup;"
        if (hasShutdown) code += "${BuiltinTrigger.SHUTDOWN.codeType} shutdown;"
        return code;
    }

    fun generateReactorCtorCode(action: Action)  =  with(PrependOperator) {
        """
            |self->_triggers[trigger_idx++] = (Trigger *) &self->${action.name};
            |${action.codeType}_ctor(&self->${action.name}, &self->super, ${action.minDelay.toCCode()}, ${action.minSpacing.toCCode()});
            |
            """.trimMargin()
    };

    fun generateReactorCtorCode(builtin: BuiltinTrigger)  =  with(PrependOperator) {
        """
            |self->_triggers[trigger_idx++] = (Trigger *) &self->${builtin.codeName};
            |${builtin.codeType}_ctor(&self->${builtin.codeName}, &self->super);
            |
            """.trimMargin()
    };
    fun generateReactorCtorCodes(): String {
        var code = reactor.actions.joinToString(prefix = "// Initialize actions and builtin triggers\n", separator = "\n", postfix = "\n") { generateReactorCtorCode(it)}
        if(hasStartup) code += generateReactorCtorCode(BuiltinTrigger.STARTUP);
        if(hasShutdown) code += generateReactorCtorCode(BuiltinTrigger.SHUTDOWN);
        return code;
    }

}