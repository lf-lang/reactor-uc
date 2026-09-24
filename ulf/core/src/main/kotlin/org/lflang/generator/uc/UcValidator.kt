package org.lflang.generator.uc

import java.nio.file.Path
import org.lflang.MessageReporter
import org.lflang.allConnections
import org.lflang.allInstantiations
import org.lflang.allModes
import org.lflang.allReactions
import org.lflang.ast.ASTUtils
import org.lflang.generator.CodeMap
import org.lflang.generator.DiagnosticReporting
import org.lflang.generator.ValidationStrategy
import org.lflang.generator.Validator
import org.lflang.generator.uc.UcModeGenerator.Companion.outsideAnyMode
import org.lflang.generator.uc.UcPortGenerator.Companion.width
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

    /** How a connection's endpoint is written in the program. */
    private val VarRef.portPath: String
      get() = "${container?.name?.plus(".") ?: ""}${variable.name}"

    /**
     * Every reaction an enclosing mode has to gate on this reactor's behalf: those it declares
     * outside any mode of its own, and the same for whatever it instantiates outside a mode,
     * transitively.
     *
     * Mirrors the walk `UcModeGenerator.collectContained` makes. A mode-local reaction is absent
     * because the mode holding it gates it, in its own reactor's generator.
     */
    private fun Reactor.reachableContainedReactions(): List<Reaction> =
        allReactions.filter { it.outsideAnyMode } +
            allInstantiations
                .filter { it.outsideAnyMode }
                .flatMap { it.reactor.reachableContainedReactions() }

    /** Whether this reactor, or anything it instantiates, declares a delayed connection. */
    private fun Reactor.hasDelayedConnections(): Boolean =
        allConnections.any { it.isPhysical || it.delay != null } ||
            allInstantiations.any { it.reactor.hasDelayedConnections() }

    /** Whether this reactor, or anything it instantiates, instantiates a bank. */
    private fun Reactor.hasBanks(): Boolean =
        allInstantiations.any { it.widthSpec != null || it.reactor.hasBanks() }

    /**
     * The refusal for a reaction carrying a builtin trigger alongside anything else, or null when
     * there is none to make.
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
     * A `reaction(reset)` has meaning only as a mode's reset entry, so one written on the reactor
     * itself has no gate to take.
     *
     * Reads the reactor's own list rather than [allReactions]: an inherited reaction is reported
     * when its declaring reactor is validated, and reporting it twice helps nobody.
     */
    private fun validateResetOutsideMode(reactor: Reactor): List<String> =
        reactor.reactions
            .filter { reaction ->
              reaction.triggers.filterIsInstance<BuiltinTriggerRef>().any {
                it.type == BuiltinTrigger.RESET
              }
            }
            .map {
              "reactor '${reactor.name}': a reaction triggered by 'reset' outside any mode is " +
                  "refused"
            }

    /** A state machine has exactly one entry point, counted after an extends chain is merged. */
    private fun validateSingleInitialMode(reactor: Reactor): List<String> {
      val initialModes = reactor.allModes.filter { it.isInitial }
      if (initialModes.size <= 1) return emptyList()
      return listOf(
          "reactor '${reactor.name}': ${initialModes.size} modes are marked initial once " +
              "${reactor.name} and its superclasses are combined (" +
              "${initialModes.joinToString(", ") { it.name }}); only one is allowed")
    }

    /** What a mode may instantiate. The walk that hoists a child's triggers assumes width 1. */
    private fun validateModeInstantiations(mode: Mode, where: String): List<String> {
      val errors = mutableListOf<String>()
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

        if (inst.reactor.hasDelayedConnections())
            errors +=
                "$where: contained reactor '${inst.name}' declares a delayed connection, " +
                    "which is not supported yet: only a delayed connection written in the " +
                    "mode itself is bound to that mode's lifecycle, so a contained one would " +
                    "fire while the mode is inactive and have its event dropped. Declare the " +
                    "connection in the mode itself, move it out of the mode entirely, or " +
                    "remove its delay"
      }
      return errors
    }

    /**
     * What a mode may connect. A mode owns a connection's events, so it must be able to gate it.
     */
    private fun validateModeConnections(mode: Mode, where: String): List<String> {
      val errors = mutableListOf<String>()
      for (conn in mode.connections) {
        val widePorts =
            (conn.leftPorts + conn.rightPorts).filter { ((it.variable as? Port)?.width ?: 1) > 1 }
        if (widePorts.isNotEmpty())
            errors +=
                "$where: multiport connections inside modes are not supported yet " +
                    "(${widePorts.joinToString(", ") { "'${it.portPath}'" }}); every port of a " +
                    "connection inside a mode must have width 1"

        // Mirrors UcGroupedConnection.isDelayed.
        val isDelayed = conn.isPhysical || conn.delay != null
        val ungatedSources =
            conn.leftPorts.filter { it.container == null || it.container !in mode.instantiations }
        if (isDelayed && ungatedSources.isNotEmpty())
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
      return errors
    }

    /** Reactions a mode gates, its own and those of the reactors it reaches. */
    private fun validateModeReactions(mode: Mode, where: String): List<String> {
      val errors = mutableListOf<String>()
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
      return errors
    }

    /**
     * Returns one human-readable error per modal construct in [reactor] that the uC backend cannot
     * express yet, or refuses by design.
     */
    fun validateModes(reactor: Reactor): List<String> {
      val errors = mutableListOf<String>()
      errors += validateResetOutsideMode(reactor)
      errors += validateSingleInitialMode(reactor)
      for (mode in reactor.allModes) {
        val where = "mode '${mode.name}' of reactor '${reactor.name}'"
        errors += validateModeInstantiations(mode, where)
        errors += validateModeConnections(mode, where)
        errors += validateModeReactions(mode, where)
        if (mode.watchdogs.isNotEmpty())
            errors += "$where: watchdogs inside modes are not supported"
      }
      return errors
    }
  }
}
