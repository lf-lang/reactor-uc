package org.lflang.generator.uc

import org.lflang.allStateVars
import org.lflang.isInitialized
import org.lflang.lf.Reactor
import org.lflang.lf.StateVar
import org.lflang.toText

class UcStateGenerator(private val reactor: Reactor) {
    private val StateVar.isArray
        get(): Boolean = type.cStyleArraySpec != null

    private val StateVar.arrayLength
        get(): Int = type.cStyleArraySpec.length

    fun generateReactorStructFields() =
        reactor.allStateVars.joinToString(prefix = "// State variables \n", separator = "\n") {
            if (it.isArray) {
                "${it.type.id} ${it.name}[${it.arrayLength}];"
            } else {
                "${it.type.toText()} ${it.name};"
            }
        }

    fun generateInitializeStateVars() =
        reactor.allStateVars.filter{it.isInitialized}.joinToString(prefix = "// Initialize State variables \n", separator = "\n") {
            if (it.isArray) {
                """|${it.type.id} _${it.name}_init[${it.arrayLength}] = ${it.init.expr.toCCode()};
                   |memcpy(&self->${it.name}, &_${it.name}_init, sizeof(_${it.name}_init));
                """.trimMargin()
            } else {
                "self->${it.name} = ${it.init.expr.toCCode()};"
            }
        }
}
