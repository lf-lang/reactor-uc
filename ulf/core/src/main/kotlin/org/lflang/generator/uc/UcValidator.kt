package org.lflang.generator.uc

import java.nio.file.Path
import org.eclipse.emf.ecore.EObject
import org.lflang.MessageReporter
import org.lflang.allActions
import org.lflang.allConnections
import org.lflang.allInstantiations
import org.lflang.allModes
import org.lflang.allReactions
import org.lflang.allStateVars
import org.lflang.allTimers
import org.lflang.ast.ASTUtils
import org.lflang.generator.CodeMap
import org.lflang.generator.DiagnosticReporting
import org.lflang.generator.ValidationStrategy
import org.lflang.generator.Validator
import org.lflang.generator.uc.UcPortGenerator.Companion.isVoid
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasPhysicalActions
import org.lflang.lf.ActionOrigin
import org.lflang.lf.BuiltinTrigger
import org.lflang.lf.BuiltinTriggerRef
import org.lflang.lf.Mode
import org.lflang.lf.Port
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.lf.VarRef
import org.lflang.reactor
import org.lflang.util.LFCommand

class UcValidator(
    private val fileConfig: UcFileConfig,
    messageReporter: MessageReporter,
    codeMaps: Map<Path, CodeMap>
) : Validator(messageReporter, codeMaps) {

  private val ucValidationStrategy =
      object : ValidationStrategy {
        override fun getCommand(generatedFile: Path?): LFCommand {
          return LFCommand.get(
              "cargo",
              listOf("clippy", "--message-format", "json-diagnostic-rendered-ansi"),
              true,
              fileConfig.srcGenPkgPath)
        }

        override fun getErrorReportingStrategy() = DiagnosticReporting.Strategy { _, _, _ -> }

        override fun getOutputReportingStrategy() =
            DiagnosticReporting.Strategy { validationOutput, errorReporter, map ->
              for (messageLine in validationOutput.lines()) {
                println(messageLine)
              }
            }

        override fun getPriority(): Int = 0

        override fun isFullBatch(): Boolean = true
      }

  override fun getPossibleStrategies(): Collection<ValidationStrategy> =
      listOf(ucValidationStrategy)

  override fun getBuildReportingStrategies():
      Pair<DiagnosticReporting.Strategy, DiagnosticReporting.Strategy> =
      Pair(
          ucValidationStrategy.errorReportingStrategy, ucValidationStrategy.outputReportingStrategy)

  companion object {
    /**
     * Whether an element is declared directly on its reactor rather than inside one of that
     * reactor's modes. 
     */
    private val EObject.outsideAnyMode: Boolean
      get() = eContainer() !is Mode

    /**
     * How a connection's endpoint is written in the program.
     */
    private val VarRef.portPath: String
      get() = "${container?.name?.plus(".") ?: ""}${variable.name}"

    /**
     * A modal reactor an enclosing mode reaches, and the instance path that reaches it. 
     */
    private data class ReachedModal(val path: String, val reactor: Reactor)

    /**
     * Every modal reactor reachable from [this] through plain (non-modal) containment, each with
     * the instance path that reaches it. Mirrors
     * the descent `UcModeGenerator`'s `computeContainedWiring` makes: it walks through a
     * non-modal child into whatever that child instantiates in turn, and stops at a modal one,
     * whose interior is left to its own modes.
     */
    private fun Reactor.reachableModalReactors(path: String): List<ReachedModal> =
        if (allModes.isNotEmpty()) listOf(ReachedModal(path, this))
        else allInstantiations.flatMap { it.reactor.reachableModalReactors("$path.${it.name}") }

    /**
     * Every reaction an enclosing mode has to gate on [this]'s behalf: its own and those of
     * whatever it instantiates, transitively, stopping at a modal reactor. Empty for a modal
     * reactor itself.
     */
    private fun Reactor.reachableContainedReactions(): List<Reaction> =
        if (allModes.isNotEmpty()) emptyList()
        else allReactions + allInstantiations.flatMap { it.reactor.reachableContainedReactions() }

    /**
     * Whether [this] reactor, or anything it instantiates transitively, declares a delayed
     * connection.
     */
    private fun Reactor.hasDelayedConnections(): Boolean =
        allConnections.any { it.isPhysical || it.delay != null } ||
            allInstantiations.any { it.reactor.hasDelayedConnections() }

    /**
     * Whether [this] reactor, or anything it instantiates transitively, instantiates a bank.
     */
    private fun Reactor.hasBanks(): Boolean =
        allInstantiations.any { it.widthSpec != null || it.reactor.hasBanks() }

    /**
     * The refusal for a reaction carrying a builtin trigger alongside anything else, or null
     * when there is none to make.
     */
    private fun mixedBuiltinTriggerRefusal(reaction: Reaction, subject: String): String? {
      val builtinTriggers = reaction.triggers.filterIsInstance<BuiltinTriggerRef>()
      if (builtinTriggers.isEmpty() || reaction.triggers.size <= 1) return null
      val anchor = builtinTriggers.first()
      // toOriginalText, not toText: toText embeds CodeMap.Correspondence source-mapping tags
      // meant for generated code, which would leak into this diagnostic.
      val others =
          reaction.triggers
              .filterNot { it === anchor }
              .joinToString(", ") {
                if (it is BuiltinTriggerRef) "'${it.type.literal}'" else ASTUtils.toOriginalText(it)
              }
      return "$subject triggered by both '${anchor.type.literal}' and $others cannot be gated. " +
          "Reaction.enabled is one pointer per reaction, and each of those triggers needs a " +
          "different gate. Split it into one reaction per trigger."
    }

    /**
     * Returns one human-readable error per modal construct in [reactor] that the uC backend
     * cannot express yet, or refuses by design.
     */
    fun validateModes(reactor: Reactor): List<String> {
      val errors = mutableListOf<String>()

      for (reaction in reactor.reactions) {
        if (reaction.triggers.filterIsInstance<BuiltinTriggerRef>().any {
          it.type == BuiltinTrigger.RESET
        })
            errors +=
                "reactor '${reactor.name}': a reaction triggered by 'reset' outside any mode is refused"
      }

      val allModes = ASTUtils.allModes(reactor)
      val initialModes = allModes.filter { it.isInitial }
      if (initialModes.size > 1) {
        errors +=
            "reactor '${reactor.name}': ${initialModes.size} modes are marked initial once " +
                "${reactor.name} and its superclasses are combined (" +
                "${initialModes.joinToString(", ") { it.name }}); only one is allowed"
      }
      for (mode in allModes) {
        val where = "mode '${mode.name}' of reactor '${reactor.name}'"
        for (action in mode.actions.filter { it.origin == ActionOrigin.PHYSICAL })
            errors += "$where: physical action '${action.name}' inside a mode is refused by design"

        for (inst in mode.instantiations) {
          if (inst.widthSpec != null)
              errors +=
                  "$where: banks inside modes are not supported yet ('${inst.name}'); " +
                      "a reactor instantiated inside a mode must have width 1"
          else if (inst.reactor.hasBanks())
              errors +=
                  "$where: banks inside modes are not supported yet: contained reactor " +
                      "'${inst.name}' instantiates one, and the mode's walk descends into it, " +
                      "hoisting every element's triggers and gating every element's reactions; " +
                      "every reactor a mode reaches, at any depth, must have width 1"
        }

        for (inst in mode.instantiations.filter { it.reactor.hasPhysicalActions() })
            errors +=
                "$where: contained reactor '${inst.name}' contains a physical action, which " +
                    "is refused inside a mode by design"

        for (inst in mode.instantiations.filter { it.reactor.hasDelayedConnections() })
            errors +=
                "$where: contained reactor '${inst.name}' declares a delayed connection, " +
                    "which is not supported yet: only a delayed connection written in the " +
                    "mode itself is bound to that mode's lifecycle, so a contained one would " +
                    "fire while the mode is inactive and have its event dropped. Declare the " +
                    "connection in the mode itself, move it out of the mode entirely, or " +
                    "remove its delay"

        for (inst in mode.instantiations) {
          for ((path, modal) in inst.reactor.reachableModalReactors(inst.name)) {

            val outside =
                modal.allTimers.filter { it.outsideAnyMode } +
                    modal.allActions.filter { it.outsideAnyMode } +
                    modal.allReactions.filter { it.outsideAnyMode } +
                    modal.allInstantiations.filter { it.outsideAnyMode } +
                    modal.allStateVars.filter { it.isReset && it.outsideAnyMode }
            if (outside.isNotEmpty())
                errors +=
                    "$where: contained modal reactor '$path' declares timers, actions, " +
                        "reset state, " +
                        "reactions or instantiations outside its own modes, which are not " +
                        "supported yet; only a modal reactor whose entire body lives in its " +
                        "modes may be instantiated inside a mode"
          }
        }
        for (conn in mode.connections) {

          val widePorts =
              (conn.leftPorts + conn.rightPorts).filter { ((it.variable as? Port)?.width ?: 1) > 1 }
          if (widePorts.isNotEmpty())
              errors +=
                  "$where: multiport connections inside modes are not supported yet " +
                      "(${widePorts.joinToString(", ") { "'${it.portPath}'" }}); every port of a " +
                      "connection inside a mode must have width 1"

          val delayed = conn.isPhysical || conn.delay != null // mirrors UcGroupedConnection.isDelayed
          val ungatedSources =
              conn.leftPorts.filter { it.container == null || it.container !in mode.instantiations }
          if (delayed && ungatedSources.isNotEmpty())
              errors +=
                  "$where: a delayed connection whose source " +
                      "(${ungatedSources.joinToString(", ") { "'${it.portPath}'" }}) is not a port " +
                      "of a reactor instantiated in this mode is refused: the mode cannot gate " +
                      "what stages the connection's events, so a value produced while the mode " +
                      "is inactive would be delivered where reactor-c drops it"

          if (conn.isPhysical)
              errors +=
                  "$where: a physical connection ('~>') inside a mode is refused: its event is " +
                      "tagged from physical time, while a history entry restores a suspended " +
                      "event by shifting it by the logical time the mode was away, so the " +
                      "restore rule has no defined meaning for it"
        }
        if (mode.watchdogs.isNotEmpty())
            errors += "$where: watchdogs inside modes are not supported"
        for (reaction in mode.reactions) {
          mixedBuiltinTriggerRefusal(reaction, "$where: a reaction")?.let { errors += it }
        }

        for (inst in mode.instantiations) {
          for (reaction in inst.reactor.reachableContainedReactions()) {
            mixedBuiltinTriggerRefusal(
                    reaction, "$where: a reaction of contained reactor '${inst.name}'")
                ?.let { errors += it }
          }
        }
      }
      return errors
    }
  }
}
