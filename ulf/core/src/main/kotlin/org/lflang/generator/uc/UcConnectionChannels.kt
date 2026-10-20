package org.lflang.generator.uc

import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.lf.Connection
import org.lflang.lf.VarRef

/**
 * Expands LF connections into the channels they connect.
 */
object UcConnectionChannels {

  /**
   * Given a LF connection and the list of federates of the program (empty if it is not federated),
   * create all the ConnectionChannels found within the LF Connection. This must handle multiports,
   * banks, iterated connections and federated connections.
   */
  fun parse(conn: Connection, allFederates: List<UcFederate>): List<UcConnectionChannel> {
    val res = mutableListOf<UcConnectionChannel>()
    val rhsPorts = conn.rightPorts.map { getChannelQueue(it, allFederates) }
    var rhsPortIndex = 0

    var lhsPorts = conn.leftPorts.map { getChannelQueue(it, allFederates) }
    var lhsPortIndex = 0

    // Keep parsing out connections until we are out of right-hand-side (rhs) ports
    while (rhsPortIndex < rhsPorts.size) {
      // First get the current lhs and rhs port and UcGroupedConnection that we are working with
      val lhsPort = lhsPorts[lhsPortIndex]
      val rhsPort = rhsPorts[rhsPortIndex]
      if (rhsPort.channelsLeft() > lhsPort.channelsLeft()) {
        val rhsChannelsToAdd = rhsPort.takeChannels(lhsPort.channelsLeft())
        val lhsChannelsToAdd = lhsPort.takeRemainingChannels()
        lhsChannelsToAdd.zip(rhsChannelsToAdd).forEach {
          res.add(UcConnectionChannel(it.first, it.second, conn))
        }
        lhsPortIndex += 1
      } else if (rhsPort.channelsLeft() < lhsPort.channelsLeft()) {
        val numRhsChannelsToAdd = rhsPort.channelsLeft()
        val rhsChannelsToAdd = rhsPort.takeRemainingChannels()
        val lhsChannelsToAdd = lhsPort.takeChannels(numRhsChannelsToAdd)
        lhsChannelsToAdd.zip(rhsChannelsToAdd).forEach {
          res.add(UcConnectionChannel(it.first, it.second, conn))
        }
        rhsPortIndex += 1
      } else {
        lhsPort.takeRemainingChannels().zip(rhsPort.takeRemainingChannels()).forEach {
          res.add(UcConnectionChannel(it.first, it.second, conn))
        }
        rhsPortIndex += 1
        lhsPortIndex += 1
      }

      // If we are out of lhs variables, but not rhs, then there should be an iterated connection.
      // We handle it by resetting the lhsChannels variable and index and continuing until
      // we have been through all rhs channels.
      if (lhsPortIndex >= lhsPorts.size && rhsPortIndex < rhsPorts.size) {
        assert(conn.isIterated)
        lhsPorts = conn.leftPorts.map { getChannelQueue(it, allFederates) }
        lhsPortIndex = 0
      }
    }
    return res
  }

  /**
   * Given a port VarRef, and the list of federates. Create a channel queue. I.e. create all the
   * UcChannels and associate them with the correct federates.
   */
  private fun getChannelQueue(portVarRef: VarRef, allFederates: List<UcFederate>): UcChannelQueue {
    return if (portVarRef.container?.isAFederate ?: false) {
      val federates = allFederates.filter { it.inst == portVarRef.container }
      UcChannelQueue(portVarRef, federates)
    } else {
      UcChannelQueue(portVarRef, emptyList())
    }
  }
}
