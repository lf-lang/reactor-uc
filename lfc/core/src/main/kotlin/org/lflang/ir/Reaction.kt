package org.lflang.ir

data class Reaction(
    val idx: Int,
    val body: TargetCode,
    val triggerRefs: List<TriggerRef>,
    val sourcesRefs: List<TriggerRef>,
    val effectsRefs: List<TriggerRef>,
    val container: Reactor,
    val loc: LocationInformation,
    val maxWait: MaxWaitReaction?,
    val deadline: DeadlineReaction?
) {
  lateinit var sources: List<Trigger>
  lateinit var effects: List<Trigger>
  lateinit var observers: List<Trigger>

  val triggers: List<Trigger>
    get() = sources + effects + observers

  val codeName
    get(): String = "reaction$index"

  val nameInReactor
    get(): String = "self->${codeName}"

  val index
    get(): Int {
      var idx = 0
      for (r in this.container.reactions) {
        if (this == r) {
          break
        }
        idx += 1
      }
      return idx
    }

  val allUncontainedTriggers
    get() = triggers.filterNot { it.isContainedRef }

  val ctorDeadlineArgs
    get() =
        if (this.deadline != null)
            "LF_REACTION_TYPE(${container.codeType}, ${codeName}_deadline_violation_handler)  "
        else "NULL"

  val ctorStpArgs
    get() =
        if (this.maxWait != null)
            "LF_REACTION_TYPE(${container.codeType}, ${codeName}_stp_violation_handler)"
        else "NULL"

  val allUncontainedEffects
    get() = effects.filterNot { it.isContainedRef }

  val allUncontainedSources
    get() = sources.filterNot { it.isContainedRef }

  val allContainedEffects
    get() = effects.filter { it.isContainedRef }
}

fun Reaction.resolveTriggers() {
  // TODO: initialize triggers here
}

data class DeadlineReaction(
    val body: TargetCode,
    val deadline: TimeValue,
)

data class MaxWaitReaction(
    val body: TargetCode,
    val maxWait: TimeValue,
)
