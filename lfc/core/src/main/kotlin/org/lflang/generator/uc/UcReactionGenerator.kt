package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

class UcReactionGenerator(
    private val reactor: Reactor,
    private val portGenerator: UcPortGenerator
) {
        val Reaction.codeType
            get(): String = name ?: "${reactor.name}_Reaction_$priority"
        val Reaction.codeName
            get(): String = name ?: "${reactor.name}_reaction_$priority"
        val Reaction.bodyFuncName
            get(): String = name ?: "${reactor.name}_Reaction_${priority}_body"

    private val VarRef.codeType
        get() =
            when (val variable = this.variable) {
                is Timer  -> "${reactor.name}_Timer_${name}"
                is Action -> "${reactor.name}_Action_${name}"
                is Output -> "${reactor.name}_Output_${name}"
                is Input -> "${reactor.name}_Input_${name}"
                else      -> AssertionError("Unexpected variable type")
            }

    private val VarRef.typeMacro
        get() =
            when (val variable = this.variable) {
                is Timer  -> "TIMER"
                is Action -> "ACTION"
                is Output -> "OUTPUT"
                is Input -> "INPUT"
                else      -> AssertionError("Unexpected variable type")
            }

    private val TriggerRef.codeType
        get() = when {
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.STARTUP  -> "${reactor.name}_Startup"
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.SHUTDOWN -> "${reactor.name}_Shutdown"
            this is VarRef                                                    -> codeType
            else                                                              -> AssertionError("Unexpected trigger type")
        }

    private val TriggerRef.typeMacro
        get() = when {
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.STARTUP  -> "STARTUP"
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.SHUTDOWN -> "SHUTDOWN"
            this is VarRef                                                    -> typeMacro
            else                                                              -> AssertionError("Unexpected trigger type")
        }

    fun generateEffectsField(reaction: Reaction) =
        if (reaction.allUncontainedEffects.size > 0) "Trigger *_effects[${reaction.allUncontainedEffects.size}];" else "\n"

    fun generateEffectsFieldPtr(reaction: Reaction) = if (reaction.allUncontainedEffects.size > 0) "self->_effects" else "NULL"

    fun generateReactionCtor(reaction: Reaction) = with(PrependOperator) {
        """
        |static void ${reaction.codeType}_ctor(${reaction.codeType} *self, Reactor *parent) {
        |   Reaction_ctor(&self->super, parent, ${reaction.bodyFuncName}, ${generateEffectsFieldPtr(reaction)}, ${reaction.allUncontainedEffects.size}, ${reaction.priority-1});
        |}
       """.trimMargin()
    };

    fun generateSelfStruct(reaction: Reaction) = with(PrependOperator) {
        """
            |typedef struct {
            |  Reaction super;
        ${" |  "..generateEffectsField(reaction)}
            |} ${reaction.codeType};
            """.trimMargin()
    };

    fun generateSelfStructs() =
        reactor.reactions.joinToString(
            separator = "\n",
            prefix = "// Reaction structs\n",
            postfix = "\n"
        ) { generateSelfStruct(it) };

    fun generateReactorStructFields() =
        reactor.reactions.joinToString(
            separator = "\n",
            postfix = "\n"
        ) {
            with(PrependOperator) {
                """
                |${it.codeType} ${it.codeName};
            """.trimMargin()
            }
        };

    fun generateReactionCtors() =
        reactor.reactions.joinToString(
            separator = "\n",
            prefix = "// Reaction constructors\n",
            postfix = "\n"
        ) { generateReactionCtor(it) };

    fun generateReactionBodies() =
        reactor.reactions.joinToString(
            separator = "\n",
            prefix = "// Reaction bodies\n",
            postfix = "\n"
        ) { generateReactionBody(it) };

    fun generateReactionBody(reaction: Reaction) = with(PrependOperator) {
        """
            |static void ${reaction.bodyFuncName}(Reaction *_self) {
            |   // Bring expected variable names into scope
            |   ${reactor.codeType} *self = (${reactor.codeType} *) _self->parent;
            |   (void)self;
            |   Environment *env = self->super.env;
            |   (void)env;
            |   ${generateTriggersInScope(reaction)}
            |   // Start of user-witten reaction body
            |   ${reaction.code.toText()}
            |   // End of user-written reaction body
            |}
        """.trimMargin()
    }

    fun generateTriggersInScope(reaction: Reaction) =
        reaction.allUncontainedTriggers.plus(reaction.allUncontainedEffects).joinToString(separator = "\n"){
                """
                    |${it.codeType} *${it.name} = &self->${it.name};
                    |(void) ${it.name};
                """.trimMargin()
            };

    fun generateTriggerRegisterEffect(reaction: Reaction) =
        reaction.allUncontainedTriggers.joinToString(
            separator = "\n",
        ) {"${it.typeMacro}_REGISTER_EFFECT(self->${it.name}, self->${reaction.codeName});"};

    fun generateTriggerRegisterSource(reaction: Reaction) =
        reaction.allUncontainedEffects.joinToString(
            separator = "\n",
            ) {"${it.typeMacro}_REGISTER_SOURCE(self->${it.name}, self->${reaction.codeName});"};

    fun generateRegisterEffects(reaction: Reaction) =
        reaction.allUncontainedEffects.joinToString(
            separator = "\n",
        ) {
            "REACTION_REGISTER_EFFECT(self->${reaction.codeName}, self->${it.name});"
        };

    fun generateReactorCtorCode(reaction: Reaction) = with(PrependOperator) {
        """
            |self->_reactions[${reaction.priority - 1}] = &self->${reaction.codeName}.super;
            |${reaction.codeType}_ctor(&self->${reaction.codeName}, &self->super);
            |// Register all triggers of this reaction.
        ${" |"..generateTriggerRegisterEffect(reaction)}
        ${" |"..generateTriggerRegisterSource(reaction)}
        ${" |"..generateRegisterEffects(reaction)}
            """.trimMargin()
    };

    fun generateReactorCtorCodes() =
        reactor.reactions.joinToString(separator = "\n", prefix = "// Initialize Reactions \n") { generateReactorCtorCode(it) }

    private val VarRef.isContainedRef: Boolean get() = container != null
        private val TriggerRef.isContainedRef: Boolean get() = this is VarRef && isContainedRef

    private fun VarRef.isEffectOf(reaction: Reaction): Boolean =
        reaction.effects.any { name == it.name && container?.name == it.container?.name }

    private fun TriggerRef.isEffectOf(reaction: Reaction): Boolean = this is VarRef && isEffectOf(reaction)

    private val Reaction.allUncontainedTriggers get() = triggers.filterNot { it.isEffectOf(this) || it.isContainedRef }
    private val Reaction.allUncontainedEffects get() = effects.filterNot { it.isContainedRef };

}
