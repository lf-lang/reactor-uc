package org.lflang.generator.uc

import org.lflang.generator.uc.UcModeGenerator.Companion.usesModes
import org.lflang.lf.Reactor

/**
 * Build-time contribution made by a statically linked Reactor-UC runtime extension.
 *
 * Runtime behavior is described by the C `LfExtensionDescriptor`. This model owns only the
 * generated project's source embedding and its build requirements, for both build systems a
 * generated project ships: the CMakeLists and the Makefile beside it.
 */
data class UcRuntimeExtension(
    val id: String,
    /**
     * Variable naming this extension's location. Read from the environment to find a checkout to
     * embed, and set by both generated build files to the embedded copy.
     */
    val pathVariable: String,
    /** Directory the copy is embedded under, inside the generated project. */
    val embeddedDirectory: String,
    /** Entries copied out of the extension's checkout into that directory. */
    val artifacts: List<String>,
    /** Runtime CMake options that must be forced on before the runtime is configured. */
    val requiredRuntimeOptions: List<String>,
    /** Namespaced CMake target the generated program links. */
    val cmakeTarget: String,
    /**
     * Make fragment, relative to [embeddedDirectory], declaring the sources, include directories
     * and compile definitions a Makefile-driven platform needs. It is the Make counterpart of the
     * extension's CMakeLists, and the extension owns it.
     */
    val makeFragment: String,
)

object UcRuntimeExtensions {
  val micromode =
      UcRuntimeExtension(
          id = "micromode",
          pathVariable = "MICROMODE_PATH",
          embeddedDirectory = "micromode",
          artifacts = listOf("src", "include", "CMakeLists.txt", "micromode.mk"),
          requiredRuntimeOptions = listOf("LF_RUNTIME_EXTENSIONS"),
          cmakeTarget = "micromode::micromode",
          makeFragment = "micromode.mk",
      )

  fun requiredBy(reactor: Reactor): List<UcRuntimeExtension> =
      if (reactor.usesModes) listOf(micromode) else emptyList()
}
