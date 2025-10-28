package org.lflang.ir

data class Reaction(
    val idx: Int,
    val body: TargetCode,
    val triggerRefs: List<TriggerRef>,
    val sourcesRefs: List<TriggerRef>,
    val effectsRefs: List<TriggerRef>,
    val container: Reactor,
    val loc: ReactorLocationInformation, //TODO:
    val maxWait: MaxWaitReaction?,
    val deadline: DeadlineReaction?,
    val stp: StpViolationReaction?
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


  val allContainedTriggers
        get() = triggerRefs.filter { it is VariableContainedTriggerRef }

    val allContainedEffects
        get() = effectsRefs.filter { it is VariableContainedTriggerRef }

  val allContainedSources
        get() = sourcesRefs.filter { it is VariableContainedTriggerRef }


  val allUncontainedTriggers
      get() = triggerRefs.filter { it is VariableNameTriggerRef }

  val allUncontainedEffects
      get() = effectsRefs.filter { it is VariableNameTriggerRef }

  val allUncontainedSources
      get() = sourcesRefs.filter { it is VariableNameTriggerRef }

  // Calculate the total number of effects, considering that we might write to
  // a contained input port
  val totalNumEffects
      get(): Int {
          var res = 0
          for (effect in allUncontainedEffects) {
              val variable = effect.resolve()
              res +=
                  if (variable is Port) {
                      variable.width
                  } else {
                      1
                  }
          }
          for (effect in allContainedEffects) {
              res += effect.container.codeWidth * (effect.resolve() as Port).width
          }
          return res
      }

  val allContainedEffectsTriggersAndSources
      get() = run {
          val res = mutableMapOf<ReactorInstantiation, List<TriggerRef>>()
          for (triggerRef in allContainedEffects.plus(allContainedSources).plus(allContainedTriggers)) {
              val containedTriggerRef = triggerRef as VariableContainedTriggerRef
              if (containedTriggerRef.container.instantiation !in res.keys) {
                  res[containedTriggerRef.container.instantiation] = mutableListOf()
              }

              res[containedTriggerRef.container.instantiation] = res[containedTriggerRef.container.instantiation]!!.plus(triggerRef)
          }
          res
      }


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

}

fun Reaction.resolveTriggers() {
  // TODO: initialize triggers here
}

data class DeadlineReaction(
    val body: TargetCode,
    val deadline: TimeValue,
)

data class StpViolationReaction(
    val body: TargetCode,
)

data class MaxWaitReaction(
    val body: TargetCode,
    val maxWait: TimeValue,
)
