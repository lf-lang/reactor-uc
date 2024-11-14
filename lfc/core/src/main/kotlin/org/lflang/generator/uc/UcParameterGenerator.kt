package org.lflang.generator.uc

import org.lflang.inferredType
import org.lflang.joinWithLn
import org.lflang.lf.Instantiation
import org.lflang.lf.Parameter
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.toText

class UcParameterGenerator(private val reactor: Reactor) {

    companion object {

        val Parameter.targetType get(): String = this.inferredType.CType

        val Parameter.isPresentName get(): String = "__${this.name}"
    }

    fun generateReactorStructFields() =
            reactor.parameters.joinToString(prefix = "// Reactor parameters\n", separator = "\n", postfix = "\n") {"${it.inferredType.CType} ${it.name};"}

    fun generateReactorCtorCodes() =
            reactor.parameters.joinToString(prefix = "// Initialize Reactor parameters\n", separator = "\n", postfix = "\n") {
                """|
                    |self->${it.name} = ${it.name};
                """.trimMargin()
            }

    fun generateReactorCtorDefArguments() =
            reactor.parameters.joinToString() {", ${it.inferredType.CType} ${it.name}"}

    fun generateReactorCtorDeclArguments(r: Instantiation) =
            r.reactor.parameters.joinToString() {
                if (r.parameters.filter{ p -> p.lhs.name == it.name}.isEmpty()) {
                    ", ${it.init.expr.toCCode()}"
                } else {
                    ", ${r.parameters.find{ p -> p.lhs.name == it.name}!!.rhs.expr.toCCode()}"
                }
            }
}
