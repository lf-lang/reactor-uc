package org.lflang.generator.uc

class UcShutdownCoordinatorGenerator(
    private val federate: UcFederate,
    private val connectionGenerator: UcConnectionGenerator,
) {

  companion object {
    // The number of system events allocated for each neighbor. Used to schedule received messages
    // as system events. A neighbor sends a shutdown announcement only when it accepts a new
    // (better) shutdown proposal, and it only forwards announcements that the receiver can still
    // accept (step <= longest path, see ShutdownCoordinator_perform_shutdown). Thus, a federate
    // receives at most one announcement per accepted proposal and neighbor. With a single
    // initiator, or with several initiators in a fully connected federation (where announcements
    // are not forwarded), this is at most one pending announcement per neighbor. The pool is shared
    // by all neighbors.
    val numSystemEventsPerBundle = 1

    // The number of additional system events allocated.
    val numSystemEventsConst = 2

    // Returns the number of system events needed by the shutdown coordinator subsystem, given a
    // number of
    // neighbors.
    fun getNumSystemEvents(numBundles: Int) =
        numSystemEventsPerBundle * numBundles + numSystemEventsConst

    val instName = "shutdown_coordinator"
  }

  private val numNeighbors = connectionGenerator.getNumFederatedConnectionBundles()
  private val numSystemEvents = getNumSystemEvents(numNeighbors)
  private val longestPath = connectionGenerator.getLongestFederatePath()
  private val typeName = "Federate"

  fun generateSelfStruct() =
      "LF_DEFINE_SHUTDOWN_COORDINATOR_STRUCT(${typeName}, ${numSystemEvents})"

  fun generateCtor() =
      "LF_DEFINE_SHUTDOWN_COORDINATOR_CTOR(Federate, ${longestPath}, ${numSystemEvents});"

  fun generateFederateStructField() = "${typeName}ShutdownCoordinator ${instName};"

  fun generateFederateCtorCode() = "LF_INITIALIZE_SHUTDOWN_COORDINATOR(${typeName});"
}
