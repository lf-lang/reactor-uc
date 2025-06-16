package org.lflang.ir

interface TargetType {
    fun toTargetCode(): String
}


class StringType : TargetType {
    override fun toTargetCode(): String = "char*"
}


