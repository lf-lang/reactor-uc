package org.lflang.generator.uc

import org.lflang.FileConfig
import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.joinWithLn
import org.lflang.lf.Reactor
import org.lflang.toUnixString
import java.nio.file.Path
import kotlin.io.path.name
import kotlin.math.max

abstract class UcMakeGenerator() {
    abstract fun generateMake(sources: List<Path>): String
}

class UcMakeGeneratorNonFederated(private val main: Reactor, private val targetConfig: TargetConfig, private val fileConfig: FileConfig, private val numEvents: Int, private val numReactions: Int): UcMakeGenerator() {
    private val S = '$' // a little trick to escape the dollar sign with $S
    override fun generateMake(sources: List<Path>) = with(PrependOperator) {
        val sources = sources.filterNot { it.name=="lf_main.c" }
        """
            | # Makefile generated for ${fileConfig.name}
            |LFC_GEN_SOURCES = \
        ${" |    "..sources.joinWithLn { it.toUnixString() + if (it != sources.last()) " \\" else ""}}
            |LFC_GEN_MAIN = lf_main.c
            |REACTION_QUEUE_SIZE = ${max(numReactions, 1)}
            |EVENT_QUEUE_SIZE = ${max(numEvents, 1)}
            |
        """.trimMargin()
    }
}
