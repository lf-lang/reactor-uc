package org.lflang.generator.uc

import java.nio.file.Path
import org.lflang.AttributeUtils
import org.lflang.target.PlatformType
import org.lflang.util.FileUtil

class UcPlatformGeneratorFederated(
    generator: UcGeneratorFederated,
    override val srcGenPath: Path,
    private val federate: UcFederate,
) : UcPlatformGenerator(generator) {

  override val buildPath = srcGenPath.resolve("build")
  override val targetName: String = federate.codeType

  override fun generatePlatformFiles() {
    val generator = generator as UcGeneratorFederated
    val numEventsAndReactions = generator.totalNumEventsAndReactionsFederated(federate)
    val mainGenerator =
        UcMainGeneratorFederated(
            federate,
            generator.federates,
            numEventsAndReactions.first,
            numEventsAndReactions.second,
            generator.fileConfig,
        )
    val cmakeGenerator = UcCmakeGeneratorFederated(federate, generator.fileConfig)
    val makeGenerator = UcMakeGeneratorFederated(federate, generator.fileConfig)
    super.doGeneratePlatformFiles(mainGenerator, cmakeGenerator, makeGenerator)

    if (AttributeUtils.getPlatform(federate.inst.eContainer()) == PlatformType.Platform.NATIVE) {
      messageReporter.nowhere().info("Generating launch script for native federation.")
      generateLaunchScript()
    }
  }

  fun generateLaunchScript() {
    val generator = generator as UcGeneratorFederated
    val launchScriptGenerator = UcFederatedLaunchScriptGenerator(fileConfig)
    FileUtil.writeToFile(
        launchScriptGenerator.generateLaunchScript(generator.federates),
        fileConfig.binPath.resolve(fileConfig.name),
    )
    fileConfig.binPath.resolve(fileConfig.name).toFile().setExecutable(true)
  }
}
