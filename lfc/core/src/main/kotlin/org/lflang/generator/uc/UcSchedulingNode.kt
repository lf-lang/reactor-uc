package org.lflang.generator.uc

import org.lflang.AttributeUtils
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeTypeFederate
import org.lflang.isBank
import org.lflang.lf.Instantiation
import org.lflang.target.property.type.PlatformType

enum class NodeType {
  ENCLAVE,
  FEDERATE
}

/**
 * A SchedulingNode is either a federate or an enclave. We use SchedulingNodes for disentangling
 * connections because federated and enclaved connections are handled similarly.
 */
open class UcSchedulingNode(val inst: Instantiation, val bankIdx: Int, val nodeType: NodeType) {
  val isBank = inst.isBank

  override fun equals(other: Any?): Boolean {
    if (this === other) return true
    if (other !is UcSchedulingNode) return false
    if (this.nodeType != other.nodeType) return false

    val sameInst = inst == other.inst
    val sameBank = bankIdx == other.bankIdx
    return if (isBank) sameInst && sameBank else sameInst
  }
}

class UcEnclave(inst: Instantiation, bankIdx: Int) :
    UcSchedulingNode(inst, bankIdx, NodeType.ENCLAVE) {}

class UcFederate(inst: Instantiation, bankIdx: Int) :
    UcSchedulingNode(inst, bankIdx, NodeType.FEDERATE) {
  val platform: PlatformType.Platform = AttributeUtils.getFederatePlatform(inst)
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

  fun getDefaultInterface(): UcNetworkInterface = interfaces.first()

  fun getCompileDefs(): List<String> =
      interfaces.distinctBy { it.type }.map { it.compileDefs } + "FEDERATED"
}
