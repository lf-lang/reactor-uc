package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

class UcReactionGenerator(private val reactor: Reactor) {
    private val Reaction.codeName
        get(): String = name ?: "reaction$index"
    val Reaction.index
        get(): Int = priority - 1

    private val Reaction.allUncontainedTriggers
        get() = triggers.filterNot { it.isEffectOf(this) || it.isContainedRef }
    private val Reaction.allUncontainedEffects
        get() = effects.filterNot { it.isContainedRef }
    private val Reaction.allUncontainedSources
        get() = sources.filterNot { it.isContainedRef }

    private val Reaction.allContainedEffects
        get() = effects.filter { it.isContainedRef }
    private val Reaction.allContainedTriggers
        get() = triggers.filter { !it.isEffectOf(this) && it.isContainedRef }
    private val Reaction.allContainedSources
        get() = sources.filter { !it.isEffectOf(this) && it.isContainedRef }

    private val Reaction.allContainedEffectsTriggersAndSources
        get() = run {
            val res = mutableMapOf<Instantiation, List<VarRef>>()
            for (effect in allContainedEffects) {
                res.getOrPut(effect.container!!) { mutableListOf(effect) }.plus(effect)
            }
            for (trigger in allContainedTriggers) {
                res.getOrPut((trigger as VarRef).container!!) { mutableListOf(trigger) }.plus(trigger)
            }
            for (source in allContainedSources) {
                res.getOrPut((source as VarRef).container!!) { mutableListOf(source) }.plus(source)
            }
            res
        }

    private fun TriggerRef.isEffectOf(reaction: Reaction): Boolean = this is VarRef && isEffectOf(reaction)

    private val TriggerRef.scope
        get() = when {
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.STARTUP -> "SCOPE_STARTUP(${reactor.codeType});"
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.SHUTDOWN -> "SCOPE_SHUTDOWN(${reactor.codeType});"
            this is VarRef -> scope
            else -> AssertionError("Unexpected trigger type")
        }

    private val VarRef.scope
        get() =
            when (val variable = this.variable) {
                is Timer -> "SCOPE_TIMER(${reactor.codeType}, ${name});"
                is Action -> "SCOPE_ACTION(${reactor.codeType}, ${name});"
                is Output -> "SCOPE_PORT(${reactor.codeType}, ${name});"
                is Input -> "SCOPE_PORT(${reactor.codeType}, ${name});"
                else -> throw AssertionError("Unexpected variable type")
            }
    private val VarRef.fullName: String get() = if (container != null) "${container.name}.${name}" else name

    private val VarRef.isContainedRef: Boolean get() = container != null
    private val TriggerRef.isContainedRef: Boolean get() = this is VarRef && isContainedRef

    private fun VarRef.isEffectOf(reaction: Reaction): Boolean =
        reaction.effects.any { name == it.name && container?.name == it.container?.name }


    private fun registerSource(varRef: VarRef, reaction: Reaction) =
        when (val variable = varRef.variable) {
            is Action -> "ACTION_REGISTER_SOURCE(${varRef.name}, ${reaction.codeName});"
            is Output -> "PORT_REGISTER_SOURCE(${varRef.fullName}, ${reaction.codeName});"
            is Input -> "PORT_REGISTER_SOURCE(${varRef.fullName}, ${reaction.codeName});"
            else -> throw AssertionError("Unexpected variable type ${varRef}")
        }

    private fun registerEffect(triggerRef: TriggerRef, reaction: Reaction) =
        when {
            triggerRef is BuiltinTriggerRef && triggerRef.type == BuiltinTrigger.STARTUP -> "STARTUP_REGISTER_EFFECT(${reaction.codeName});"
            triggerRef is BuiltinTriggerRef && triggerRef.type == BuiltinTrigger.SHUTDOWN -> "SHUTDOWN_REGISTER_EFFECT(${reaction.codeName});"
            triggerRef is VarRef -> registerEffect(triggerRef, reaction)
            else -> throw AssertionError("Unexpected variable type")
        }

    private fun registerEffect(varRef: VarRef, reaction: Reaction) =
        when (val variable = varRef.variable) {
            is Timer -> "TIMER_REGISTER_EFFECT(${varRef.name}, ${reaction.codeName});"
            is Action -> "ACTION_REGISTER_EFFECT(${varRef.name}, ${reaction.codeName});"
            is Output -> "PORT_REGISTER_EFFECT(${varRef.fullName}, ${reaction.codeName});"
            is Input -> "PORT_REGISTER_EFFECT(${varRef.fullName}, ${reaction.codeName});"
            else -> throw AssertionError("Unexpected variable type")
        }

    private fun registerObserver(varRef: VarRef, reaction: Reaction) =
        when (val variable = varRef.variable) {
            is Action -> "ACTION_REGISTER_OBSERVER(${varRef.name}, ${reaction.codeName});"
            is Output -> "PORT_REGISTER_OBSERVER(${varRef.fullName}, ${reaction.codeName});"
            is Input -> "PORT_REGISTER_OBSERVER(${varRef.fullName}, ${reaction.codeName});"
            else -> throw AssertionError("Unexpected variable type")
        }

    private fun generateReactionCtor(reaction: Reaction) =
        "DEFINE_REACTION_CTOR(${reactor.codeType}, ${reaction.codeName}, ${reaction.index});"

    private fun generateSelfStruct(reaction: Reaction) =
        "DEFINE_REACTION_STRUCT(${reactor.codeType}, ${reaction.codeName}, ${reaction.effects.size});"

    fun generateSelfStructs() =
        reactor.reactions.joinToString(
            separator = "\n",
            prefix = "// Reaction structs\n",
            postfix = "\n"
        ) { generateSelfStruct(it) }

    fun generateReactorStructFields() =
        reactor.reactions.joinToString(
            separator = "\n",
            postfix = "\n"
        ) {
            "REACTION_INSTANCE(${reactor.codeType}, ${it.codeName});"
        }

    fun generateReactionCtors() =
        reactor.reactions.joinToString(
            separator = "\n",
            prefix = "// Reaction constructors\n",
            postfix = "\n"
        ) { generateReactionCtor(it) }

    fun generateReactionBodies() =
        reactor.reactions.joinToString(
            separator = "\n",
            prefix = "// Reaction bodies\n",
            postfix = "\n"
        ) { generateReactionBody(it) }

    private fun generateReactionBody(reaction: Reaction) = with(PrependOperator) {
        """
            |DEFINE_REACTION_BODY(${reactor.codeType}, ${reaction.codeName}) {
            |  // Bring self struct, environment, triggers, effects and sources into scope.
            |  SCOPE_SELF(${reactor.codeType});
            |  SCOPE_ENV();
         ${"|  "..generateTriggersEffectsAndSourcesInScope(reaction)}
         ${"|  "..generateContainedTriggersAndSourcesInScope(reaction)}
            |  // Start of user-witten reaction body
         ${"|  "..reaction.code.toText()}
            |}
        """.trimMargin()
    }

    private fun generateContainedTriggerInScope(trigger: VarRef) =
        "PORT_PTR_INSTANCE(${trigger.container.reactor.codeType}, ${trigger.name});"

    private fun generateContainedTriggerFieldInit(trigger: VarRef) =
        ".${trigger.name} = &self->${trigger.container.name}.${trigger.name}"

    private fun generateContainedReactorScope(triggers: List<VarRef>, inst: Instantiation) = with(PrependOperator) {
        """|
           |// Generated struct providing access to ports of child reactor `${inst.name}`
           |struct _${inst.reactor.codeType}_${inst.name} {
        ${"|  "..triggers.joinToString { generateContainedTriggerInScope(it) }}
           |};
           |struct _${inst.reactor.codeType}_${inst.name} ${inst.name} = {${
            triggers.joinToString(separator = ", ") {
                generateContainedTriggerFieldInit(
                    it
                )
            }
        }};
        """.trimMargin()
    }

    private fun generateTriggersEffectsAndSourcesInScope(reaction: Reaction) =
        reaction.allUncontainedTriggers.plus(reaction.allUncontainedEffects).plus(reaction.allUncontainedSources)
            .joinToString(separator = "\n") { with(PrependOperator) { it.scope.toString() } }

    private fun generateContainedTriggersAndSourcesInScope(reaction: Reaction) =
        reaction.allContainedEffectsTriggersAndSources.toList()
            .joinToString(separator = "\n") { generateContainedReactorScope(it.second, it.first) }

    private fun generateTriggerRegisterEffect(reaction: Reaction) =
        reaction.triggers.joinToString(
            separator = "\n",
        ) { registerEffect(it, reaction) }

    private fun generateTriggerRegisterObserver(reaction: Reaction) =
        reaction.sources.joinToString(
            separator = "\n",
        ) { registerObserver(it, reaction) }

    private fun generateTriggerRegisterSource(reaction: Reaction) =
        reaction.effects.joinToString(
            separator = "\n",
        ) { registerSource(it, reaction) }

    private fun generateRegisterEffects(reaction: Reaction) =
        reaction.effects.joinToString(
            separator = "\n",
        ) {
            "REACTION_REGISTER_EFFECT(${reaction.codeName}, ${it.fullName});"
        };

    private fun generateReactorCtorCode(reaction: Reaction) = with(PrependOperator) {
        """
            |INITIALIZE_REACTION(${reactor.codeType}, ${reaction.codeName});
        ${" |  "..generateTriggerRegisterEffect(reaction)}
        ${" |  "..generateTriggerRegisterSource(reaction)}
        ${" |  "..generateTriggerRegisterObserver(reaction)}
        ${" |  "..generateRegisterEffects(reaction)}
            """.trimMargin()
    }

    fun generateReactorCtorCodes() =
        reactor.reactions.joinToString(
            separator = "\n",
            prefix = "// Initialize Reactions \n"
        ) { generateReactorCtorCode(it) }

    /**
     * Returns all the reactions triggered by the Output port which are contained in the parent reactor.
     * This is used for reactions triggered by contained output ports.
     */
    fun getParentReactionEffectsOfOutput(inst: Instantiation, port: Output): List<Reaction> {
        val res = mutableListOf<Reaction>();
        for (reaction in reactor.reactions) {
            if (reaction.allContainedTriggers.filter { it is VarRef && it.variable == port }.isNotEmpty()) {
                res.add(reaction)
            }
        }
        return res
    }

    fun getParentReactionObserversOfOutput(inst: Instantiation, port: Output): List<Reaction> {
        val res = mutableListOf<Reaction>();
        for (reaction in reactor.reactions) {
            if (reaction.allContainedSources.filter { it is VarRef && it.variable == port }.isNotEmpty()) {
                res.add(reaction)
            }
        }
        return res
    }

    fun getParentReactionSourcesOfInput(inst: Instantiation, port: Input): List<Reaction> {
        val res = mutableListOf<Reaction>();
        for (reaction in reactor.reactions) {
            if (reaction.allContainedEffects.filter { it is VarRef && it.variable == port }.isNotEmpty()) {
                res.add(reaction)
            }
        }
        return res
    }

}
