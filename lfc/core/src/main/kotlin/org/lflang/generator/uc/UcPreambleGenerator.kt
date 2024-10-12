package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcPortGenerator.Companion.codeType
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.Instantiation
import org.lflang.lf.Port
import org.lflang.lf.Reactor
import org.lflang.validation.AttributeSpec

class UcPreambleGenerator(
    private val reactor: Reactor,
) {
    fun generateReactorPreamble() = reactor.preambles.joinToString(prefix= "// Reactor preambles\n", separator = "\n", postfix = "\n") { it.code.toText()}
}
