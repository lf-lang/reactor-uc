package org.lflang.generator.uc.federated

import org.lflang.generator.uc.UcCmakeGeneratorFederated
import org.lflang.generator.uc.UcMainGeneratorFederated
import org.lflang.generator.uc.UcMakeGeneratorFederated
import org.lflang.generator.uc.UcPlatformGenerator
import org.lflang.ir.Reactor
import org.lflang.target.property.NoCompileProperty
import org.lflang.target.property.PlatformProperty
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil
import java.nio.file.Path

class UcPlatformGeneratorFederated(
    generator: UcGeneratorFederated,
    override val srcGenPath: Path,
    private val federate: UcFederate,
    mainReactor: Reactor
) : UcPlatformGenerator(generator, mainReactor) {

  override val buildPath = srcGenPath.resolve("build")
  override val targetName: String = federate.codeType

  override fun generatePlatformFiles() {
    val generator = generator as UcGeneratorFederated
    val numEventsAndReactions = generator.totalNumEventsAndReactionsFederated(federate)
    val mainGenerator =
        UcMainGeneratorFederated(
            federate,
            generator.federates,
            generator.targetConfig,
            numEventsAndReactions.first,
            numEventsAndReactions.second,
            generator.fileConfig
        )
    val cmakeGenerator = UcCmakeGeneratorFederated(federate, targetConfig, generator.fileConfig)
    val makeGenerator = UcMakeGeneratorFederated(federate, targetConfig, generator.fileConfig)
    super.doGeneratePlatformFiles(mainGenerator, cmakeGenerator, makeGenerator)

    if (targetConfig.get(PlatformProperty.INSTANCE).platform == PlatformType.Platform.NATIVE &&
        !targetConfig.get(NoCompileProperty.INSTANCE)) {
      messageReporter.nowhere().info("Generating launch script for native federation.")
      generateLaunchScript()
    }
  }

  fun generateLaunchScript() {
    val generator = generator as UcGeneratorFederated
    val launchScriptGenerator = UcFederatedLaunchScriptGenerator(fileConfig)
    FileUtil.writeToFile(
        launchScriptGenerator.generateLaunchScript(generator.federates),
        fileConfig.binPath.resolve(fileConfig.name))
    fileConfig.binPath.resolve(fileConfig.name).toFile().setExecutable(true)
  }
}