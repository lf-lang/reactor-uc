package org.lflang.generator.uc

import org.lflang.AttributeUtils.getLinkAttribute
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeWidth
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.lf.Connection
import org.lflang.lf.Port
import org.lflang.lf.VarRef


class UcConnectionChannel(val src: UcChannel, val dest: UcChannel, val conn: Connection) {
    val isFederated = (src.federate != null) && (dest.federate != null) && (src.federate != dest.federate)

    fun getChannelType(): NetworkChannelType {
        val linkAttr = getLinkAttribute(conn)
        return if (linkAttr == null) {
            src.federate?.getDefaultInterface()?.type ?: NetworkChannelType.NONE
        } else {
            val srcIf = linkAttr.getParamString("left")
            if (srcIf == null) {
                src.federate?.getDefaultInterface()?.type ?: NetworkChannelType.NONE
            } else {
                src.federate?.getInterface(srcIf)?.type ?: NetworkChannelType.NONE
            }
        }
    }
}

open class UcGroupedConnection(
    val src: VarRef,
    val channels: List<UcConnectionChannel>,
    val lfConn: Connection,
) {
    val delay = lfConn.delay.orNever().toCCode()
    val isPhysical = lfConn.isPhysical
    val isLogical = !lfConn.isPhysical && lfConn.delay == null
    val srcInst = src.container
    val srcPort = src.variable as Port
    val isDelayed = lfConn.isPhysical || !isLogical // We define physical connections as delayed.

    private var uid: Int = -1

    val bankWidth = srcInst?.codeWidth ?: 1
    val portWidth = srcPort.width
    val numDownstreams = {
        val frequencyMap =
            channels.groupingBy { Pair(it.src.getCodePortIdx(), it.src.getCodeBankIdx()) }.eachCount()
        frequencyMap.values.maxOrNull() ?: 0
    }
    val maxNumPendingEvents = 16 // FIXME: Must be derived from the program

    fun assignUid(id: Int) {
        uid = id
    }

    fun getUniqueName() = "conn_${srcInst?.name ?: ""}_${srcPort.name}_${uid}"
}

class UcFederatedGroupedConnection(
    src: VarRef,
    channels: List<UcConnectionChannel>,
    lfConn: Connection,
    val srcFed: UcFederate,
    val destFed: UcFederate,
) : UcGroupedConnection(src, channels, lfConn) {

    val serializeFunc = "serialize_payload_default"
    val deserializeFunc = "deserialize_payload_default"
}

class UcFederatedConnectionBundle(
    val src: UcFederate,
    val dest: UcFederate,
    val groupedConnections: List<UcFederatedGroupedConnection>
) {
    val networkChannel: UcNetworkChannel = UcNetworkChannel.createNetworkChannelForBundle(this)

    fun numOutputs(federate: UcFederate) = groupedConnections.count { it.srcFed == federate }

    fun numInputs(federate: UcFederate) = groupedConnections.count { it.destFed == federate }

    fun generateNetworkChannelCtor(federate: UcFederate): String =
        if (federate == src) {
            networkChannel.generateChannelCtorSrc()
        } else {
            networkChannel.generateChannelCtorDest()
        }
}

// Convenience class around a port variable reference. It is used to encapsulate the management of multi-connections
// where a single lhs port has to
class UcChannel(val varRef: VarRef, val portIdx: Int, val bankIdx: Int, val federate: UcFederate?) {
    fun getCodePortIdx() = portIdx
    fun getCodeBankIdx() = if (federate == null) bankIdx else 0

    private val portOfContainedReactor = varRef.container != null
    private val reactorInstance =
        if (portOfContainedReactor) "${varRef.container.name}[${getCodeBankIdx()}]." else ""
    private val portInstance = "${varRef.name}[${getCodePortIdx()}]"

    fun generateChannelPointer() = "&self->${reactorInstance}${portInstance}"
}

// Wrapper around a variable reference to a Port. Contains a channel for each bank/multiport within it.
// For each connection statement where a port is referenced, we create an UcPort and use this class
// to figure out how the individual channels are connect to other UcPorts.
class UcPort(varRef: VarRef, federates: List<UcFederate>) {
    private val bankWidth = varRef.container?.width ?: 1
    private val portWidth = (varRef.variable as Port).width
    private val isInterleaved = varRef.isInterleaved
    private val channels = ArrayDeque<UcChannel>()

    // Construct the stack of channels belonging to this port. If this port is interleaved,
    // then we create channels first for ports then for banks.
    init {
        if (isInterleaved) {
            for (i in 0..<portWidth) {
                for (j in 0..<bankWidth) {
                    channels.add(UcChannel(varRef, i, j, federates.find { it.bankIdx == j }))
                }
            }
        } else {
            for (i in 0..<bankWidth) {
                for (j in 0..<portWidth) {
                    channels.add(UcChannel(varRef, j, i, federates.find { it.bankIdx == i }))
                }
            }
        }
    }

    fun takeRemainingChannels(): List<UcChannel> = takeChannels(channels.size)

    // Get a number of channels from this port. This has sideeffects and will remove these
    // channels from the port.
    fun takeChannels(numChannels: Int): List<UcChannel> {
        assert(numChannels >= channels.size)
        val res = mutableListOf<UcChannel>()
        for (i in 1..numChannels) {
            res.add(channels.removeFirst())
        }
        return res
    }

    fun channelsLeft(): Int = channels.size
}
