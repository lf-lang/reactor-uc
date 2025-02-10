package org.lflang.generator.uc

import java.nio.file.Path
import kotlin.io.path.name
import kotlin.math.max
import org.lflang.FileConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.PrependOperator.rangeTo
import org.lflang.joinWithLn
import org.lflang.lf.Reactor
import org.lflang.target.TargetConfig
import org.lflang.toUnixString

abstract class UcMakeGenerator(
    private val mainTarget: String,
    private val numEvents: Int,
    private val numReactions: Int
) {
  abstract fun generateMake(sources: List<Path>): String

  protected val S = '$' // a little trick to escape the dollar sign with $S

  fun doGenerateMake(sources: List<Path>, compileDefs: List<String>) =
      with(PrependOperator) {
        val sources = sources.filterNot { it.name == "lf_main.c" }
        """
            |# Makefile generated for ${mainTarget}
            |LFC_GEN_SOURCES = \
        ${" |    "..sources.joinWithLn { it.toUnixString() + if (it != sources.last()) " \\" else "" }}
            |LFC_GEN_MAIN = lf_main.c
            |LFC_GEN_COMPILE_DEFS = \
        ${" |    "..compileDefs.joinWithLn { it + if (it != compileDefs.last()) " \\" else "" }}
            |REACTION_QUEUE_SIZE = ${max(numReactions, 1)}
            |EVENT_QUEUE_SIZE = ${max(numEvents, 2)}
            |
        """
            .trimMargin()
      }
}

class UcMakeGeneratorNonFederated(
    private val main: Reactor,
    private val targetConfig: TargetConfig,
    private val fileConfig: FileConfig,
    numEvents: Int,
    numReactions: Int
) : UcMakeGenerator(fileConfig.name, numEvents, numReactions) {
  override fun generateMake(sources: List<Path>) = doGenerateMake(sources, emptyList())
}

class UcMakeGeneratorFederated(
    private val federate: UcFederate,
    targetConfig: TargetConfig,
    fileConfig: UcFileConfig,
    numEvents: Int,
    numReactions: Int
) : UcMakeGenerator(federate.codeType, numEvents, numReactions) {
  override fun generateMake(sources: List<Path>): String {
    val channelTypes = federate.interfaces.map { it.type }.toSet()
    val channelTypesCompileDefs =
        channelTypes.joinWithLn {
          when (it) {
            NetworkChannelType.TCP_IP -> "CFLAGS += -DNETWORK_CHANNEL_TCP"
            NetworkChannelType.COAP_UDP_IP -> "CFLAGS += -DNETWORK_CHANNEL_COAP"
            NetworkChannelType.UART -> "CFLAGS += -DNETWORK_CHANNEL_UART"
            NetworkChannelType.NONE -> ""
            NetworkChannelType.CUSTOM -> ""
          }
        }

    return """
            |${doGenerateMake(sources, federate.getCompileDefs())}
            |${channelTypesCompileDefs}
        """
        .trimMargin()
  }
}
