package org.lflang.generator.uc

import org.lflang.allActions
import org.lflang.allInstantiations
import org.lflang.allModes
import org.lflang.allReactions
import org.lflang.allTimers
import org.lflang.generator.uc.UcActionGenerator.Companion.maxNumPendingEvents
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeWidth
import org.lflang.generator.uc.UcReactionGenerator.Companion.codeName
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.generator.uc.UcStateGenerator.Companion.resetSourceName
import org.lflang.generator.uc.UcStateGenerator.Companion.allResetStateVars
import org.lflang.generator.uc.UcStateGenerator.Companion.resetStateVars
import org.lflang.lf.Action
import org.lflang.lf.ActionOrigin
import org.lflang.lf.BuiltinTrigger
import org.lflang.lf.BuiltinTriggerRef
import org.lflang.lf.Instantiation
import org.lflang.lf.Mode
import org.lflang.lf.ModeTransition
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.lf.StateVar
import org.lflang.reactor

class UcModeGenerator(
    private val reactor: Reactor,

    private val connectionGenerator: UcConnectionGenerator
) {

  companion object {
    const val MODE_STATE_FIELD = "_mode_state"

    val Reactor.usesModes: Boolean
      get() = allModes.isNotEmpty() || allInstantiations.any { it.reactor.usesModes }

    val Mode.codeName: String
      get() = "_mode_$name"

    fun collectHistoryEnterableModes(reactor: Reactor): Set<Mode> {
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

    fun Mode.baseRef(historyEnterableModes: Set<Mode>): String =
        if (this in historyEnterableModes) "self->$codeName.super" else "self->$codeName"

    const val ACTIVATION_FIELD = MODE_STATE_FIELD + "_activation"

    val Reaction.isEntryReaction: Boolean
      get() =
          triggers.filterIsInstance<BuiltinTriggerRef>().any {
            it.type == BuiltinTrigger.STARTUP || it.type == BuiltinTrigger.RESET
          }

    val Mode.ownEntryReactions: List<Reaction>
      get() = reactions.filter { it.isEntryReaction }

    val Mode.logicalActions: List<Action>
      get() = actions.filter { it.origin != ActionOrigin.PHYSICAL }

    const val ACTIVATION_MAX_PENDING_EVENTS = 1
  }

  val hasModes: Boolean = reactor.allModes.isNotEmpty()

  private val historyEnterableModes: Set<Mode> = collectHistoryEnterableModes(reactor)

  private fun Mode.isHistoryEnterable(): Boolean = this in historyEnterableModes

  private fun Mode.baseRef(): String = baseRef(historyEnterableModes)

  private val containedWiring: Map<Mode, ContainedWiring> =
      reactor.allModes.associateWith { it.computeContainedWiring() }

  private val Mode.needsActivation: Boolean
    get() =
        ownEntryReactions.isNotEmpty() ||
            containedWiring.getValue(this).activationEffects.isNotEmpty()

  private val modesNeedingActivation: List<Mode> = reactor.allModes.filter { it.needsActivation }

  private val hasActivation: Boolean = modesNeedingActivation.isNotEmpty()

  private fun collectActivationEffects(): List<String> {
    val paths = mutableListOf<String>()
    for (mode in reactor.allModes) {
      for (path in containedWiring.getValue(mode).activationEffects) {
        paths += path
      }
    }
    return paths
  }

  private val activationEffects: List<String> = collectActivationEffects()

  val numTriggers: Int
    get() = if (hasActivation) 1 else 0

  val maxNumPendingEvents: Int
    get() = numTriggers * ACTIVATION_MAX_PENDING_EVENTS

  private data class ModeTrigger(
      val address: String,
      val savedField: String? = null,
      val savedCap: Int = 0,
  )

  private class ContainedWiring {
    val triggers = mutableListOf<ModeTrigger>()
    val gates = mutableListOf<Pair<String, String>>()
    val actionGates = mutableListOf<String>()*/
    val modalChildren = mutableListOf<String>()

    val activationEffects = mutableListOf<String>()

    val resetVars = mutableListOf<Pair<String, StateVar>>()
  }

  private fun Mode.computeContainedWiring(): ContainedWiring {
    val wiring = ContainedWiring()

    fun walk(instantiations: List<Instantiation>, path: String, prefix: String) {
      for (inst in instantiations) {
        for (i in 0 until inst.codeWidth) {
          val child = "$path${inst.name}[$i]."
          if (inst.reactor.allModes.isNotEmpty()) {
            wiring.modalChildren += child
            continue
          }
          for (timer in inst.reactor.allTimers) {
            wiring.triggers += ModeTrigger("(Trigger*)&$child${timer.name}")
            wiring.gates += Pair("$child${timer.name}", "effectively_active")
          }
          for (action in inst.reactor.allActions.filter { it.origin != ActionOrigin.PHYSICAL }) {

            wiring.triggers +=
                ModeTrigger(
                    "(Trigger*)&$child${action.name}",
                    "${prefix}_${inst.name}_${i}_${action.name}_saved",
                    action.maxNumPendingEvents)
            wiring.actionGates += "$child${action.name}"
          }
          for (reaction in inst.reactor.allReactions) {
            val path = "$child${reaction.codeName(inst.reactor)}"
            wiring.gates += Pair(path, gateWordFor(reaction))
            if (reaction.isEntryReaction) wiring.activationEffects += path
          }
          for (sv in inst.reactor.allResetStateVars) {
            wiring.resetVars += Pair(child, sv)
          }
          walk(inst.reactor.allInstantiations, child, "${prefix}_${inst.name}_$i")
        }
      }
    }
    walk(instantiations, "self->", codeName)
    return wiring
  }

  private fun Mode.ownTriggers(): List<ModeTrigger> =
      timers.map { ModeTrigger("(Trigger*)&self->${it.name}") } +
          logicalActions.map {
            ModeTrigger(
                "(Trigger*)&self->${it.name}", "${codeName}_${it.name}_saved", it.maxNumPendingEvents)
          } +
          connectionGenerator.getDelayedConnectionsIn(this).map {
            ModeTrigger(
                "(Trigger*)&self->${it.getUniqueName()}[0][0]",
                "${codeName}_${it.getUniqueName()}_saved",
                it.maxNumPendingEvents)
          } +
          containedWiring.getValue(this).triggers

  private fun gateWordFor(reaction: Reaction): String {
    val builtins = reaction.triggers.filterIsInstance<BuiltinTriggerRef>().map { it.type }
    return when {
      builtins.contains(BuiltinTrigger.STARTUP) -> "fire_startup"
      builtins.contains(BuiltinTrigger.RESET) -> "fire_reset"
      builtins.contains(BuiltinTrigger.SHUTDOWN) -> "had_startup"
      else -> "effectively_active"
    }
  }

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

  fun generateReactorCtorActionCodes(): String {
    val activation =
        if (!hasActivation) ""
        else "LF_INITIALIZE_ACTION(${reactor.codeType}, $ACTIVATION_FIELD, 0, 0);"
    val gates =
        reactor.allModes
            .flatMap { it.logicalActions }
            .joinToString(separator = "\n") { "LF_MICROMODE_ACTION_GATE(${it.name});" }
    return listOf(activation, gates).filter { it.isNotEmpty() }.joinToString(separator = "\n")
  }

  fun generateReactorStructFields(): String {
    if (!hasModes) return ""
    val sb = StringBuilder("// Modes\n")
    sb.appendLine("LF_MODE_BOOKKEEPING_INSTANCES(${reactor.allModes.size});")
    for (mode in reactor.allModes) {

      if (mode.isHistoryEnterable()) {
        sb.appendLine("lf_history_mode_t ${mode.codeName};")
      } else {
        sb.appendLine("lf_mode_t ${mode.codeName};")
      }
      val triggers = mode.ownTriggers()
      if (triggers.isNotEmpty()) {
        sb.appendLine("Trigger* ${mode.codeName}_triggers[${triggers.size}];")

        sb.appendLine("lf_trigger_slot_t ${mode.codeName}_slots[${triggers.size}];")

        if (mode.isHistoryEnterable()) {
          sb.appendLine("lf_history_slot_t ${mode.codeName}_hslots[${triggers.size}];")
          for (trigger in triggers) {
            if (trigger.savedField != null) {
              sb.appendLine("Event ${trigger.savedField}[${trigger.savedCap}];")
            }
          }
        }
      }
      if (mode.reactions.isNotEmpty()) {
        sb.appendLine("Reaction* ${mode.codeName}_reactions[${mode.reactions.size}];")
      }
      val numResetVars = mode.resetStateVars.size + containedWiring.getValue(mode).resetVars.size
      if (numResetVars > 0) {

        sb.appendLine("lf_reset_var_t ${mode.codeName}_reset_vars[$numResetVars];")
      }
    }
    if (hasActivation) {
      sb.appendLine("LF_ACTION_INSTANCE(${reactor.codeType}, $ACTIVATION_FIELD);")
    }
    return sb.toString()
  }

  fun generateReactorCtorCodes(): String {
    if (!hasModes) return ""

    val initial = reactor.allModes.single { it.isInitial }
    val sb = StringBuilder("// Initialize modes\n")
    reactor.allModes.forEachIndexed { i, mode ->
      sb.appendLine("self->_modes[$i] = &${mode.baseRef()};")
    }
    sb.appendLine("LF_INITIALIZE_MODE_STATE(&${initial.baseRef()}, NULL);")
    for (mode in reactor.allModes) {
      val triggers = mode.ownTriggers()
      triggers.forEachIndexed { i, t ->
        sb.appendLine("self->${mode.codeName}_triggers[$i] = ${t.address};")
      }

      if (mode.isHistoryEnterable()) {
        triggers.forEachIndexed { i, t ->
          if (t.savedField != null) {
            sb.appendLine("self->${mode.codeName}_hslots[$i].saved = self->${t.savedField};")

            sb.appendLine(
                "_Static_assert(sizeof(self->${t.savedField}) / sizeof(Event) >= ${t.savedCap}, " +
                    "\"${t.savedField} is smaller than the take bound wired for this trigger\");")
          }
        }
      }
      mode.reactions.forEachIndexed { i, r ->
        sb.appendLine(
            "self->${mode.codeName}_reactions[$i] = (Reaction*)&self->${r.codeName(reactor)};")
      }
      val trigArgs =
          if (triggers.isEmpty()) "NULL, 0, NULL"
          else "self->${mode.codeName}_triggers, ${triggers.size}, self->${mode.codeName}_slots"

      val activationArg =
          if (mode.needsActivation) "&self->$ACTIVATION_FIELD.super" else "NULL"
      val reactArgs =
          if (mode.reactions.isEmpty()) "NULL, 0"
          else "self->${mode.codeName}_reactions, ${mode.reactions.size}"
      val resetRows =
          mode.resetStateVars.map { Pair("self->", it) } + containedWiring.getValue(mode).resetVars
      resetRows.forEachIndexed { i, (path, sv) ->
        sb.appendLine(
            "self->${mode.codeName}_reset_vars[$i] = (lf_reset_var_t){&$path${sv.name}, " +
                "&$path${sv.resetSourceName}, sizeof($path${sv.name})};")
      }
      val resetArgs =
          if (resetRows.isEmpty()) "NULL, 0"
          else "self->${mode.codeName}_reset_vars, ${resetRows.size}"

      if (mode.isHistoryEnterable()) {
        val hslotArg = if (triggers.isEmpty()) "NULL" else "self->${mode.codeName}_hslots"
        sb.appendLine(
            "lf_micromode_history_mode_ctor(&self->${mode.codeName}, &self->$MODE_STATE_FIELD, " +
                "$trigArgs, $hslotArg, $resetArgs, $activationArg, $reactArgs);")
      } else {
        sb.appendLine(
            "lf_micromode_mode_ctor(&${mode.baseRef()}, &self->$MODE_STATE_FIELD, " +
                "$trigArgs, $resetArgs, $activationArg, $reactArgs);")
      }
    }
    for (mode in reactor.allModes) {
      for (timer in mode.timers) {
        sb.appendLine("LF_TIMER_SET_GATE(${timer.name}, &${mode.baseRef()}.effectively_active);")
      }
      for (reaction in mode.reactions) {
        sb.appendLine(
            "LF_REACTION_SET_GATE(${reaction.codeName(reactor)}, " +
                "&${mode.baseRef()}.${gateWordFor(reaction)});")
      }
      val contained = containedWiring.getValue(mode)
      for ((path, gateWord) in contained.gates) {
        sb.appendLine("LF_MICROMODE_CHILD_GATE($path, &${mode.baseRef()}.$gateWord);")
      }
      for (path in contained.actionGates) {
        sb.appendLine("LF_MICROMODE_CHILD_ACTION_GATE($path);")
      }

      for (path in contained.activationEffects) {
        sb.appendLine("LF_ACTION_REGISTER_EFFECT(self->$ACTIVATION_FIELD, $path);")
      }

      for (path in contained.modalChildren) {
        sb.appendLine("$path$MODE_STATE_FIELD.parent_mode = &${mode.baseRef()};")
      }
    }
    return sb.toString()
  }
}
