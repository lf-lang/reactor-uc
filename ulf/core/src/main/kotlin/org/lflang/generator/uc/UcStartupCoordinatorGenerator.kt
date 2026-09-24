package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

enum class JoiningPolicy {
  JOIN_IMMEDIATELY,
  JOIN_TIMER_ALIGNED;

  companion object {
    fun parse(str: String): JoiningPolicy =
        when (str) {
          "\"JOIN_IMMEDIATELY\"" -> JOIN_IMMEDIATELY
          "\"JOIN_TIMER_ALIGNED\"" -> JOIN_TIMER_ALIGNED
          else -> throw IllegalArgumentException("Unknown joining policy: $str")
        }
  }
}

fun JoiningPolicy.toCString() =
    when (this) {
      JoiningPolicy.JOIN_IMMEDIATELY -> "JOIN_IMMEDIATELY"
      JoiningPolicy.JOIN_TIMER_ALIGNED -> "JOIN_TIMER_ALIGNED"
      else -> throw IllegalArgumentException("Joining policy not handled")
    }

class UcStartupCoordinatorGenerator(
    private val federate: UcFederate,
    private val connectionGenerator: UcConnectionGenerator,
    private val joiningPolicy: JoiningPolicy,
) {

  companion object {
    // The number of system events allocated for each neighbor. Used to schedule received messages
    // as
    // system events. The worst-case number of events is 3. It happens when you have received:
    // HandshakeResponse
    // HandshakeRequest
    // And then you handle the HandshakeRequest which produces a HandshakeResponse to your peer,
    // but before you have completed the handling of this HandshakeRequest you receive the first
    // StartTimeProposal from that peer.
    val numSystemEventsPerBundle = 3

    // The number of additional system events allocated. These are the events a federate
    // schedules for ITSELF, and the value must match the NUM_RESERVED_EVENTS compile def in
    // startup_coordinator.c. Several self-scheduled events can be live at once: the event being handled (not
    // freed until the bottom of StartupCoordinator_handle_system_event), the handshake-request
    // retry the handler re-arms while any neighbour is unaccounted for, the mixed-state retry,
    // and the start-time proposal or request that follows. This does not need to scale with the
    // number of neighbours.
    val numSystemEventsConst = 5

    // Returns the number of system events needed by the clock sync subsystem, given a number of
    // neighbors.
    fun getNumSystemEvents(numBundles: Int) =
        numSystemEventsPerBundle * numBundles + numSystemEventsConst

    val instName = "startup_coordinator"
  }

  private val numNeighbors = connectionGenerator.getNumFederatedConnectionBundles()
  private val numSystemEvents = getNumSystemEvents(numNeighbors)
  private val longestPath = connectionGenerator.getLongestFederatePath()
  private val typeName = "Federate"

  fun generateSelfStruct() =
      "LF_DEFINE_STARTUP_COORDINATOR_STRUCT(${typeName}, ${numNeighbors}, ${numSystemEvents})"

  fun generateCtor() =
      "LF_DEFINE_STARTUP_COORDINATOR_CTOR(Federate, ${numNeighbors}, ${longestPath}, ${numSystemEvents}, ${numSystemEventsConst}, ${joiningPolicy.toCString()});"

  fun generateFederateStructField() = "${typeName}StartupCoordinator ${instName};"

  fun generateFederateCtorCode() = "LF_INITIALIZE_STARTUP_COORDINATOR(${typeName});"
}
