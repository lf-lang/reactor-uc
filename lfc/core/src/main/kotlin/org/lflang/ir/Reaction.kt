package org.lflang.ir

import kotlin.time.Duration

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
