package org.lflang.ir

enum class TriggerKind {
    TIMER,
    ACTION,
    INPUT,
    OUTPUT,
    STARTUP,
    SHUTDOWN
}

class CType(
    val targetCode: org.lflang.ir.TargetCode,
    val isArray: Boolean,
    val arrayLength: Int,
    val isVoid: Boolean
)

abstract class TriggerRef(
    open val container: Reactor
)

class StartupTriggerRef(container: Reactor) : TriggerRef(container) {}

class ShutdownTriggerRef(container: Reactor) : TriggerRef(container) {}

data class VariableNameTriggerRef(
    override val container: Reactor,
    val variable: String
) : TriggerRef(container) {}

data class VariableContainedTriggerRef(
    override val container: Reactor,
    val variable: String,
    val instance: String
) : TriggerRef(container) {}

fun VariableContainedTriggerRef.resolve(): Trigger {
    val foundTrigger = container.triggers.find { it.lfName == this.variable };

    if (foundTrigger == null) {
        throw IllegalArgumentException("Trigger '$variable' has not been found inside reactor ${container.lfName}")
    }

    return foundTrigger;
}

abstract class Trigger {
    abstract val lfName: String
    abstract var container: Reactor?
    abstract val kind: TriggerKind

    fun setContainer(reactor: Reactor) {
        this.container = reactor
    }

    /** Reactions triggered, by this trigger*/
    val getEffects
        get() = {
            this.container!!.reactions.filter { it.effects.contains(this) }
        }

    /** Reactions that can trigger, this trigger */
    val getSources
        get() = {
            this.container!!.reactions.filter { it.sources.contains(this) }
        }

    /** Reactions that can observe this trigger */
    val getObservers
        get() = {
            this.container!!.reactions.filter { it.observers.contains(this) }
        }
}

class Timer(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.TIMER,
    override var container: Reactor? = null,
    val offset: TimeValue,
    val period: TimeValue
) : Trigger() {}

class Action(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.ACTION,
    override var container: Reactor? = null,
    val isPhysical: Boolean = false,
    val type: CType,
    val maxNumPendingEvents: Int = 1,
    val minDelay: TimeValue,
    val minSpacing: TimeValue,
) : Trigger() {

    /** Returns the C Enum representing the type of action. */
    val actionType
        get(): String = if (isPhysical) "PhysicalAction" else "LogicalAction"

}

class Startup(
    override val lfName: String,
    override var container: Reactor? = null,
    override val kind: TriggerKind = TriggerKind.STARTUP,
) : Trigger() {}

class Shutdown(
    override val lfName: String,
    override var container: Reactor? = null,
    override val kind: TriggerKind = TriggerKind.SHUTDOWN,
) : Trigger() {}