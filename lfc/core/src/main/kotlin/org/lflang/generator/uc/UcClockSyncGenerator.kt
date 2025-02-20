package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

class UcClockSyncGenerator(
    private val federate: UcFederate,
    private val connectionGenerator: UcConnectionGenerator
) {

  companion object {
    val numSystemEventsPerBundle = 2
    val numSystemEventsConst =
        1 // Number of system event which have memory directly allocated on the ClockSynchronization
          // struct.

    fun getNumSystemEvents(numBundles: Int) =
        numSystemEventsPerBundle * numBundles + numSystemEventsConst

    val instName = "clock_sync"
  }

  private val numNeighbors = connectionGenerator.getNumFederatedConnectionBundles()
  private val numSystemEvents = getNumSystemEvents(numNeighbors)
  private val typeName = "Federate"

  fun generateSelfStruct() =
      "LF_DEFINE_CLOCK_SYNC_STRUCT(${typeName}, ${numNeighbors}, ${numSystemEvents})"

  fun generateCtor() =
      "LF_DEFINE_CLOCK_SYNC_CTOR(Federate, ${numNeighbors}, ${numSystemEvents }, ${federate.isGrandmaster})"

  fun generateFederateStructField() = "${typeName}ClockSynchronization ${instName};"

  fun generateFederateCtorCode() = "LF_INITIALIZE_CLOCK_SYNC(${typeName});"
}
