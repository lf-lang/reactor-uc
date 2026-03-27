package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

data object UcClockSyncMainState {
  const val OFF = 1
  const val INIT = 2
  const val ON = 3
}

class UcClockSyncMainAttribute(var state: Int = UcClockSyncMainState.INIT) {
  // Secondary constructor that initializes the data class from an Attribute object
  constructor(inst: Reactor) : this() {
    state = getStateFromReactor(inst)
  }

  fun getStateFromReactor(inst: Reactor): Int {
    return when (AttributeUtils.getClockSyncAttrValue(inst)) {
      "off" -> UcClockSyncMainState.OFF
      "on" -> UcClockSyncMainState.ON
      "init" -> UcClockSyncMainState.INIT
      else -> UcClockSyncMainState.INIT
    }
  }

  fun setState(inst: Reactor) {
    state = getStateFromReactor(inst)
  }
}
