package org.lflang.generator.uc.espidf

import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcCmakeGeneratorNonFederated
import org.lflang.generator.uc.UcFileConfig
import org.lflang.joinWithLn
import org.lflang.lf.Instantiation
import org.lflang.target.TargetConfig
import org.lflang.target.property.BuildTypeProperty
import org.lflang.target.property.CmakeIncludeProperty
import org.lflang.target.property.LoggingProperty
import org.lflang.toUnixString
import java.nio.file.Path

/**
 * ESP-IDF specific CMake generator.
 * Adds ESP-IDF specific linking options.
 */
class UcEspIdfCmakeGenerator(
    mainDef: Instantiation,
    targetConfig: TargetConfig,
    fileConfig: UcFileConfig,
) : UcCmakeGeneratorNonFederated(mainDef, targetConfig, fileConfig) {
  
  override fun generateCmakeLinkingOptions() = "-Wno-unused-parameter"
}
