package org.lflang.generator.uc

import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeTypeFederate
import org.lflang.isBank
import org.lflang.lf.Instantiation

class UcFederate(val inst: Instantiation, val bankIdx: Int) {
    val isBank = inst.isBank
    private val interfaces = mutableListOf<UcNetworkInterface>()

    val codeType = if (isBank) "${inst.codeTypeFederate}_${bankIdx}" else inst.codeTypeFederate

    fun addInterface(iface: UcNetworkInterface) {
        interfaces.add(iface)
    }

    fun getInterface(name: String): UcNetworkInterface =
        interfaces.find { it.name == name }!!

    fun getDefaultInterface(): UcNetworkInterface =
        interfaces.first()

    fun getCompileDefs(): List<String> = interfaces.distinctBy { it.type }.map { it.compileDefs }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is UcFederate) return false

        val sameInst = inst == other.inst
        val sameBank = bankIdx == other.bankIdx
        return if (isBank) sameInst && sameBank else sameInst
    }
}
