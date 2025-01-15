package org.lflang.generator.uc

import org.lflang.reactor
import org.lflang.util.FileUtil
import java.nio.file.Path

class UcPlatformGeneratorFederated(generator: UcGeneratorFederated, override val srcGenPath: Path, private val federate: UcFederate) :
    UcPlatformGenerator(generator) {

    override val buildPath = srcGenPath.resolve("build")
    override val targetName: String = federate.codeType

    override fun generatePlatformFiles() {
        val generator = generator as UcGeneratorFederated
        val mainGenerator = UcMainGeneratorFederated(federate, generator.federates, generator.targetConfig, generator.fileConfig)
        val numEventsAndReactions = generator.totalNumEventsAndReactionsFederated(federate)
        val cmakeGenerator = UcCmakeGeneratorFederated(federate, targetConfig, generator.fileConfig, numEventsAndReactions.first, numEventsAndReactions.second)
        val makeGenerator = UcMakeGeneratorNonFederated(federate.inst.reactor, targetConfig, generator.fileConfig, numEventsAndReactions.first, numEventsAndReactions.second)
        super.doGeneratePlatformFiles(mainGenerator, cmakeGenerator, makeGenerator)

        generateLaunchScript()
    }

    fun generateLaunchScript() {
        val generator = generator as UcGeneratorFederated
        val launchScriptGenerator = UcFederatedLaunchScriptGenerator(fileConfig)
        FileUtil.writeToFile(launchScriptGenerator.generateLaunchScript(generator.federates), fileConfig.binPath.resolve(fileConfig.name))
        fileConfig.binPath.resolve(fileConfig.name).toFile().setExecutable(true)
    }
}