package org.lflang.generator.uc

import org.eclipse.emf.ecore.resource.Resource
import org.lflang.*
import org.lflang.lf.Reactor
import org.lflang.scoping.LFGlobalScopeProvider

class UcPreambleGenerator(
    private val resource: Resource,
    private val reactor: Reactor,
) {
    fun generateReactorPrivatePreamble() = reactor.preambles.joinToString(prefix= "// Private preambles\n", separator = "\n", postfix = "\n") { it.code.toText()}
    fun generateReactorPublicPreamble() = resource.model.preambles.joinToString(prefix = "// Public preambles\n", separator = "\n", postfix = "\n") {it.code.toText()}
}
