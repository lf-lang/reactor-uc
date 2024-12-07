package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

class UcReactionGenerator(private val reactor: Reactor) {
    private val Reaction.codeName
        get(): String = name ?: "reaction$index"
    private val Reaction.nameInReactor
        get(): String = "self->${codeName}"
    val Reaction.index
        get(): Int {
            var idx = 0
            for (r in reactor.allReactions) {
                if (this == r) {
                    break
                }
                idx+=1
            }
            return idx
        }

    private val Reaction.ctorDeadlineArg
        get() = if (deadline != null) ", ${deadline.delay.toCCode()}" else ""
    private val Reaction.innerBodyName
        get() = "${reactor.codeType}_${codeName}"
    private val Reaction.innerDeadlineHandlerName
        get() = "${reactor.codeType}_${codeName}_deadline_handler"
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
    private val Reaction.allVariableReferences get() = (effects + sources + triggers.mapNotNull { it as? VarRef }).distinct()
    private val Reaction.allReferencedContainers get() = allVariableReferences.mapNotNull { it.container }.distinct()
    private val reactionsWithDeadline = reactor.allReactions.filter {it.deadline != null}

    // Calculate the total number of effects, considering that we might write to
    // a contained input port
    private val Reaction.totalNumEffects
        get(): Int {
            var res = 0
            for (effect in allUncontainedEffects) {
                val variable = effect.variable
                if (variable is Port) {
                    res += variable.width
                } else {
                    res += 1
                }
            }
            for (effect in allContainedEffects) {
                res += effect.container.width * (effect.variable as Port).width
            }
            return res
        }

    private val Reaction.allContainedEffectsTriggersAndSources
        get() = run {
            val res = mutableMapOf<Instantiation, List<VarRef>>()
            for (varRef in allContainedEffects.plus(allContainedSources).plus(allContainedTriggers)) {
                val varRef = varRef as VarRef
                if (varRef.container!! !in res.keys) {
                    res[varRef.container] = mutableListOf()
                }
                res[varRef.container] = res[varRef.container]!!.plus(varRef)
            }
            res
        }

    private fun TriggerRef.isEffectOf(reaction: Reaction): Boolean = this is VarRef && isEffectOf(reaction)

    private fun Reaction.getInnerParameters(): List<String> =
        allUncontainedTriggers.map {
            if (it is VarRef && it.variable is Port && it.variable.isMultiport) {
                "const ${it.codeType} *${it.name}[], size_t ${it.name}_width"
            } else {
                "const ${it.codeType} *${it.name}"
            }
        } + allUncontainedSources.map {
            if (it.variable is Port && it.variable.isMultiport) {
                "const ${it.codeType} *${it.name}[], size_t ${it.name}_width"
            } else {
                "const ${it.codeType} *${it.name}"
            }
        } + allUncontainedEffects.map {
            if (it.variable is Port && it.variable.isMultiport) {
                "${it.codeType} *${it.name}[], size_t ${it.name}_width"
            } else {
                "${it.codeType} *${it.name}"
            }
        } + allReferencedContainers.map {
            if (it.isBank) {
                "${getContainedStructName(this, it)} ${it.name}[], size_t ${it.name}_width"
            } else {
                "${getContainedStructName(this, it)} ${it.name}"
            }
        }

    private val TriggerRef.codeType
        get() = when {
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.STARTUP -> "${reactor.codeType}_Startup"
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.SHUTDOWN -> "${reactor.codeType}_Shutdown"
            this is VarRef -> codeType
            else -> AssertionError("Unexpected trigger type")
        }

    private val VarRef.codeType
        get() = "${reactor.codeType}_${name}"

    private val TriggerRef.scopeTrigger
        get() = when {
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.STARTUP -> "LF_SCOPE_STARTUP(${reactor.codeType});"
            this is BuiltinTriggerRef && this.type == BuiltinTrigger.SHUTDOWN -> "LF_SCOPE_SHUTDOWN(${reactor.codeType});"
            this is VarRef -> scopeTrigger
            else -> AssertionError("Unexpected trigger type")
        }

    private val VarRef.scopeTrigger
        get() =
            when (val variable = this.variable) {
                is Timer -> "LF_SCOPE_TIMER(${reactor.codeType}, ${name});"
                is Action -> "LF_SCOPE_ACTION_TRIGGER(${reactor.codeType}, ${name});"
                is Port -> {
                    if (variable.width > 1) {
                        "LF_SCOPE_MULTIPORT_TRIGGER(${reactor.codeType}, ${name});"
                    } else {
                        "LF_SCOPE_PORT_TRIGGER(${reactor.codeType}, ${name});"
                    }
                }
                else -> throw AssertionError("Unexpected variable type")
            }

    private val VarRef.scopeEffect
        get() =
            when (val variable = this.variable) {
                is Timer -> "LF_SCOPE_TIMER(${reactor.codeType}, ${name});"
                is Action -> "LF_SCOPE_ACTION_EFFECT(${reactor.codeType}, ${name});"
                is Port -> {
                    if (variable.width > 1) {
                        "LF_SCOPE_MULTIPORT_EFFECT(${reactor.codeType}, ${name});"
                    } else {
                        "LF_SCOPE_PORT_EFFECT(${reactor.codeType}, ${name});"
                    }
                }
                else -> throw AssertionError("Unexpected variable type")
            }
    private val VarRef.fullName: String get() = if (container != null) "${container.name}.${name}" else name

    private val VarRef.isContainedRef: Boolean get() = container != null
    private val TriggerRef.isContainedRef: Boolean get() = this is VarRef && isContainedRef

    private fun VarRef.isEffectOf(reaction: Reaction): Boolean =
        reaction.effects.any { name == it.name && container?.name == it.container?.name }


    private fun getContainedStructName(reaction: Reaction, inst: Instantiation) =
        "${reactor.codeType}_${reaction.codeName}_${inst.name}"

    private fun registerPortSource(varRef: VarRef, port: Port,  reaction: Reaction) =
        if (varRef.container != null) {
            (0..(varRef.container.width-1)).toList().joinToString(separator = "\n") {
                "LF_PORT_REGISTER_SOURCE(self->${varRef.container.name}[${it}].${port.name}, ${reaction.nameInReactor}, ${port.width})"
            }
        } else {
            "LF_PORT_REGISTER_SOURCE(self->${varRef.name}, ${reaction.nameInReactor}, ${port.width});"
        }

    private fun registerSource(varRef: VarRef, reaction: Reaction) =
        when (val variable = varRef.variable) {
            is Action -> "LF_ACTION_REGISTER_SOURCE(self->${varRef.name}, ${reaction.nameInReactor});"
            is Port -> registerPortSource(varRef, variable, reaction)
            else -> throw AssertionError("Unexpected variable type ${varRef}")
        }

    private fun registerEffect(triggerRef: TriggerRef, reaction: Reaction) =
        when {
            triggerRef is BuiltinTriggerRef && triggerRef.type == BuiltinTrigger.STARTUP -> "LF_STARTUP_REGISTER_EFFECT(${reaction.nameInReactor});"
            triggerRef is BuiltinTriggerRef && triggerRef.type == BuiltinTrigger.SHUTDOWN -> "LF_SHUTDOWN_REGISTER_EFFECT(${reaction.nameInReactor});"
            triggerRef is VarRef -> registerEffect(triggerRef, reaction)
            else -> throw AssertionError("Unexpected variable type")
        }

    private fun registerPortEffect(varRef: VarRef, port: Port,  reaction: Reaction) =
        if (varRef.container != null) {
            (0..(varRef.container.width-1)).toList().joinToString(separator = "\n") {
                "LF_PORT_REGISTER_EFFECT(self->${varRef.container.name}[${it}].${port.name}, ${reaction.nameInReactor}, ${port.width})"
            }
        } else {
            "LF_PORT_REGISTER_EFFECT(self->${varRef.name}, ${reaction.nameInReactor}, ${port.width});"
        }

    private fun registerEffect(varRef: VarRef, reaction: Reaction) =
        when (val variable = varRef.variable) {
            is Timer -> "LF_TIMER_REGISTER_EFFECT(self->${varRef.name}, ${reaction.nameInReactor});"
            is Action -> "LF_ACTION_REGISTER_EFFECT(self->${varRef.name}, ${reaction.nameInReactor});"
            is Port -> registerPortEffect(varRef, variable, reaction)
            else -> throw AssertionError("Unexpected variable type")
        }

    private fun registerPortObserver(varRef: VarRef, port: Port,  reaction: Reaction) =
        if (varRef.container != null) {
            (0..(varRef.container.width-1)).toList().joinToString(separator = "\n") {
                "LF_PORT_REGISTER_OBSERVER(self->${varRef.container.name}[${it}].${port.name}, ${reaction.nameInReactor}, ${port.width})"
            }
        } else {
            "LF_PORT_REGISTER_OBSERVER(self->${varRef.name}, ${reaction.nameInReactor}, ${port.width});"
        }
    private fun registerObserver(varRef: VarRef, reaction: Reaction) =
        when (val variable = varRef.variable) {
            is Action -> "LF_ACTION_REGISTER_OBSERVER(self->${varRef.name}, ${reaction.codeName});"
            is Port -> registerPortObserver(varRef, variable, reaction)
            else -> throw AssertionError("Unexpected variable type")
        }

    private fun generateReactionCtor(reaction: Reaction) =
        "LF_DEFINE_REACTION_CTOR(${reactor.codeType}, ${reaction.codeName}, ${reaction.index} ${reaction.ctorDeadlineArg});"

    private fun generateSelfStruct(reaction: Reaction) =
        "LF_DEFINE_REACTION_STRUCT(${reactor.codeType}, ${reaction.codeName}, ${reaction.totalNumEffects});"

    fun generateSelfStructs() =
        reactor.allReactions.joinToString(
            separator = "\n",
            prefix = "// Reaction structs\n",
            postfix = "\n"
        ) { generateSelfStruct(it) }

    fun generateReactorStructFields() =
        reactor.allReactions.joinToString(
            separator = "\n",
            postfix = "\n"
        ) {
            "LF_REACTION_INSTANCE(${reactor.codeType}, ${it.codeName});"
        }

    fun generateReactionCtors() =
        reactor.allReactions.joinToString(
            separator = "\n",
            prefix = "// Reaction constructors\n",
            postfix = "\n"
        ) { generateReactionCtor(it) }

    fun generateReactionBodies() =
        reactor.allReactions.joinToString(
            separator = "\n",
            prefix = "// Reaction bodies\n",
            postfix = "\n"
        ) { generateReactionBody(it) }

    fun generateReactionInnerBodyDeclarations() =
        reactor.allReactions.joinToString(
            separator = "\n",
            prefix = "// Reaction inner bodies\n",
            postfix = "\n"
        ) { "${generateReactionInnerBodyDeclaration(it)};" }

    fun generateReactionInnerArgumentStructs() =
        reactor.allReactions.joinToString(
            separator = "\n",
            prefix = "// Contained reactor argument structs\n",
            postfix = "\n"
        ) {
            generateReactionInnerArgumentStructs(it)
        }

    fun generateReactionInnerArgumentStructs(reaction: Reaction) =
        reaction.allContainedEffectsTriggersAndSources.toList()
            .joinToString(separator = "\n") {
                generateContainedReactorDefinition(reaction, it.second, it.first)
            }

    fun generateReactionInnerBodies() =
        reactor.allReactions.joinToString(
            separator = "\n",
            prefix = "// Reaction inner bodies\n",
            postfix = "\n"
        ) { generateReactionInnerBodyDefinition(it) }

    fun generateReactionDeadlineHandlers() =
        reactionsWithDeadline.joinToString(
            separator = "\n",
            prefix = "// Reaction deadline handlers\n",
            postfix="\n"
        ) { generateReactionDeadlineHandler(it) }

    private fun generateReactionScope(reaction: Reaction) = with(PrependOperator) {
        """ |// Bring self struct, environment, triggers, effects and sources into scope.
            |  LF_SCOPE_SELF(${reactor.codeType});
            |  LF_SCOPE_ENV();
         ${"|  "..generateTriggersEffectsAndSourcesInScope(reaction)}
         ${"|  "..generateContainedTriggersAndSourcesInScope(reaction)}
        """.trimMargin()
    }

    private fun generateReactionInnerBodyDeclaration(reaction: Reaction) = with(PrependOperator) {
        "void ${reaction.innerBodyName}(${reactor.codeType} *self, Environment *env ${reaction.getInnerParameters().joinToString(prefix = ",")})"
    }

    private fun generateReactionInnerBodyDefinition(reaction: Reaction): String {
        if (reaction.code == null) return ""

        with(PrependOperator) {
            return """|
           |${generateReactionInnerBodyDeclaration(reaction)} {
           |  // User written reaction body
        ${"|  "..reaction.code.toText()}
           |}
        """.trimMargin()
        }
    }

    private fun generateReactionInnerBodyCall(reaction: Reaction): String {
        with(reaction) {
            val parameters = allUncontainedTriggers.map { "${it.name} ${if (it is VarRef && it.variable is Input && it.variable.isMultiport) ", ${it.name}_width" else ""}"} +
                allUncontainedSources.map { "${it.name} ${if (it.variable is Input && it.variable.isMultiport) ", ${it.name}_width" else ""}"} +
                allUncontainedEffects.map { "${it.name} ${if (it.variable is Output && it.variable.isMultiport) ", ${it.name}_width" else ""}"} +
                allReferencedContainers.map {
                    "${it.name} ${if (it.isBank) ", ${it.name}_width" else ""}"
                }
            return "${innerBodyName}(self, env ${parameters.joinToString(prefix = ",")});"
            }
    }

    private fun generateReactionDeadlineHandler(reaction: Reaction) = with(PrependOperator) {
        """
            |LF_DEFINE_REACTION_DEADLINE_HANDLER(${reactor.codeType}, ${reaction.codeName}) {
         ${"|  "..generateReactionScope(reaction)}
            |  // Start of user-witten reaction body
         ${"|  "..reaction.deadline.code.toText()}
            |}
        """.trimMargin()
    }

    private fun generateReactionBody(reaction: Reaction) = with(PrependOperator) {
        """
            |LF_DEFINE_REACTION_BODY(${reactor.codeType}, ${reaction.codeName}) {
         ${"|  "..generateReactionScope(reaction)}
            |  // Call reaction inner body
         ${"|  "..generateReactionInnerBodyCall(reaction)}
            |}
        """.trimMargin()
    }

    private fun generateContainedTriggerInScope(trigger: VarRef) =
        if (trigger.variable.isMultiport) {
            "LF_MULTIPORT_PTR_INSTANCE(${trigger.container.reactor.codeType}, ${trigger.name}, ${(trigger.variable as Port).width});" // FIXME: What about this?
        } else {
            "LF_PORT_PTR_INSTANCE(${trigger.container.reactor.codeType}, ${trigger.name});" // FIXME: What about this?
        }

    private fun generateContainedMultiportTriggerFieldInit(instName: String, containerName: String, trigger: VarRef, port: Port) = with(PrependOperator) {
        """|
           |${instName}.${trigger.name}_width = ${port.width};
           |for (int j = 0; j<${port.width}; j++) {
           |  ${instName}.${trigger.name}[j] = ${containerName}.${trigger.name}[j]; 
           |}
        """.trimMargin()
    }

    private fun generateContainedTriggerFieldInit(instName: String, trigger: VarRef) =
        if (trigger.variable.isMultiport) {
            generateContainedMultiportTriggerFieldInit(instName, "&self->${trigger.container.name}[0]", trigger, trigger.variable as Port)
        } else {
            "${instName}.${trigger.name} = self->${trigger.container.name}->${trigger.name};"
        }

    private fun generateContainedBankTriggerFieldInit(instName: String, trigger: VarRef) =
        if (trigger.variable.isMultiport) {
            generateContainedMultiportTriggerFieldInit("${instName}[i]", "&self->${trigger.container.name}[i]", trigger, trigger.variable as Port)
        } else {
            "${instName}[i].${trigger.name} = &self->${trigger.container.name}[i].${trigger.name};"
        }

    private fun generateContainedReactorDefinition(reaction: Reaction, triggers: List<VarRef>, inst: Instantiation) = with(PrependOperator) {
        """|
           |// Generated struct providing access to ports of child reactor `${inst.name}`
           |typedef struct {
        ${"|  "..triggers.joinToString(separator = "\n") { generateContainedTriggerInScope(it) }}
           |} ${getContainedStructName(reaction, inst)};
        """.trimMargin()
    }

    private fun generateContainedReactorStructDeclaration(reaction: Reaction, triggers: List<VarRef>, inst: Instantiation) = with(PrependOperator) {
        """|
           | ${getContainedStructName(reaction, inst)} ${inst.name};
        ${"|"..triggers.joinToString(separator = "\n") {generateContainedTriggerFieldInit("${inst.name}", it)}}
        """.trimMargin()
    }

    private fun generateContainedBankStructDeclaration(reaction: Reaction, triggers: List<VarRef>, inst: Instantiation) = with(PrependOperator) {
        """
           |${getContainedStructName(reaction, inst)} ${inst.name}[${inst.width}];
           |size_t ${inst.name}_width = ${inst.width};
           |for (int i = 0; i<${inst.width}; i++) {
        ${"|  "..triggers.joinToString(separator = "\n") {generateContainedBankTriggerFieldInit( "${inst.name}", it)}}
           |}
        """.trimMargin()
    }

    private fun generateTriggersEffectsAndSourcesInScope(reaction: Reaction) =
        reaction.allUncontainedTriggers.plus(reaction.allUncontainedSources).filter{it !in reaction.allUncontainedEffects}.distinct().joinToString(prefix = "\n", separator = "\n", postfix="\n"){
            it.scopeTrigger.toString()
        } + reaction.allUncontainedEffects.joinToString(separator = "\n") { with(PrependOperator) { it.scopeEffect.toString() } }

    private fun generateContainedTriggersAndSourcesInScope(reaction: Reaction) =
        reaction.allContainedEffectsTriggersAndSources.toList()
            .joinToString(separator = "\n") {
                if (it.first.width > 1) {
                    generateContainedBankStructDeclaration(reaction, it.second, it.first)
                } else {
                    generateContainedReactorStructDeclaration(reaction, it.second, it.first)
                }
            }

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

    private fun generateReactorCtorCode(reaction: Reaction) = with(PrependOperator) {
        """
            |LF_INITIALIZE_REACTION(${reactor.codeType}, ${reaction.codeName});
        ${" |  "..generateTriggerRegisterEffect(reaction)}
        ${" |  "..generateTriggerRegisterSource(reaction)}
        ${" |  "..generateTriggerRegisterObserver(reaction)}
            """.trimMargin()
    }

    fun generateReactorCtorCodes() =
        reactor.allReactions.joinToString(
            separator = "\n",
            prefix = "// Initialize Reactions \n"
        ) { generateReactorCtorCode(it) }

    /**
     * Returns all the reactions triggered by the Output port which are contained in the parent reactor.
     * This is used for reactions triggered by contained output ports.
     */
    fun getParentReactionEffectsOfOutput(inst: Instantiation, port: Output): List<Reaction> {
        val res = mutableListOf<Reaction>();
        for (reaction in reactor.allReactions) {
            if (reaction.allContainedTriggers.filter { it is VarRef && it.variable == port }.isNotEmpty()) {
                res.add(reaction)
            }
        }
        return res
    }

    fun getParentReactionObserversOfOutput(inst: Instantiation, port: Output): List<Reaction> {
        val res = mutableListOf<Reaction>();
        for (reaction in reactor.allReactions) {
            if (reaction.allContainedSources.filter { it is VarRef && it.variable == port }.isNotEmpty()) {
                res.add(reaction)
            }
        }
        return res
    }

    fun getParentReactionSourcesOfInput(inst: Instantiation, port: Input): List<Reaction> {
        val res = mutableListOf<Reaction>();
        for (reaction in reactor.allReactions) {
            if (reaction.allContainedEffects.filter { it is VarRef && it.variable == port }.isNotEmpty()) {
                res.add(reaction)
            }
        }
        return res
    }
}
