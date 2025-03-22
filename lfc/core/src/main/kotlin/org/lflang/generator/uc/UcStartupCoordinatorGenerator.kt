package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

class UcStartupCoordinatorGenerator(
    private val federate: UcFederate,
    private val connectionGenerator: UcConnectionGenerator
) {

  companion object {
    // The number of system events allocated for each neigbor. Used to schedule received messages as
    // system events.
    val numSystemEventsPerBundle = 5

    // The number of additional system events allocated. This system event is used for the periodic
    // SyncRequest event. The value must match the NUM_RESERVED_EVENTS compile def in
    // startup_coordination.c
    val numSystemEventsConst = 3

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
      "LF_DEFINE_STARTUP_COORDINATOR_CTOR(Federate, ${numNeighbors}, ${longestPath}, ${numSystemEvents});"

  fun generateFederateStructField() = "${typeName}StartupCoordinator ${instName};"

  fun generateFederateCtorCode() = "LF_INITIALIZE_STARTUP_COORDINATOR(${typeName});"
}
