package org.lflang.generator.uc

import org.eclipse.emf.ecore.EObject
import org.lflang.allActions
import org.lflang.allInstantiations
import org.lflang.allModes
import org.lflang.allReactions
import org.lflang.allTimers
import org.lflang.generator.uc.UcActionGenerator.Companion.maxNumPendingEvents
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeWidth
import org.lflang.generator.uc.UcReactionGenerator.Companion.codeName
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.generator.uc.UcStateGenerator.Companion.allResetStateVars
import org.lflang.generator.uc.UcStateGenerator.Companion.resetSourceName
import org.lflang.generator.uc.UcStateGenerator.Companion.resetStateVars
import org.lflang.lf.BuiltinTrigger
import org.lflang.lf.BuiltinTriggerRef
import org.lflang.lf.Instantiation
import org.lflang.lf.Mode
import org.lflang.lf.ModeTransition
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.lf.StateVar
import org.lflang.reactor

/**
 * Emits micromode's bookkeeping for a modal reactor: one `lf_mode_t` per mode, the arrays it points
 * at, the activation action its modes share, and the gates that suppress what a mode holds while it
 * is inactive.
 *
 * Modes move nothing out of the generated reactor struct. A mode's timer stays an ordinary `Timer`
 * field and this generator only adds the layer that gates it. All storage is fixed size and inline
 * in the reactor struct, sized per instance.
 */
class UcModeGenerator(
    private val reactor: Reactor,
    // Not named `connections`: Mode declares a member of that name, which would shadow it.
    private val connectionGenerator: UcConnectionGenerator,
) {

  companion object {
    /** Field holding a modal reactor's `lf_mode_state_t`. Named by micromode's ABI macro. */
    const val MODE_STATE_FIELD = "_mode_state"

    /**
     * Field holding the zero-delay action micromode schedules when a mode is entered, so the entry
     * lands one microstep after the transition. One per reactor: at most one mode of a state
     * machine is active, so every mode can share it.
     */
    const val ACTIVATION_FIELD = MODE_STATE_FIELD + "_activation"

    /**
     * Activation events outstanding at once. Exactly one: it is scheduled at the end of a tag with
     * offset 0 and consumed at the next microstep, before another can be scheduled.
     */
    const val ACTIVATION_MAX_PENDING_EVENTS = 1

    /** Gate words micromode gives a mode. `lf_micromode_validate_all` refuses any other. */
    private const val GATE_ACTIVE = "effectively_active"
    private const val GATE_STARTUP = "fire_startup"
    private const val GATE_RESET = "fire_reset"
    private const val GATE_SHUTDOWN = "had_startup"

    /**
     * Whether a program rooted here needs micromode, directly or through anything it instantiates.
     * The build glue keys on this.
     *
     * `GeneratorBase.hasModalReactors` is not a substitute: it reads `Reactor.getModes()`, which
     * misses a mode inherited through `extends`.
     */
    val Reactor.usesModes: Boolean
      get() = allModes.isNotEmpty() || allInstantiations.any { it.reactor.usesModes }

    /** Field holding this mode's `lf_mode_t`. */
    val Mode.codeName: String
      get() = "_mode_$name"

    /**
     * Whether an element is declared on its reactor rather than inside one of its modes. The `all*`
     * accessors flatten the two together.
     */
    val EObject.outsideAnyMode: Boolean
      get() = eContainer() !is Mode

    /**
     * The modes that need suspend storage, since a reset entry discards whatever was saved: those
     * some reaction targets with `-> history(m)`, and every mode of a reactor instantiated below a
     * mode like that. A history entry of the enclosing mode resumes the current one of them.
     */
    fun collectHistoryEnterableModes(reactor: Reactor): Set<Mode> =
        if (reactor.nestedUnderHistoryMode) reactor.allModes.toSet()
        else collectHistoryTargets(reactor)

    /**
     * Whether some instance of this reactor lies, at any depth, inside a mode that its own reactor
     * targets with `-> history(m)`. Decided per reactor class, over every instantiation of it in
     * the loaded program, because the struct layout is per class.
     */
    private val Reactor.nestedUnderHistoryMode: Boolean
      get() {
        val reactors =
            eResource()?.resourceSet?.allContents?.asSequence()?.filterIsInstance<Reactor>()
                ?: return false
        return reactors.any { container ->
          container.allInstantiations.any { inst ->
            inst.reactor == this &&
                ((inst.eContainer() as? Mode)?.let { it in collectHistoryTargets(container) } ==
                    true || container.nestedUnderHistoryMode)
          }
        }
      }

    /**
     * The modes some reaction of this reactor targets with `-> history(m)`.
     *
     * `VarRef.transition` is an EEnum whose unset value is RESET, so a bare `-> B` lands on reset
     * here exactly as it does in UcReactionGenerator.
     */
    private fun collectHistoryTargets(reactor: Reactor): Set<Mode> {
      val targets = mutableSetOf<Mode>()
      for (mode in reactor.allModes) {
        for (reaction in mode.reactions) {
          for (effect in reaction.effects) {
            if (effect.transition != ModeTransition.HISTORY) continue
            val target = effect.variable
            if (target is Mode) targets += target
          }
        }
      }
      return targets
    }

    /**
     * How codegen addresses a mode as an `lf_mode_t`: through `.super` for the `lf_history_mode_t`
     * subtype, directly otherwise. Every reference goes through here, so a misclassification is a
     * pointer-type error rather than something that silently works.
     */
    fun Mode.baseRef(historyEnterableModes: Set<Mode>): String =
        if (this in historyEnterableModes) "self->$codeName.super" else "self->$codeName"

    /** Whether a mode entry fires this reaction: a `reaction(startup)` or a `reaction(reset)`. */
    private val Reaction.isEntryReaction: Boolean
      get() =
          triggers.filterIsInstance<BuiltinTriggerRef>().any {
            it.type == BuiltinTrigger.STARTUP || it.type == BuiltinTrigger.RESET
          }
  }

  /* ------------------------------------------------------------------- model */

  /**
   * One entry of a mode's trigger array: the trigger's address and, for one that can hold events
   * across tags, the field buffering them while the mode is inactive and that buffer's length.
   *
   * A timer needs no buffer, since `Timer_suspend` owns one. [capacity] is the trigger's own
   * `max_pending_events`, which both sizes the buffer and bounds the take.
   */
  private data class ModeTrigger(
      val address: String,
      val savedField: String? = null,
      val capacity: Int = 0,
  )

  /** A timer or reaction of a contained reactor, and the gate word it takes. */
  private data class Gate(val path: String, val word: String)

  /** A row of a mode's `reset_vars`: the accessor reaching the variable, and the variable. */
  private data class ResetVar(val path: String, val variable: StateVar)

  /** Everything one mode needs emitted, gathered by [buildModeInfo]. */
  private class ModeInfo(val mode: Mode, val isHistoryEnterable: Boolean) {
    /** Triggers the mode owns, in the order its parallel slot arrays are indexed by. */
    val triggers = mutableListOf<ModeTrigger>()
    /** Timers and reactions of contained reactors, each with the gate word it takes. */
    val childGates = mutableListOf<Gate>()
    /** Logical actions of contained reactors, whose schedule call the gate intercepts. */
    val childActions = mutableListOf<String>()
    /** Rows of the mode's `reset_vars`, its own first. */
    val resetVars = mutableListOf<ResetVar>()
    /**
     * Entry reactions of contained reactors. They are absent from [reactions], so the tag-start
     * hook never reaches them and the activation action is their only trigger.
     */
    val activationEffects = mutableListOf<String>()
    /** Contained modal reactors, whose `parent_mode` this mode fills in. */
    val modalChildren = mutableListOf<String>()

    /**
     * Every reaction written inside the mode, not only the entry ones: `lf_micromode_on_tag_start`
     * filters this array by gate word.
     */
    val reactions: List<Reaction>
      get() = mode.reactions

    /**
     * Whether entering the mode has to cost a microstep. True when the mode owns an entry reaction,
     * or when a reactor instantiated inside it does.
     */
    val needsActivation: Boolean
      get() = mode.reactions.any { it.isEntryReaction } || activationEffects.isNotEmpty()
  }

  val hasModes: Boolean = reactor.allModes.isNotEmpty()

  private val historyEnterableModes: Set<Mode> = collectHistoryEnterableModes(reactor)

  private val infos: List<ModeInfo> = reactor.allModes.map { buildModeInfo(it) }

  /**
   * The contained entry reactions over every mode of this reactor, all of them effects of the one
   * shared activation. Merging them is safe because each stays gated on the `fire_startup` or
   * `fire_reset` of the mode that contains it.
   */
  private val activationEffects: List<String> = infos.flatMap { it.activationEffects }

  private val hasActivation: Boolean = infos.any { it.needsActivation }

  /** `LF_INITIALIZE_ACTION` appends the activation to the reactor's `_triggers` array. */
  val numTriggers: Int
    get() = if (hasActivation) 1 else 0

  val maxNumPendingEvents: Int
    get() = numTriggers * ACTIVATION_MAX_PENDING_EVENTS

  private fun baseRef(mode: Mode): String = mode.baseRef(historyEnterableModes)

  /**
   * Which gate word a reaction's `Reaction.enabled` points at.
   *
   * `startup` takes fire_startup rather than effectively_active because the tag-start hook
   * re-offers a mode's entry reactions on every entry, and effectively_active would run this one
   * again on each reset re-entry. `shutdown` takes had_startup, which is sticky, so every mode ever
   * entered runs its own shutdown reaction.
   */
  private fun gateWordFor(reaction: Reaction): String {
    val builtins = reaction.triggers.filterIsInstance<BuiltinTriggerRef>().map { it.type }
    return when {
      builtins.contains(BuiltinTrigger.STARTUP) -> GATE_STARTUP
      builtins.contains(BuiltinTrigger.RESET) -> GATE_RESET
      builtins.contains(BuiltinTrigger.SHUTDOWN) -> GATE_SHUTDOWN
      else -> GATE_ACTIVE
    }
  }

  /** What one mode declares itself, then what the reactors instantiated inside it contribute. */
  private fun buildModeInfo(mode: Mode): ModeInfo {
    val info = ModeInfo(mode, mode in historyEnterableModes)
    val name = mode.codeName
    for (timer in mode.timers) {
      info.triggers += ModeTrigger("(Trigger*)&self->${timer.name}")
    }
    // Physical actions included: micromode gates and suspends them exactly as logical ones,
    // which is what reactor-c does.
    for (action in mode.actions) {
      info.triggers +=
          ModeTrigger(
              "(Trigger*)&self->${action.name}",
              "${name}_${action.name}_saved",
              action.maxNumPendingEvents)
    }
    // `[0][0]` addresses the single element of a connection's `[bankWidth][portWidth]` array.
    // Both dimensions are 1 inside a mode: UcValidator refuses banks and multiports there.
    for (conn in connectionGenerator.getDelayedConnectionsIn(mode)) {
      info.triggers +=
          ModeTrigger(
              "(Trigger*)&self->${conn.getUniqueName()}[0][0]",
              "${name}_${conn.getUniqueName()}_saved",
              conn.maxNumPendingEvents)
    }
    for (stateVar in mode.resetStateVars) {
      info.resetVars += ResetVar("self->", stateVar)
    }
    collectContained(info, mode.instantiations, "self->", name)
    return info
  }

  /**
   * Adds what the reactors instantiated inside a mode contribute to it, from one walk of the
   * containment tree below it.
   *
   * A mode owns what a child declares OUTSIDE any mode of the child's own, whether that child is
   * modal or not: those elements are live exactly when the mode is, so they take its gate and its
   * suspend and restore lifecycle. A child's mode-local elements stay with the child's own modes,
   * which is what keeps a history entry of this mode from resuming them.
   *
   * A modal child is recorded as well, so its state can be linked to this mode by `parent_mode`.
   * That link is what makes the child's own modes inactive while this one is.
   *
   * Connections are not hoisted: UcValidator refuses a contained delayed one, and a zero-delay one
   * holds nothing across a tag.
   *
   * [path] is the accessor reaching the current reactor, "self->" at the root and "self->c[0]." one
   * level down, so a member is named by concatenation. [prefix] names the generated fields for it,
   * and is derived from the path, so two children of one mode, or two elements of one bank, can
   * never collide.
   */
  private fun collectContained(
      info: ModeInfo,
      instantiations: List<Instantiation>,
      path: String,
      prefix: String,
  ) {
    for (inst in instantiations) {
      val child = inst.reactor
      for (i in 0 until inst.codeWidth) {
        val at = "$path${inst.name}[$i]."
        val field = "${prefix}_${inst.name}_$i"
        if (child.allModes.isNotEmpty()) info.modalChildren += at
        for (timer in child.allTimers.filter { it.outsideAnyMode }) {
          info.triggers += ModeTrigger("(Trigger*)&$at${timer.name}")
          info.childGates += Gate("$at${timer.name}", GATE_ACTIVE)
        }
        for (action in child.allActions.filter { it.outsideAnyMode }) {
          info.triggers +=
              ModeTrigger(
                  "(Trigger*)&$at${action.name}",
                  "${field}_${action.name}_saved",
                  action.maxNumPendingEvents)
          info.childActions += "$at${action.name}"
        }
        for (reaction in child.allReactions.filter { it.outsideAnyMode }) {
          val target = "$at${reaction.codeName(child)}"
          info.childGates += Gate(target, gateWordFor(reaction))
          if (reaction.isEntryReaction) info.activationEffects += target
        }
        for (stateVar in child.allResetStateVars.filter { it.outsideAnyMode }) {
          info.resetVars += ResetVar(at, stateVar)
        }
        collectContained(info, child.allInstantiations.filter { it.outsideAnyMode }, at, field)
      }
    }
  }

  /* ---------------------------------------------------------------- emission */

  /** The activation action's typedef. VOID because an activation carries no value. */
  fun generateSelfStructs(): String =
      if (!hasActivation) ""
      else
          "LF_DEFINE_ACTION_STRUCT_VOID(${reactor.codeType}, $ACTIVATION_FIELD, LogicalAction, " +
              "${activationEffects.size}, 0, 0, $ACTIVATION_MAX_PENDING_EVENTS);"

  fun generateCtors(): String =
      if (!hasActivation) ""
      else
          "LF_DEFINE_ACTION_CTOR_VOID(${reactor.codeType}, $ACTIVATION_FIELD, LogicalAction, " +
              "ACTION_POLICY_DEFER, ${activationEffects.size}, 0, 0, " +
              "$ACTIVATION_MAX_PENDING_EVENTS);"

  /**
   * The activation's `LF_INITIALIZE_ACTION`, then the mode gate on every mode-local action. Both
   * belong with the reactor's ordinary action initialization, because `LF_MICROMODE_ACTION_GATE`
   * overwrites the `Action.schedule` pointer `Action_ctor` assigns.
   *
   * Zero delay and zero spacing: micromode requires the activation to land on the microstep
   * immediately after the tag the mode was entered on.
   */
  fun generateReactorCtorActionCodes(): String {
    val activation =
        if (!hasActivation) ""
        else "LF_INITIALIZE_ACTION(${reactor.codeType}, $ACTIVATION_FIELD, 0, 0);"
    val gates =
        reactor.allModes
            .flatMap { it.actions }
            .joinToString(separator = "\n") { "LF_MICROMODE_ACTION_GATE(${it.name});" }
    return fuseNonEmpty(activation, gates)
  }

  fun generateReactorStructFields(): String {
    if (!hasModes) return ""
    val sb = StringBuilder("// Modes\n")
    sb.appendLine("LF_MODE_BOOKKEEPING_INSTANCES(${infos.size});")
    for (info in infos) {
      sb.append(generateModeFields(info))
    }
    if (hasActivation) {
      sb.appendLine("LF_ACTION_INSTANCE(${reactor.codeType}, $ACTIVATION_FIELD);")
    }
    return sb.toString()
  }

  /** One mode's `lf_mode_t` and the arrays it points at. */
  private fun generateModeFields(info: ModeInfo): String {
    val name = info.mode.codeName
    val sb = StringBuilder()
    // The subtype only where a history transition can reach the mode: it is the base type plus
    // the `hslots` pointer, and a mode nothing history-enters has nothing to point it at.
    sb.appendLine(if (info.isHistoryEnterable) "lf_history_mode_t $name;" else "lf_mode_t $name;")
    if (info.triggers.isNotEmpty()) {
      sb.appendLine("Trigger* ${name}_triggers[${info.triggers.size}];")
      sb.appendLine("lf_trigger_slot_t ${name}_slots[${info.triggers.size}];")
      // The third array, and the buffers its rows point at, exist only where a history entry can
      // reach the mode. Leaving any other purges its pending events outright.
      if (info.isHistoryEnterable) {
        sb.appendLine("lf_history_slot_t ${name}_hslots[${info.triggers.size}];")
        for (trigger in info.triggers) {
          if (trigger.savedField != null) {
            sb.appendLine("Event ${trigger.savedField}[${trigger.capacity}];")
          }
        }
      }
    }
    if (info.reactions.isNotEmpty()) {
      sb.appendLine("Reaction* ${name}_reactions[${info.reactions.size}];")
    }
    if (info.resetVars.isNotEmpty()) {
      // Per instance, never `static const`: every row holds `&self->...`, so a modal reactor
      // instantiated twice needs two of them.
      sb.appendLine("lf_reset_var_t ${name}_reset_vars[${info.resetVars.size}];")
    }
    return sb.toString()
  }

  /**
   * The `_modes` array and the mode state, then each mode's arrays and constructor, and last the
   * gate assignments, which must follow every constructor above because they write into what those
   * constructors set up.
   */
  fun generateReactorCtorCodes(): String {
    if (!hasModes) return ""
    // `single` rather than `first`: LFValidator guarantees one initial mode within a reactor's
    // own list, and UcValidator refuses more than one once an extends chain is merged. Falling
    // back to an arbitrary mode would hide a break in either check.
    val initial = reactor.allModes.single { it.isInitial }
    val sb = StringBuilder("// Initialize modes\n")
    infos.forEachIndexed { i, info -> sb.appendLine("self->_modes[$i] = &${baseRef(info.mode)};") }
    // NULL parent: a reactor cannot tell from inside itself whether it was instantiated inside a
    // mode. An enclosing mode fills the link in, in generateModeGates below.
    sb.appendLine("LF_INITIALIZE_MODE_STATE(&${baseRef(initial)}, NULL);")
    for (info in infos) {
      sb.append(generateModeCtor(info))
    }
    for (info in infos) {
      sb.append(generateModeGates(info))
    }
    return sb.toString()
  }

  /** One mode's arrays, filled from the same lists that sized its struct fields. */
  private fun generateModeCtor(info: ModeInfo): String {
    val name = info.mode.codeName
    val sb = StringBuilder()
    info.triggers.forEachIndexed { i, trigger ->
      sb.appendLine("self->${name}_triggers[$i] = ${trigger.address};")
    }
    if (info.isHistoryEnterable) {
      info.triggers.forEachIndexed { i, trigger ->
        if (trigger.savedField == null) return@forEachIndexed
        sb.appendLine("self->${name}_hslots[$i].saved = self->${trigger.savedField};")
        // The take banks up to the trigger's own bound with no bound of its own, so a buffer
        // sized from anything else would be written past its end.
        sb.appendLine(
            "_Static_assert(sizeof(self->${trigger.savedField}) / sizeof(Event) >= " +
                "${trigger.capacity}, \"${trigger.savedField} is smaller than the take bound " +
                "wired for this trigger\");")
      }
    }
    info.reactions.forEachIndexed { i, reaction ->
      sb.appendLine(
          "self->${name}_reactions[$i] = (Reaction*)&self->${reaction.codeName(reactor)};")
    }
    info.resetVars.forEachIndexed { i, (path, stateVar) ->
      sb.appendLine(
          "self->${name}_reset_vars[$i] = (lf_reset_var_t){&$path${stateVar.name}, " +
              "&$path${stateVar.resetSourceName}, sizeof($path${stateVar.name})};")
    }

    val triggerArgs =
        if (info.triggers.isEmpty()) "NULL, 0, NULL"
        else "self->${name}_triggers, ${info.triggers.size}, self->${name}_slots"
    val reactionArgs =
        if (info.reactions.isEmpty()) "NULL, 0"
        else "self->${name}_reactions, ${info.reactions.size}"
    val resetArgs =
        if (info.resetVars.isEmpty()) "NULL, 0"
        else "self->${name}_reset_vars, ${info.resetVars.size}"
    // Read off needsActivation and never off the mode's own entry reactions: a mode whose only
    // entry work belongs to a reactor it contains would otherwise be handed NULL while that
    // reaction was still registered as an effect of the action.
    val activationArg = if (info.needsActivation) "&self->$ACTIVATION_FIELD.super" else "NULL"

    // The subtype's constructor takes the mode by its own type rather than through baseRef, so a
    // misclassification is an incompatible-pointer error here rather than a mode that silently
    // refuses every history transition at run time.
    if (info.isHistoryEnterable) {
      val hslotArg = if (info.triggers.isEmpty()) "NULL" else "self->${name}_hslots"
      sb.appendLine(
          "lf_micromode_history_mode_ctor(&self->$name, &self->$MODE_STATE_FIELD, " +
              "$triggerArgs, $hslotArg, $resetArgs, $activationArg, $reactionArgs);")
    } else {
      sb.appendLine(
          "lf_micromode_mode_ctor(&self->$name, &self->$MODE_STATE_FIELD, " +
              "$triggerArgs, $resetArgs, $activationArg, $reactionArgs);")
    }
    return sb.toString()
  }

  /**
   * One mode's gate assignments, for what it declares and for what it contains. Every in-mode timer
   * and reaction must be gated before `lf_micromode_validate_all` runs, which asserts exactly that.
   */
  private fun generateModeGates(info: ModeInfo): String {
    val base = baseRef(info.mode)
    val sb = StringBuilder()
    for (timer in info.mode.timers) {
      sb.appendLine("LF_TIMER_SET_GATE(${timer.name}, &$base.$GATE_ACTIVE);")
    }
    for (reaction in info.reactions) {
      sb.appendLine(
          "LF_REACTION_SET_GATE(${reaction.codeName(reactor)}, &$base.${gateWordFor(reaction)});")
    }
    for ((path, word) in info.childGates) {
      sb.appendLine("LF_MICROMODE_CHILD_GATE($path, &$base.$word);")
    }
    for (path in info.childActions) {
      sb.appendLine("LF_MICROMODE_CHILD_ACTION_GATE($path);")
    }
    for (path in info.activationEffects) {
      sb.appendLine("LF_ACTION_REGISTER_EFFECT(self->$ACTIVATION_FIELD, $path);")
    }
    // Where a contained modal reactor's NULL parent is filled in. This link is what puts the
    // child's own modes under this mode's gate.
    for (path in info.modalChildren) {
      sb.appendLine("$path$MODE_STATE_FIELD.parent_mode = &$base;")
    }
    return sb.toString()
  }
}
