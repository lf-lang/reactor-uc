package org.lflang.ir

open class Port(
    override val kind: TriggerKind = TriggerKind.INPUT,
    override val lfName: String,
    open val dataType: CType,
    val isMultiport: Boolean,
    val isInterleaved: Boolean,
    val width: Int
) : Trigger(lfName, kind) {
  val externalArgs
    get(): String = "_${lfName}_external"

  // val width
  //    get(): Int = widthSpec?.getWidth() ?: 1

  val maxWait: TimeValue
    get(): TimeValue = container.federate.maxWait

  val codeType: String = this.lfName


    fun triggerRef(r: Reactor) : TriggerRef {
        if (r == this.container) {
            return VariableNameTriggerRef(
                container = r,
                variable = this
            )
        } else {
            return VariableContainedTriggerRef(
                container = container,
                variable = this,
                instance = container.instantiation.name
            )
        }
    }

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
) : Trigger(lfName, kind) {}

class InputPort(
    override val kind: TriggerKind = TriggerKind.INPUT,
    override val lfName: String,
    override val dataType: CType,
    isMultiport: Boolean,
    isInterleaved: Boolean,
    width: Int,
) : Port(kind, lfName, dataType, isMultiport, isInterleaved, width) {
  lateinit var incomingConnections: List<Connection>
}

class OutputPort(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.OUTPUT,
    override val dataType: CType,
    isMultiport: Boolean,
    isInterleaved: Boolean,
    width: Int,
) : Port(kind, lfName, dataType, isMultiport, isInterleaved, width) {
  lateinit var outgoingConnections: List<Connection>
}
