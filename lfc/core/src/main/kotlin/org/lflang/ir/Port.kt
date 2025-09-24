package org.lflang.ir

open class Port(
    override val kind: TriggerKind = TriggerKind.INPUT,
    override val lfName: String,
    open val dataType: CType,
    val isMultiport: Boolean,
    val width: Int
) : Trigger() {
    override lateinit var container: Reactor
  val externalArgs
    get(): String = "_${lfName}_external"

  // val width
  //    get(): Int = widthSpec?.getWidth() ?: 1

  val maxWait: TimeValue
    get(): TimeValue = container!!.federate.maxWait

  val codeType: String = this.lfName

  /*
  val Type.isArray
      get(): Boolean = cStyleArraySpec != null

  val Type.arrayLength
      get(): Int = cStyleArraySpec.length */
}

class Multiport(
    override val lfName: String,
    override val kind: TriggerKind,
    val ports: List<Port>,
) : Trigger() {
    override lateinit var container: Reactor
}

data class PortRef(val name: String)

class InputPort(
    override val kind: TriggerKind = TriggerKind.INPUT,
    override val lfName: String,
    override val dataType: CType,
    isMultiport: Boolean,
    width: Int,
) : Port(kind, lfName, dataType, isMultiport, width) {

  override lateinit var container: Reactor
  lateinit var incomingConnections: List<Connection>
}

class OutputPort(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.OUTPUT,
    override val dataType: CType,
    isMultiport: Boolean,
    width: Int,
) : Port(kind, lfName, dataType, isMultiport, width) {

    override lateinit var container: Reactor
  lateinit var outgoingConnections: List<Connection>
}
