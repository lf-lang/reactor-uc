package org.lflang.ir

enum class ConnectionKind {
    CONNECTION,
    DELAYED_CONNECTION,
    PHYSICAL_CONNECTION,
    FEDERATED_CONNECTION,
}

class Connection(
    val sourceRef: PortRef,
    val targetPortRefs: List<PortRef>,
    val kind: ConnectionKind,
    val delay: TimeValue,
    val isIterated: Boolean,
) {
    lateinit var sources: List<Port>
    lateinit var targets: List<Port>

    val isPhysical: Boolean get() = kind == ConnectionKind.PHYSICAL_CONNECTION
}
