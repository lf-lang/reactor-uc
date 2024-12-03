package org.lflang.generator.uc

import org.lflang.allStateVars
import org.lflang.lf.Reactor
import org.lflang.toText

class UcStateGenerator(private val reactor: Reactor) {
    fun generateReactorStructFields() =
        reactor.allStateVars.joinToString(prefix = "// State variables \n", separator = "\n") { "${it.type.toText()} ${it.name};" }
}
