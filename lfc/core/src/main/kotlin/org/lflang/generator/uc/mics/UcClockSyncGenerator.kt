package org.lflang.generator.uc.mics

import org.lflang.generator.uc.UcConnectionGenerator
import org.lflang.generator.uc.federated.UcFederate
import org.lflang.lf.*
import org.lflang.target.TargetConfig
import org.lflang.target.property.ClockSyncModeProperty
import org.lflang.target.property.type.ClockSyncModeType

data class UcClockSyncParameters(
    val disabled: Boolean = UcClockSyncParameters.DEFAULT_DISABLED,
    var grandmaster: Boolean = UcClockSyncParameters.DEFAULT_GRANDMASTER,
    val period: Long = UcClockSyncParameters.DEFAULT_PERIOD,
    val maxAdj: Int = UcClockSyncParameters.DEFAULT_MAX_ADJ,
    val Kp: Double = UcClockSyncParameters.DEFAULT_KP,
    val Ki: Double = UcClockSyncParameters.DEFAULT_KI,
) {
  companion object {
    const val DEFAULT_DISABLED = false
    const val DEFAULT_GRANDMASTER = false
    const val DEFAULT_PERIOD = 1000000000L
    const val DEFAULT_MAX_ADJ = 200000000
    const val DEFAULT_KP = 0.7
    const val DEFAULT_KI = 0.3
  }

  // Secondary constructor that initializes the data class from an Attribute object
  constructor(
      attr: Attribute
  ) : this(
      disabled = attr.getParamBool("disabled") ?: DEFAULT_DISABLED,
      grandmaster = attr.getParamBool("grandmaster") ?: DEFAULT_GRANDMASTER,
      period = attr.getParamBigInt("period") ?: DEFAULT_PERIOD,
      maxAdj = attr.getParamInt("max_adj") ?: DEFAULT_MAX_ADJ,
      Kp = attr.getParamFloat("kp") ?: DEFAULT_KP,
      Ki = attr.getParamFloat("ki") ?: DEFAULT_KI)
}

class UcClockSyncGenerator(
    private val federate: UcFederate,
    private val connectionGenerator: UcConnectionGenerator,
    private val targetConfig: TargetConfig
) {

  companion object {
    // The number of system events allocated for each neigbor. Used to schedule received messages as
    // system events.
    val numSystemEventsPerBundle = 3

    // The number of additional system events allocated. This system event is used for the periodic
    // SyncRequest event. The value must match the NUM_RESERVED_EVENTS compile def in
    // clock_synchronization.
    val numSystemEventsConst = 2

    // Returns the number of system events needed by the clock sync subsystem, given a number of
    // neighbors.
    fun getNumSystemEvents(numBundles: Int) =
        numSystemEventsPerBundle * numBundles + numSystemEventsConst

    val instName = "clock_sync"
  }

  private val numNeighbors = connectionGenerator.getNumFederatedConnectionBundles()
  private val numSystemEvents = getNumSystemEvents(numNeighbors)
  private val typeName = "Federate"
  private val clockSync = federate.clockSyncParams
  private val disabled = federate.clockSyncParams.disabled

  fun enabled() =
      !disabled &&
          targetConfig.getOrDefault(ClockSyncModeProperty.INSTANCE) !=
              ClockSyncModeType.ClockSyncMode.OFF

  fun generateSelfStruct() =
      if (enabled()) "LF_DEFINE_CLOCK_SYNC_STRUCT(${typeName}, ${numNeighbors}, ${numSystemEvents})"
      else ""

  fun generateCtor() =
      if (enabled())
          "LF_DEFINE_CLOCK_SYNC_CTOR(Federate, ${numNeighbors}, ${numSystemEvents }, ${clockSync.grandmaster}, ${clockSync.period}, ${clockSync.maxAdj}, ${clockSync.Kp}, ${clockSync.Ki});"
      else ""

  fun generateFederateStructField() =
      if (enabled()) "${typeName}ClockSynchronization ${instName};" else ""

  fun generateFederateCtorCode() = if (enabled()) "LF_INITIALIZE_CLOCK_SYNC(${typeName});" else ""
}
