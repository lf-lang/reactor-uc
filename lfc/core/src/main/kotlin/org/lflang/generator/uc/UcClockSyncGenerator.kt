package org.lflang.generator.uc

import org.lflang.*
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
    // Default values for clock sync params. Taken from linuxptp.
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

  // Number of neighbors is the same as the number of federated connection bundles.
  private val numNeighbors = connectionGenerator.getNumFederatedConnectionBundles()
  // We allocate three event per neighbor and an additional two events for the bookkeeping timer.
  val numSystemEvents = numNeighbors * 3 + 2

  // Clock sync is enabled by default. But can be disabled by a global target property (clock-sync:
  // off) or by
  // federate-specific annotation.
  val enabled =
      !federate.clockSyncParams.disabled &&
          targetConfig.getOrDefault(ClockSyncModeProperty.INSTANCE) !=
              ClockSyncModeType.ClockSyncMode.OFF
}
