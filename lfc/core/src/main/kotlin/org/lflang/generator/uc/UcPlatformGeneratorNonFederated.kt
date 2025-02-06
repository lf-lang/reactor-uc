package org.lflang.generator.uc

import java.nio.file.Path
import org.lflang.reactor

class UcPlatformGeneratorNonFederated(generator: UcGenerator, override val srcGenPath: Path) :
    UcPlatformGenerator(generator) {

  override val buildPath = srcGenPath.resolve("build")
  override val targetName = fileConfig.name

    override fun generatePlatformFiles() {
        val mainGenerator = UcMainGeneratorNonFederated(mainReactor, generator.targetConfig, generator.fileConfig)
        val numEventsAndReactions = generator.totalNumEventsAndReactions(generator.mainDef.reactor)
        val cmakeGenerator = UcCmakeGeneratorNonFederated(generator.mainDef, targetConfig, generator.fileConfig, numEventsAndReactions.first, numEventsAndReactions.second)
        val makeGenerator = UcMakeGeneratorNonFederated(mainReactor, targetConfig, generator.fileConfig, numEventsAndReactions.first, numEventsAndReactions.second)
        super.doGeneratePlatformFiles(mainGenerator, cmakeGenerator, makeGenerator)
    }
}
