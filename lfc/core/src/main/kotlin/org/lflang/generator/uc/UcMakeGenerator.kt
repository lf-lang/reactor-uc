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

class UcMakeGenerator(private val main: Reactor, private val targetConfig: TargetConfig, private val fileConfig: FileConfig) {
    private val S = '$' // a little trick to escape the dollar sign with $S
    fun generateMake(sources: List<Path>) = with(PrependOperator) {
        """
            | # Makefile genrated for ${fileConfig.name}
            |LF_SOURCES = \
        ${" |    "..sources.joinWithLn { it.toUnixString() + " \\ "}}
            |REACTION_QUEUE_SIZE = ${main.getReactionQueueSize()}
            |EVENT_QUEUE_SIZE = ${main.getEventQueueSize()}
            |
        """.trimMargin()
    }
}
