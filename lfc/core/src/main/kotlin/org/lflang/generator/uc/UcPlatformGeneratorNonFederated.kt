package org.lflang.generator.uc

import org.lflang.generator.CodeMap
import org.lflang.generator.LFGeneratorContext
import org.lflang.reactor
import org.lflang.target.property.BuildTypeProperty
import org.lflang.target.property.type.BuildTypeType.BuildType
import org.lflang.toUnixString
import org.lflang.util.FileUtil
import org.lflang.util.LFCommand
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.Paths
import kotlin.io.path.createSymbolicLinkPointingTo

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