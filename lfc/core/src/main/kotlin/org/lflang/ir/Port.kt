package org.lflang.ir

open class Port(
    override val kind: TriggerKind = TriggerKind.INPUT,
    override val lfName: String,
    override var container: Reactor?,
    open val dataType: CType,
    val isMultiport: Boolean,
    val width: Int
) : Trigger() {
    val externalArgs
        get(): String = "_${lfName}_external"

    //val width
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
                override var container: Reactor?,
                override val kind: TriggerKind,
                val ports: List<Port>,
) : Trigger() {}


data class PortRef(val name: String)

class InputPort(
    override val kind: TriggerKind = TriggerKind.INPUT,
    override val lfName: String,
    override var container: Reactor?,
    override val dataType: CType,
    isMultiport: Boolean,
    width: Int,
) : Port(kind, lfName, container, dataType, isMultiport, width) {

    lateinit var incomingConnections: List<Connection>
}

class OutputPort(
    override val lfName: String,
    override val kind: TriggerKind = TriggerKind.OUTPUT,
    override var container: Reactor?,
    override val dataType: CType,
    isMultiport: Boolean,
    width: Int,
    ) : Port(kind, lfName, container, dataType, isMultiport, width) {

    lateinit var outgoingConnections: List<Connection>
}