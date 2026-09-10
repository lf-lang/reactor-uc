package org.lflang.generator.uc

import java.nio.file.Path
import kotlin.io.path.name
import org.lflang.FileConfig
import org.lflang.generator.PrependOperator
import org.lflang.joinWithLn
import org.lflang.lf.Reactor
import org.lflang.toUnixString

/**
 * Emits the Makefile a generated project carries beside its CMakeLists, for the platforms that
 * build from Make rather than CMake.
 */
abstract class UcMakeGenerator(
    private val mainTarget: String,
) {
  abstract fun generateMake(sources: List<Path>, extensions: List<UcRuntimeExtension>): String

  protected val S = '$' // a little trick to escape the dollar sign with $S

  /**
   * Where each runtime extension was embedded, and the Make fragment describing it.
   *
   * A platform building from this Makefile includes every fragment in `LFC_GEN_EXTENSION_MKS` and
   * appends what they declare. `LFC_GEN_EXTENSIONS` names the extensions for a build system that
   * wants a module name instead, as RIOT does.
   */
  private fun generateExtensions(extensions: List<UcRuntimeExtension>): String {
    if (extensions.isEmpty()) return ""
    // Located from this Makefile rather than from CURDIR, because a platform fragment includes
    // it from the application directory rather than running make here. Exported so the sub-makes
    // RIOT spawns for its external modules see it too.
    val paths =
        extensions.joinWithLn {
          "export ${it.pathVariable} = $S(LFC_GEN_DIR)/${it.embeddedDirectory}"
        }
    val fragments = extensions.joinToString(" ") { "$S(${it.pathVariable})/${it.makeFragment}" }
    return "LFC_GEN_DIR := $S(patsubst %/,%,$S(dir $S(lastword $S(MAKEFILE_LIST))))\n" +
        "LFC_GEN_EXTENSIONS = ${extensions.joinToString(" ") { it.id }}\n" +
        "$paths\n" +
        "LFC_GEN_EXTENSION_MKS = $fragments"
  }

  fun doGenerateMake(
      sources: List<Path>,
      compileDefs: List<String>,
      extensions: List<UcRuntimeExtension>,
  ) =
      with(PrependOperator) {
        val genSources = sources.filterNot { it.name == "lf_main.c" }
        """
            |# Makefile generated for ${mainTarget}
            |LFC_GEN_SOURCES = \
        ${" |    "..genSources.joinWithLn { it.toUnixString() + if (it != genSources.last()) " \\" else "" }}
            |LFC_GEN_MAIN = lf_main.c
            |LFC_GEN_COMPILE_DEFS = \
        ${" |    "..compileDefs.joinWithLn { it + if (it != compileDefs.last()) " \\" else "" }}
            |RUNTIME_PATH = $S(CURDIR)/reactor-uc
        ${fuseNonEmpty(" |"..generateExtensions(extensions))}
        """
            .trimMargin()
      }
}

class UcMakeGeneratorNonFederated(
    private val main: Reactor,
    private val fileConfig: FileConfig,
) : UcMakeGenerator(fileConfig.name) {
  override fun generateMake(sources: List<Path>, extensions: List<UcRuntimeExtension>) =
      doGenerateMake(sources, emptyList(), extensions)
}

class UcMakeGeneratorFederated(
    private val federate: UcFederate,
    fileConfig: UcFileConfig,
) : UcMakeGenerator(federate.codeType) {
  override fun generateMake(sources: List<Path>, extensions: List<UcRuntimeExtension>): String {
    val channelTypes = federate.interfaces.map { it.type }.toSet()
    val channelTypesCompileDefs =
        channelTypes.joinWithLn {
          when (it) {
            NetworkChannelType.TCP_IP -> "CFLAGS += -DNETWORK_CHANNEL_TCP"
            NetworkChannelType.COAP_UDP_IP -> "CFLAGS += -DNETWORK_CHANNEL_COAP"
            NetworkChannelType.UART -> "CFLAGS += -DNETWORK_CHANNEL_UART"
            NetworkChannelType.S4NOC -> "CFLAGS += -DNETWORK_CHANNEL_S4NOC"
            NetworkChannelType.BLE -> "CFLAGS += -DNETWORK_CHANNEL_BLE"
            NetworkChannelType.NONE -> ""
            NetworkChannelType.CUSTOM -> ""
          }
        }

    return """
            |${doGenerateMake(sources, federate.getCompileDefs(), extensions)}
            |${channelTypesCompileDefs}
        """
        .trimMargin()
  }
}
