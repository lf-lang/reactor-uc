package org.lflang.generator.uc

import java.nio.file.Path
import kotlin.io.path.name
import org.lflang.FileConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.federated.NetworkChannelType
import org.lflang.generator.uc.federated.UcFederate
import org.lflang.joinWithLn
import org.lflang.ir.Reactor
import org.lflang.target.TargetConfig
import org.lflang.toUnixString

abstract class UcMakeGenerator(
    private val mainTarget: String,
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
            |RUNTIME_PATH = $(CURDIR)/reactor-uc
        """
            .trimMargin()
      }
}

class UcMakeGeneratorNonFederated(
    private val main: Reactor,
    private val targetConfig: TargetConfig,
    private val fileConfig: FileConfig,
) : UcMakeGenerator(fileConfig.name) {
  override fun generateMake(sources: List<Path>) = doGenerateMake(sources, emptyList())
}

class UcMakeGeneratorFederated(
    private val federate: UcFederate,
    targetConfig: TargetConfig,
    fileConfig: UcFileConfig,
) : UcMakeGenerator(federate.codeType) {
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
