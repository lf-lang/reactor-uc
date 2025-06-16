package org.lflang.ir

enum class ConnectionKind {
    CONNECTION,
    DELAYED_CONNECTION,
    PHYSICAL_CONNECTION,
    FEDERATED_CONNECTION,
}

data class Connection(
    val source: PortRef,
    val target: PortRef,
    val kind: ConnectionKind
)
