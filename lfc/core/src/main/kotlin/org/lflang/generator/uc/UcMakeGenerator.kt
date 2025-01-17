package org.lflang.generator.uc

import org.lflang.FileConfig
import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.getEventQueueSize
import org.lflang.generator.uc.UcReactorGenerator.Companion.getReactionQueueSize
import org.lflang.joinWithLn
import org.lflang.lf.Reactor
import org.lflang.target.property.BuildTypeProperty
import org.lflang.toUnixString
import java.nio.file.Path
import java.time.LocalDateTime
import kotlin.io.path.name
import kotlin.math.max

class UcMakeGenerator(private val main: Reactor, private val targetConfig: TargetConfig, private val fileConfig: FileConfig) {
    private val S = '$' // a little trick to escape the dollar sign with $S
    fun generateMake(sources: List<Path>) = with(PrependOperator) {
        val sources = sources.filterNot { it.name=="lf_main.c" }
        """
            | # Makefile generated for ${fileConfig.name}
            |LFC_GEN_SOURCES = \
        ${" |    "..sources.joinWithLn { it.toUnixString() + if (it != sources.last()) " \\" else ""}}
            |LFC_GEN_MAIN = lf_main.c
            |REACTION_QUEUE_SIZE = ${max(main.getReactionQueueSize(), 1)}
            |EVENT_QUEUE_SIZE = ${max(main.getEventQueueSize(), 1)}
            |
        """.trimMargin()
    }
}
