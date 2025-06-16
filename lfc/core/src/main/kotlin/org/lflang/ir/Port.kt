package org.lflang.ir

abstract class Port : Trigger() {
    abstract val dataType: TargetCode
}

data class PortRef(val name: String)

class InputPort(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.INPUT,
    override val dataType: TargetCode,
    val incomingConnections: List<Connection>
    ) : Port() {

}

class OutputPort(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.OUTPUT,
    override val dataType: TargetCode,
    val outgoingConnections: List<Connection>
    ) : Port() {

}