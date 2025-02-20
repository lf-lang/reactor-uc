package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

class UcClockSyncGenerator(
    private val federate: UcFederate,
    private val connectionGenerator: UcConnectionGenerator
) {

  companion object {
    val numSystemEventsPerBundle = 2
    val instName = "clock_sync"
  }

  private val numNeighbors = connectionGenerator.getNumFederatedConnectionBundles()
  private val typeName = "Federate"

  fun generateSelfStruct() = "LF_DEFINE_CLOCK_SYNC_STRUCT(${typeName}, ${numNeighbors})"

  fun generateCtor() =
      "LF_DEFINE_CLOCK_SYNC_STRUCT(Federate, ${numNeighbors}, ${federate.isGrandmaster})"

  fun generateFederateStructField() = "${typeName}ClockSynchronization ${instName};"

  fun generateFederateCtorCode() = "LF_INITIALIZE_CLOCK_SYNC(${typeName};"
}
