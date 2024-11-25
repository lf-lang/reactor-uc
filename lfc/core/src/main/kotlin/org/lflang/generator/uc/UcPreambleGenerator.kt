package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.Reactor

class UcPreambleGenerator(
    private val reactor: Reactor,
) {
    fun generateReactorPreamble() = reactor.preambles.joinToString(prefix= "// Reactor preambles\n", separator = "\n", postfix = "\n") { it.code.toText()}
}
