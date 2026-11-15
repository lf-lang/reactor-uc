package org.lflang.generator.uc

import org.lflang.AttributeUtils
import org.lflang.TimeValue
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeTypeFederate
import org.lflang.isBank
import org.lflang.lf.Attribute
import org.lflang.lf.Instantiation
import org.lflang.lf.Reactor
import org.lflang.target.PlatformType

class UcFederate(val inst: Instantiation, val bankIdx: Int) {
  val isBank = inst.isBank
  /**
   * The platform this federate is built for. A `@platform` attribute on the federate instantiation
   * takes precedence; otherwise the federate inherits the `@platform` of the enclosing federated
   * (main) reactor. Only if neither is annotated is the result [PlatformType.Platform.AUTO].
   */
  val platform: PlatformType.Platform =
      AttributeUtils.getPlatform(inst).takeIf { it != PlatformType.Platform.AUTO }
          ?: AttributeUtils.getPlatform(inst.eContainer() as Reactor)
  val interfaces = mutableListOf<UcNetworkInterface>()
  val codeType = if (isBank) "${inst.codeTypeFederate}_${bankIdx}" else inst.codeTypeFederate
  val name = if (isBank) "${inst.name}_${bankIdx}" else inst.name

  val clockSyncParams: UcClockSyncParameters =
      if (AttributeUtils.getClockSyncAttr(inst) != null)
          UcClockSyncParameters(AttributeUtils.getClockSyncAttr(inst))
      else UcClockSyncParameters()

  constructor(other: UcFederate) : this(other.inst, other.bankIdx)

  fun addInterface(iface: UcNetworkInterface) {
    interfaces.add(iface)
  }

  fun setGrandmaster() {
    clockSyncParams.grandmaster = true
  }

  fun getInterface(name: String): UcNetworkInterface = interfaces.find { it.name == name }!!

  fun getJoiningPolicy(): JoiningPolicy {
    val attr: Attribute? = AttributeUtils.getJoiningPolicy(inst)
    return attr
        ?.let { JoiningPolicy.parse(it.getAttrParms().get(0).getValue()) }
        .run { JoiningPolicy.JOIN_IMMEDIATELY }
  }

  fun getDefaultInterface(): UcNetworkInterface = interfaces.first()

  fun getCompileDefs(): List<String> =
      interfaces.distinctBy { it.type }.map { it.compileDefs } + "FEDERATED"

  fun getMaxWait(): TimeValue? = AttributeUtils.getMaxWaitInstance(inst)

  override fun equals(other: Any?): Boolean {
    if (this === other) return true
    if (other !is UcFederate) return false

    val sameInst = inst == other.inst
    val sameBank = bankIdx == other.bankIdx
    return if (isBank) sameInst && sameBank else sameInst
  }
}
