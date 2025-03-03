package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

fun TimeValue.toCCode() = UcTypes.getTargetTimeExpr(this)

fun Expression.toCCode(inferredType: InferredType? = null): String =
    UcTypes.getTargetExpr(this, inferredType)

fun Expression?.toCTime(): String = this?.toCCode(inferredType = InferredType.time()) ?: "0"

val InferredType.CType
  get() = UcTypes.getTargetType(this)

val VarRef.name: String
  get() = this.variable.name

val TriggerRef.name: String
  get() =
      when (this) {
        is VarRef -> this.name
        is BuiltinTriggerRef -> type.literal
        else -> unreachable()
      }

fun Attribute.getParamString(param: String): String? =
    attrParms.find { it.name == param }?.value?.trim('"')

fun Attribute.getParamInt(param: String): Int? = attrParms.find { it.name == param }?.value?.toInt()

fun Attribute.getParamBool(param: String): Boolean? =
    attrParms.find { it.name == param }?.value?.toBoolean()

fun Attribute.getParamFloat(param: String): Double? =
    attrParms.find { it.name == param }?.value?.toDouble()
