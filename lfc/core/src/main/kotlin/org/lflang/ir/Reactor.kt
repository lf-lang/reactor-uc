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

class Instantiation(
    val name: String,
    val reactor: Reactor,
    val codeWidth: Int,
) {

    fun getNumConnectionsFromPort(instantiation: org.lflang.lf.Instantiation?, port: org.lflang.lf.Port): Int {
        var count = 0
        // Find all outgoing non-federated grouped connections from this port
        for (groupedConn in nonFederatedConnections) {
            if (groupedConn.srcInst == instantiation && groupedConn.srcPort == port) {
                count += 1
            }
        }

        // Find all outgoing federated grouped connections from this port.
        for (federatedConnectionBundle in federatedConnectionBundles) {
            for (groupedConn in federatedConnectionBundle.groupedConnections) {
                if (groupedConn.srcFed == currentFederate &&
                    groupedConn.srcInst == instantiation &&
                    groupedConn.srcPort == port) {
                    count += 1
                }
            }
        }
        return count
    }
}

class Environment(
    val federates: List<Federate>
) {
    val isFederated: Boolean get() = federates.size > 1
}


class Reactor (
    val lfName: String,
    val env: Environment,
    val federate: Federate,
    val fullyQualifiedName: String,
    val isMain: Boolean,
    val preambles: List<TargetCode>,
    val ctorParams: List<ConstructorParameters>,
    val childReactors: List<Instantiation>,
    var parentReactor: Reactor? = null,
    val stateVars: List<StateVariable>,
    val location: LocationInformation,
    val codeType: String = "Reactor_$lfName",
    val includeGuard: String = "LFC_GEN_${lfName.uppercase()}_H"
) {
    lateinit var ports: List<Port>
    lateinit var triggers: Set<Trigger>
    lateinit var reactions: List<Reaction>
    lateinit var connections: List<Connection>

    val hasStartup
        get(): Boolean =
            triggers.any { it.kind == TriggerKind.STARTUP }

    val hasShutdown
        get(): Boolean =
            triggers.any { it.kind == TriggerKind.SHUTDOWN }

    val startup
        get(): Startup? = triggers.find {it.kind == TriggerKind.STARTUP } as Startup?

    val shutdown
        get(): Shutdown? = triggers.find {it.kind == TriggerKind.SHUTDOWN } as Shutdown?

    val actions
        get(): List<Action> = triggers.filter { it.kind == TriggerKind.ACTION } as List<Action>

    val inputs
        get(): List<InputPort> = triggers.filter { it.kind == TriggerKind.INPUT} as List<InputPort>

    val outputs
        get(): List<OutputPort> = triggers.filter { it.kind == TriggerKind.OUTPUT} as List<OutputPort>

    val timers
        get(): List<Timer> = triggers.filter { it.kind == TriggerKind.TIMER} as List<Timer>


    fun hasPhysicalActions(): Boolean {
        for (inst in childReactors) {
            if (inst.reactor.hasPhysicalActions()) return true
        }
        return triggers.any { it.kind == TriggerKind.ACTION  && (it as Action).isPhysical }
    }
}