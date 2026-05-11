package org.lflang.generator.uc.espidf

import org.lflang.generator.uc.UcCmakeGeneratorNonFederated
import org.lflang.generator.uc.UcFileConfig
import org.lflang.lf.Instantiation

/** ESP-IDF specific CMake generator. Adds ESP-IDF specific linking options. */
class UcEspIdfCmakeGenerator(
    mainDef: Instantiation,
    fileConfig: UcFileConfig,
) : UcCmakeGeneratorNonFederated(mainDef, fileConfig) {

  override fun generateCmakeLinkingOptions() = "-Wno-unused-parameter"
}
