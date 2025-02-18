package org.lflang.generator.uc

import java.nio.file.Path
import org.lflang.reactor

class UcPlatformGeneratorNonFederated(generator: UcGenerator, override val srcGenPath: Path) :
    UcPlatformGenerator(generator) {

  override val buildPath = srcGenPath.resolve("build")
  override val targetName = fileConfig.name

  override fun generatePlatformFiles() {
    val numEventsAndReactions = generator.totalNumEventsAndReactions(generator.mainDef.reactor)
    val mainGenerator =
        UcMainGeneratorNonFederated(
            mainReactor,
            generator.targetConfig,
            numEventsAndReactions.first,
            numEventsAndReactions.second,
            0,
            generator.fileConfig)
    val cmakeGenerator =
        UcCmakeGeneratorNonFederated(generator.mainDef, targetConfig, generator.fileConfig)
    val makeGenerator = UcMakeGeneratorNonFederated(mainReactor, targetConfig, generator.fileConfig)
    super.doGeneratePlatformFiles(mainGenerator, cmakeGenerator, makeGenerator)
  }
}
