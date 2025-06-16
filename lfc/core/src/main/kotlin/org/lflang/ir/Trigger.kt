package org.lflang.ir


enum class TriggerKind {
    TIMER,
    ACTION,
    INPUT,
    OUTPUT,
    STARTUP,
    SHUTDOWN
}

data class TriggerRef(val name: String);

abstract class Trigger {
    abstract val lfName: String
    abstract val kind: TriggerKind
}

class Timer(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.TIMER,
    val offset: TimeValue,
    val period: TimeValue,
) : Trigger() {

}

class Action(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.ACTION,
) : Trigger() {
}