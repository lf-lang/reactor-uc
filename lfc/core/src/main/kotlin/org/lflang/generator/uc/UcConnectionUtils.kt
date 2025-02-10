package org.lflang.generator.uc

import org.lflang.AttributeUtils.*
import org.lflang.TimeValue
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeWidth
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.maxWait
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.lf.Connection
import org.lflang.lf.Port
import org.lflang.lf.VarRef

/**
 * A UcConnectionChannel is the fundamental lowest-level representation of a connection in a LF
 * program. It connects two UcChannels, one at the source and one at the destination.
 */
class UcConnectionChannel(val src: UcChannel, val dest: UcChannel, val conn: Connection) {
  val isFederated =
      (src.federate != null) && (dest.federate != null) && (src.federate != dest.federate)

  /**
   * Get the NetworkChannelType of this connection. If we are not in a federated program it is NONE
   */
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

/**
 * A GroupedConnection is a set of ConnectionChannels that can be grouped together for efficiency.
 * All ConnectionChannels that start from the same LF port, either because of multiports, banks, or
 * multiple connections. Are grouped.
 *
 * TODO: Give a better exaplanation for what a GroupedConnection is.
 */
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
  val maxNumPendingEvents =
      if (getConnectionBufferSize(lfConn) > 0) getConnectionBufferSize(lfConn) else 1

  fun assignUid(id: Int) {
    uid = id
  }

  fun getUniqueName() = "conn_${srcInst?.name ?: ""}_${srcPort.name}_${uid}"
}

/**
 * In federated programs, we must group connections differently. For a non federated program. A
 * single output port connected to N different input ports could be grouped to a single
 * GroupedConnection. For a federated program, we would need N GroupedConnections if an output port
 * goes to N different federates. Moreover, if there are several kinds of NetworkChannels in the
 * program, then we can only group connections transported over the same channels.
 */
class UcFederatedGroupedConnection(
    src: VarRef,
    channels: List<UcConnectionChannel>,
    lfConn: Connection,
    val srcFed: UcFederate,
    val destFed: UcFederate,
) : UcGroupedConnection(src, channels, lfConn) {

    // FIXME: Allow user to override and provide these.
    val serializeFunc = "serialize_payload_default"
    val deserializeFunc = "deserialize_payload_default"

    fun getMaxWait(): TimeValue {
        val inputPort = channels.first().dest.varRef.variable as Port
        return inputPort.maxWait
    }
}

/**
 * A FederatedConnectionBundle will contain all GroupedConnections going between two federates, in
 * either direction. It also contains a NetworkChannel connecting and NetworkEndpoint in each
 * federate.
 */
class UcFederatedConnectionBundle(
    val src: UcFederate,
    val dest: UcFederate,
    val groupedConnections: List<UcFederatedGroupedConnection>
) {
  val networkChannel: UcNetworkChannel =
      UcNetworkChannel.createNetworkEndpointsAndChannelForBundle(this)

  fun numOutputs(federate: UcFederate) = groupedConnections.count { it.srcFed == federate }

  fun numInputs(federate: UcFederate) = groupedConnections.count { it.destFed == federate }

  fun generateNetworkChannelCtor(federate: UcFederate): String =
      if (federate == src) {
        networkChannel.generateChannelCtorSrc()
      } else {
        networkChannel.generateChannelCtorDest()
      }
}

/**
 * An UcChannel represents a single channel of an LF Port. Due to Multiports and Banks, each LF Port
 * can have multiple channels.
 */
class UcChannel(val varRef: VarRef, val portIdx: Int, val bankIdx: Int, val federate: UcFederate?) {
  fun getCodePortIdx() = portIdx

  fun getCodeBankIdx() = if (federate == null) bankIdx else 0

  private val portOfContainedReactor = varRef.container != null
  private val reactorInstance =
      if (portOfContainedReactor) "${varRef.container.name}[${getCodeBankIdx()}]." else ""
  private val portInstance = "${varRef.name}[${getCodePortIdx()}]"

  fun generateChannelPointer() = "&self->${reactorInstance}${portInstance}"
}

/**
 * This is a convenience-wrapper around a LF Port. It will construct UcChannels corresponding to the
 * multiports and banks.
 *
 * If this is a federates program. it must be passed a list of federates associated with the LF
 * Port. It is a list in case it is a bank. Then each federate of the bank must be passed to the
 * constructor.
 */
class UcChannelQueue(varRef: VarRef, federates: List<UcFederate>) {
  private val bankWidth = varRef.container?.width ?: 1
  private val portWidth = (varRef.variable as Port).width
  private val isInterleaved = varRef.isInterleaved
  /** A queue of UcChannels that can be popped of as we create UcConnetions */
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
