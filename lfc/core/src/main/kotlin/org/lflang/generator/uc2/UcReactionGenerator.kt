package org.lflang.generator.uc2

import org.lflang.generator.PrependOperator
import org.lflang.ir.*

class UcReactionGenerator(private val reactor: Reactor) {

  private val reactionsWithDeadline = reactor.reactions.filter { it.deadline != null }
  private val reactionsWithTardyViolationHandler =
      reactor.reactions.filter { it.tardy != null }


  private val TriggerRef.scope
    get() =
        when (this.resolve()) {
          is Startup -> "LF_SCOPE_STARTUP(${reactor.codeType});"
          is Shutdown -> "LF_SCOPE_SHUTDOWN(${reactor.codeType});"
          is Timer -> "LF_SCOPE_TIMER(${reactor.codeType}, ${(this.resolve() as Timer).lfName});"
          is Action -> "LF_SCOPE_ACTION(${reactor.codeType}, ${(this.resolve() as Action).lfName});"
          is Port -> {
            if ((this.resolve() as Port).width > 1) {
              "LF_SCOPE_MULTIPORT(${reactor.codeType}, ${(this.resolve() as Port).lfName});"
            } else {
              "LF_SCOPE_PORT(${reactor.codeType}, ${(this.resolve() as Port).lfName});"
            }
          }
          else -> throw AssertionError("Unexpected variable type")
        }

  private fun registerPortSource(triggerRef: TriggerRef, port: Port, reaction: Reaction) : String =
      when (triggerRef) {
          is VariableNameTriggerRef -> {
              "LF_PORT_REGISTER_SOURCE(self->${triggerRef.variable.lfName}, ${reaction.nameInReactor}, ${port.width});"
          }
          is VariableContainedTriggerRef -> {
              (0..<triggerRef.container.codeWidth).toList().joinToString(separator = "\n") {
                  "LF_PORT_REGISTER_SOURCE(self->${triggerRef.container.lfName}[${it}].${port.lfName}, ${reaction.nameInReactor}, ${port.width})"
              }
          }
          else -> throw AssertionError("Unexpected variable type $triggerRef")
      }

  private fun registerSource(triggerRef: TriggerRef, reaction: Reaction) : String =
      when (val variable = triggerRef.resolve()) {
        is Action -> "LF_ACTION_REGISTER_SOURCE(self->${variable.lfName}, ${reaction.nameInReactor});"
        is Port -> registerPortSource(triggerRef, variable, reaction)
        else -> throw AssertionError("Unexpected variable type $triggerRef")
      }

  private fun registerEffect(triggerRef: TriggerRef, reaction: Reaction) : String =
      when {
        triggerRef is StartupTriggerRef -> "LF_STARTUP_REGISTER_EFFECT(${reaction.nameInReactor});"
        triggerRef is ShutdownTriggerRef ->
            "LF_SHUTDOWN_REGISTER_EFFECT(${reaction.nameInReactor});"
        triggerRef is VariableContainedTriggerRef -> registerEffect(triggerRef, reaction)
        triggerRef is VariableNameTriggerRef -> registerEffect(triggerRef, reaction)
        else -> throw AssertionError("Unexpected variable type")
      }

  private fun registerPortEffect(triggerRef: TriggerRef, port: Port, reaction: Reaction) =
      when (triggerRef) {
        is VariableNameTriggerRef ->
            "LF_PORT_REGISTER_EFFECT(self->${triggerRef.variable.lfName}, ${reaction.nameInReactor}, ${port.width});"
        is VariableContainedTriggerRef ->
            (0..<triggerRef.container.codeWidth).toList().joinToString(separator = "\n") {
              "LF_PORT_REGISTER_EFFECT(self->${triggerRef.container.lfName}[${it}].${port.lfName}, ${reaction.nameInReactor}, ${port.width})"
            }
        else -> throw AssertionError("Unexpected variable type")
      }

  private fun registerPortObserver(triggerRef: TriggerRef, port: Port, reaction: Reaction) =
      when (triggerRef) {
        is VariableNameTriggerRef ->
            "LF_PORT_REGISTER_OBSERVER(self->${triggerRef.resolve().lfName}, ${reaction.nameInReactor}, ${port.width});"
        is VariableContainedTriggerRef ->
            (0..<triggerRef.container.codeWidth).toList().joinToString(separator = "\n") {
              "LF_PORT_REGISTER_OBSERVER(self->${triggerRef.container.lfName}[${it}].${port.lfName}, ${reaction.nameInReactor}, ${port.width})"
            }
        else -> throw AssertionError("Unexpected variable type")
      }

  private fun registerEffect(triggerRef: VariableNameTriggerRef, reaction: Reaction) =
      when (val variable = triggerRef.variable) {
        is Timer ->
            "LF_TIMER_REGISTER_EFFECT(self->${triggerRef.variable.lfName}, ${reaction.nameInReactor});"
        is Action ->
            "LF_ACTION_REGISTER_EFFECT(self->${triggerRef.variable.lfName}, ${reaction.nameInReactor});"
        is Port -> registerPortEffect(triggerRef, variable, reaction)
        else -> throw AssertionError("Unexpected variable type")
      }

  private fun generateReactionCtor(reaction: Reaction) =
      """|
       |${if (reaction.tardy != null) "LF_DEFINE_REACTION_STP_VIOLATION_HANDLER(${reactor.codeType}, ${reaction.codeName});" else ""}
       |${if (reaction.deadline != null) "LF_DEFINE_REACTION_DEADLINE_VIOLATION_HANDLER(${reactor.codeType}, ${reaction.codeName});" else ""}
       |LF_DEFINE_REACTION_CTOR(${reactor.codeType}, ${reaction.codeName}, ${reaction.index}, ${reaction.ctorDeadlineArgs}, ${reaction.ctorStpArgs});
    """
          .trimMargin()

  private fun registerObserver(triggerRef: TriggerRef, reaction: Reaction) =
      when (val variable = triggerRef.resolve()) {
        is Action ->
            "LF_ACTION_REGISTER_OBSERVER(self->${variable.lfName}, self->${reaction.codeName});"
        is Port -> registerPortObserver(triggerRef, variable, reaction)
        else -> throw AssertionError("Unexpected variable type")
      }

  private fun generateSelfStruct(reaction: Reaction) =
      "LF_DEFINE_REACTION_STRUCT(${reactor.codeType}, ${reaction.codeName}, ${reaction.totalNumEffects});"

  fun generateSelfStructs() =
      reactor.reactions.joinToString(
          separator = "\n", prefix = "// Reaction structs\n", postfix = "\n") {
            generateSelfStruct(it)
          }

  fun generateReactorStructFields() =
      reactor.reactions.joinToString(separator = "\n", postfix = "\n") {
        "LF_REACTION_INSTANCE(${reactor.codeType}, ${it.codeName});"
      }

  fun generateReactionCtors() =
      reactor.reactions.joinToString(
          separator = "\n", prefix = "// Reaction constructors\n", postfix = "\n") {
            generateReactionCtor(it)
          }

  fun generateReactionDeadlineViolationHandlers() =
      reactionsWithDeadline.joinToString(
          separator = "\n", prefix = "// Reaction deadline violation handlers\n", postfix = "\n") {
            generateReactionDeadlineViolationHandler(it)
          }

  fun generateReactionStpViolationHandlers() =
      reactionsWithTardyViolationHandler.joinToString(
          separator = "\n", prefix = "// Reaction STP violation handlers\n", postfix = "\n") {
            generateReactionStpViolationHandler(it)
          }

  fun generateReactionBodies() =
      reactor.reactions.joinToString(
          separator = "\n", prefix = "// Reaction bodies\n", postfix = "\n") {
            generateReactionBody(it)
          }

  private fun generateReactionScope(reaction: Reaction) =
      with(PrependOperator) {
        """ |// Bring self struct, environment, triggers, effects and sources into scope.
            |  LF_SCOPE_SELF(${reactor.codeType});
            |  LF_SCOPE_ENV();
         ${"|  "..generateTriggersEffectsAndSourcesInScope(reaction)}
         ${"|  "..generateContainedTriggersAndSourcesInScope(reaction)}
        """
            .trimMargin()
      }

  private fun generateReactionDeadlineViolationHandler(reaction: Reaction) =
      with(PrependOperator) {
        """
            |LF_DEFINE_REACTION_DEADLINE_VIOLATION_HANDLER(${reactor.codeType}, ${reaction.codeName}) {
         ${"|  "..generateReactionScope(reaction)}
            |  // Start of user-witten reaction deadline handler body
         ${"|  "..(reaction.deadline?.body?.code ?: "")}
            |}
        """
            .trimMargin()
      }

  private fun generateReactionStpViolationHandler(reaction: Reaction) =
      with(PrependOperator) {
        """
            |LF_DEFINE_REACTION_STP_VIOLATION_HANDLER(${reactor.codeType}, ${reaction.codeName}) {
         ${"|  "..generateReactionScope(reaction)}
            |  // Start of user-witten reaction STAA violation handler body
         ${"|  "..(reaction.tardy?.body?.code ?: "")}
            |}
        """
            .trimMargin()
      }

  private fun generateReactionBody(reaction: Reaction) =
      with(PrependOperator) {
        """
            |LF_DEFINE_REACTION_BODY(${reactor.codeType}, ${reaction.codeName}) {
         ${"|  "..generateReactionScope(reaction)}
            |  // Start of user-witten reaction body
         ${"|  "..reaction.body.code}
            |}
        """
            .trimMargin()
      }

  private fun generateContainedTriggerInScope(triggerRef: TriggerRef) =
      when (triggerRef.resolve()) {
          is Port -> {
              var port = triggerRef.resolve() as Port
              if (port.isMultiport) {
                  "LF_MULTIPORT_PTR_INSTANCE(${port.container.codeType}, ${port.lfName}, ${port.width});"
              } else {
                  "LF_PORT_PTR_INSTANCE(${port.container.codeType}, ${port.lfName});"
              }
          }
          else -> throw AssertionError("Unexpected variable type")
      }

  private fun generateContainedMultiportTriggerFieldInit(
      instName: String,
      containerName: String,
      triggerRef: TriggerRef,
      port: Port
  ) =
      """|
         |${instName}.${triggerRef.name}_width = ${port.width};
         |for (int j = 0; j<${port.width}; j++) {
         |  ${instName}.${triggerRef.name}[j] = ${containerName}.${triggerRef.name}[j];
         |}
      """
          .trimMargin()

  private fun generateContainedTriggerFieldInit(instName: String, triggerRef: TriggerRef) : String{
      if (triggerRef.resolve() !is Port) {
          throw AssertionError("Unexpected variable type")
      }

      val port = triggerRef.resolve() as Port

      if (port.isMultiport) {
          return generateContainedMultiportTriggerFieldInit(
              instName, "&self->${port.container.lfName}[0]", triggerRef, port
          )
      } else {
          return "${instName}.${port.lfName} = self->${port.container.lfName}->${triggerRef.name};"
      }
  }

  private fun generateContainedBankTriggerFieldInit(instName: String, triggerRef: TriggerRef) : String {
      if (triggerRef.resolve() !is Port) {
          throw AssertionError("Unexpected variable type")
      }

      val port = triggerRef.resolve() as Port

      if (port.isMultiport) {
          return generateContainedMultiportTriggerFieldInit(
              "${instName}[i]",
              "&self->${port.container.lfName}[i]",
              triggerRef,
              port
          )
      } else {
          return "${instName}[i].${triggerRef.name} = self->${port.container.lfName}[i].${triggerRef.name};"
      }
  }

  private fun generateContainedReactorScope(triggers: List<TriggerRef>, inst: ReactorInstantiation) =
      with(PrependOperator) {
        """|
           |// Generated struct providing access to ports of child reactor `${inst.name}`
           |struct _${inst.reactor.codeType}_${inst.name} {
        ${"|  "..triggers.joinToString(separator = "\n") { generateContainedTriggerInScope(it) }}
           |};
           |struct _${inst.reactor.codeType}_${inst.name} ${inst.name};
        ${"|"..triggers.joinToString(separator = "\n") {generateContainedTriggerFieldInit(inst.name, it)}}
        """
            .trimMargin()
      }

  private fun generateContainedBankScope(triggers: List<TriggerRef>, inst: ReactorInstantiation) =
      with(PrependOperator) {
        """|
           |// Generated struct providing access to ports of child reactor `${inst.name}`
           |struct _${inst.reactor.codeType}_${inst.name} {
        ${"|  "..triggers.joinToString(separator = "\n") { generateContainedTriggerInScope(it) }}
           |};
           |struct _${inst.reactor.codeType}_${inst.name} ${inst.name}[${inst.codeWidth}];
           |size_t ${inst.name}_width = ${inst.codeWidth};
           |for (int i = 0; i<${inst.codeWidth}; i++) {
        ${"|  "..triggers.joinToString(separator = "\n") {generateContainedBankTriggerFieldInit(inst.name, it)}}
           |}
        """
            .trimMargin()
      }

  private fun generateTriggersEffectsAndSourcesInScope(reaction: Reaction) =
      reaction.allUncontainedTriggers
          .plus(reaction.allUncontainedEffects)
          .plus(reaction.allUncontainedSources)
          .joinToString(separator = "\n") { it.scope.toString() }

  private fun generateContainedTriggersAndSourcesInScope(reaction: Reaction) =
      reaction.allContainedEffectsTriggersAndSources.toList().joinToString(separator = "\n") {
        if (it.first.codeWidth > 1) {
          generateContainedBankScope(it.second, it.first)
        } else {
          generateContainedReactorScope(it.second, it.first)
        }
      }

  private fun generateTriggerRegisterEffect(reaction: Reaction) =
      reaction.triggerRefs.joinToString(
          separator = "\n",
      ) {
        registerEffect(it, reaction)
      }

  private fun generateTriggerRegisterObserver(reaction: Reaction) =
      reaction.sourcesRefs.joinToString(
          separator = "\n",
      ) {
        registerObserver(it, reaction)
      }

  private fun generateTriggerRegisterSource(reaction: Reaction) =
      reaction.effectsRefs.joinToString(
          separator = "\n",
      ) {
        registerSource(it, reaction)
      }

  private fun generateReactorCtorCode(reaction: Reaction) =
      with(PrependOperator) {
        """
            |LF_INITIALIZE_REACTION(${reactor.codeType}, ${reaction.codeName}, ${reaction.deadline?.deadline?.toCCode()}); 
        ${" |  "..generateTriggerRegisterEffect(reaction)}
        ${" |  "..generateTriggerRegisterSource(reaction)}
        ${" |  "..generateTriggerRegisterObserver(reaction)}
            """
            .trimMargin()
      }

  fun generateReactorCtorCodes() =
      reactor.reactions.joinToString(separator = "\n", prefix = "// Initialize Reactions \n") {
        generateReactorCtorCode(it)
      }

  /**
   * Returns all the reactions triggered by the Output port which are contained in the parent
   * reactor. This is used for reactions triggered by contained output ports.
   */
  fun getParentReactionEffectsOfOutput(inst: ReactorInstantiation, port: OutputPort): List<Reaction> {
    val res = mutableListOf<Reaction>()
    for (reaction in reactor.reactions) {
      if (reaction.allContainedTriggers.any {
        it is TriggerRef && it.container == inst.reactor && it.resolve() == port
      }) {
        res.add(reaction)
      }
    }
    return res
  }

  fun getParentReactionObserversOfOutput(inst: ReactorInstantiation, port: OutputPort): List<Reaction> {
    val res = mutableListOf<Reaction>()
    for (reaction in reactor.reactions) {
      if (reaction.allContainedSources.any {
        it is TriggerRef && it.container == inst.reactor && it.resolve() == port
      }) {
        res.add(reaction)
      }
    }
    return res
  }

  fun getParentReactionSourcesOfInput(inst: ReactorInstantiation, port: InputPort): List<Reaction> {
    val res = mutableListOf<Reaction>()
    for (reaction in reactor.reactions) {
      if (reaction.allContainedEffects.any {
        it is TriggerRef && it.container == inst.reactor && it.resolve() == port
      }) {
        res.add(reaction)
      }
    }
    return res
  }
}
