package org.lflang.generator.uc

import org.lflang.generator.uc.UcModeGenerator.Companion.usesModes
import org.lflang.lf.Reactor

/**
 * Build-time contribution made by a statically linked Reactor-UC runtime extension.
 *
 * Runtime behavior is described by the C `LfExtensionDescriptor`; this model only owns the
 * generated project's source embedding and CMake requirements.
 */
data class UcRuntimeExtension(
    val id: String,
    val environmentVariable: String,
    val cmakePathVariable: String,
    val embeddedDirectory: String,
    val artifacts: List<String>,
    val requiredRuntimeOptions: List<String>,
    val cmakeTarget: String,
)

object UcRuntimeExtensions {
  val micromode =
      UcRuntimeExtension(
          id = "micromode",
          environmentVariable = "MICROMODE_PATH",
          cmakePathVariable = "MICROMODE_PATH",
          embeddedDirectory = "micromode",
          artifacts = listOf("src", "include", "CMakeLists.txt"),
          requiredRuntimeOptions = listOf("LF_RUNTIME_EXTENSIONS"),
          cmakeTarget = "micromode::micromode",
      )

  fun requiredBy(reactor: Reactor): List<UcRuntimeExtension> =
      if (reactor.usesModes) listOf(micromode) else emptyList()
}
