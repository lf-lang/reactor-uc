package org.lflang.generator.uc

import java.nio.file.Path
import org.lflang.reactor

class UcPlatformGeneratorNonFederated(generator: UcGenerator, override val srcGenPath: Path) :
    UcPlatformGenerator(generator) {

  override val buildPath = srcGenPath.resolve("build")
  override val targetName = fileConfig.name

  override fun generatePlatformFiles() {
    val numEventsAndReactions = generator.totalNumEventsAndReactions(generator.variants.top)

    // Use factory to create the appropriate platform-specific generators
    val mainGenerator =
        UcGeneratorFactory.createMainGenerator(
            generator.variants.top,
            numEventsAndReactions.first,
            numEventsAndReactions.second,
            generator.fileConfig,
        )

    val cmakeGenerator =
        UcGeneratorFactory.createCmakeGenerator(
            generator.mainDef,
            generator.fileConfig,
        )

    val makeGenerator = UcMakeGeneratorNonFederated(mainReactor, generator.fileConfig)

    super.doGeneratePlatformFiles(mainGenerator, cmakeGenerator, makeGenerator)
  }
}
