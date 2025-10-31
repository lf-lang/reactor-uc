package org.lflang.ir

data class TargetCode(val code: String)

data class StateVariable(
    val lfName: String,
    val type: CType,
    val init: TargetCode,
    val isInitialized: Boolean
)

data class ConstructorParameters(
    val lfName: String,
    val type: TargetCode,
    val defaultValue: TargetCode?,
    val isTime: Boolean,
    val defaultValueAsTimeValue: TimeValue?,
)

class FederatedEnvironment(val instantiation: List<FederateInstantiation>)

class FederateInstantiation(
    val name: String,
    val federate: Federate,
    val codeWidth: Int
) {
}

class ReactorInstantiation(
    val name: String,
    val reactor: Reactor,
    val codeWidth: Int,
    val allParameters: List<Parameter> = listOf(),
) {

}

class Environment {
    lateinit var mainReactor: Reactor
}

class Parameter(
    val name: String,
    val type: CType,
    val init: TargetCode,
    val value: TargetCode
)

class Reactor(
    val lfName: String,
    val env: Environment,
    val federate: Federate?,
    val fullyQualifiedName: String,
    val isMain: Boolean,
    val preambles: List<Preamble>,
    val ctorParams: List<ConstructorParameters>,
    val childReactors: List<ReactorInstantiation>,
    var parentReactor: Reactor? = null,
    val stateVars: List<StateVariable>,
    val location: ReactorLocationInformation,

) {
  lateinit var ports: List<Port>
  lateinit var triggers: Set<Trigger>
  lateinit var reactions: List<Reaction>
  lateinit var connections: List<Connection>

  val codeType: String = "Reactor_$lfName"
  val includeGuard: String = "LFC_GEN_${lfName.uppercase()}_H"

  val hasStartup
    get(): Boolean = triggers.any { it.kind == TriggerKind.STARTUP }

  val hasShutdown
    get(): Boolean = triggers.any { it.kind == TriggerKind.SHUTDOWN }

  val startup
    get(): Startup? = triggers.find { it.kind == TriggerKind.STARTUP } as Startup?

  val shutdown
    get(): Shutdown? = triggers.find { it.kind == TriggerKind.SHUTDOWN } as Shutdown?

  val actions
    get(): List<Action> = triggers.filter { it.kind == TriggerKind.ACTION } as List<Action>

  val inputs
    get(): List<InputPort> = triggers.filter { it.kind == TriggerKind.INPUT } as List<InputPort>

  val outputs
    get(): List<OutputPort> = triggers.filter { it.kind == TriggerKind.OUTPUT } as List<OutputPort>

  val timers
    get(): List<Timer> = triggers.filter { it.kind == TriggerKind.TIMER } as List<Timer>

  val instantiation
      get(): ReactorInstantiation  = parentReactor?.childReactors?.first { it.reactor == this} ?: throw IllegalStateException("No Reactor")

  val codeWidth
      get() : Int = instantiation.codeWidth

  fun numTriggers(): Int {
        var res =
            actions.size +
                    timers.size +
                    inputs.sumOf { it.width } +
                    outputs.sumOf { it.width }
        if (hasShutdown) res++
        if (hasStartup) res++
        return res
    }


    val numChildren = childReactors.sumOf { it.codeWidth }

    fun hasPhysicalActions(): Boolean {
    for (inst in childReactors) {
      if (inst.reactor.hasPhysicalActions()) return true
    }
    return triggers.any { it.kind == TriggerKind.ACTION && (it as Action).isPhysical }
  }
}
