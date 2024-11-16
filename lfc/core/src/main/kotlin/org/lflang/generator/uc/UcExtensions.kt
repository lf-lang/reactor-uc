package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.BuiltinTriggerRef
import org.lflang.lf.Expression
import org.lflang.lf.TriggerRef
import org.lflang.lf.VarRef

fun TimeValue.toCCode() = UcTypes.getTargetTimeExpr(this)
fun Expression.toCCode(inferredType: InferredType? = null): String =
    UcTypes.getTargetExpr(this, inferredType)
fun Expression?.toCTime(): String =
    this?.toCCode(inferredType = InferredType.time()) ?: "0"

val InferredType.CType
    get() = UcTypes.getTargetType(this)


val VarRef.name: String
    get() = this.variable.name


val TriggerRef.name: String
    get() = when (this) {
        is VarRef            -> this.name
        is BuiltinTriggerRef -> type.literal
        else                 -> unreachable()
    }
