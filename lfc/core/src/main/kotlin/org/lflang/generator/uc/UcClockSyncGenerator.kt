package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*
import org.lflang.target.TargetConfig
import org.lflang.target.property.ClockSyncModeProperty
import org.lflang.target.property.type.ClockSyncModeType

data class UcClockSyncParameters(
    val disabled: Boolean = UcClockSyncParameters.DEFAULT_DISABLED,
    var grandmaster: Boolean = UcClockSyncParameters.DEFAULT_GRANDMASTER,
    val period: Int = UcClockSyncParameters.DEFAULT_PERIOD,
    val maxAdj: Int = UcClockSyncParameters.DEFAULT_MAX_ADJ,
    val Kp: Double = UcClockSyncParameters.DEFAULT_KP,
    val Ki: Double = UcClockSyncParameters.DEFAULT_KI,
) {
  companion object {
    const val DEFAULT_DISABLED = false
    const val DEFAULT_GRANDMASTER = false
    const val DEFAULT_PERIOD = 1000000000
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
      period = attr.getParamInt("period") ?: DEFAULT_PERIOD,
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
