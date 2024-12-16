package org.lflang.generator.uc

import org.lflang.MessageReporter
import org.lflang.target.TargetConfig
import org.lflang.generator.GeneratorCommandFactory
import org.lflang.generator.LFGeneratorContext
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.reactor
import org.lflang.target.property.BuildTypeProperty
import org.lflang.toDefinition
import org.lflang.toUnixString
import java.nio.file.Path

/** Abstract class for generating platform specific files and invoking the target compiler. */
abstract class UcPlatformGenerator(protected val generator: UcGenerator) {
    protected val codeMaps = generator.codeMaps
    protected val ucSources = generator.ucSources
    protected val messageReporter: MessageReporter = generator.messageReporter
    protected val fileConfig: UcFileConfig = generator.fileConfig
    protected val targetConfig: TargetConfig = generator.targetConfig
    protected val commandFactory: GeneratorCommandFactory = generator.commandFactory
    protected val mainReactor = generator.mainDef.reactorClass.toDefinition()


    protected val relativeBinDir = fileConfig.outPath.relativize(fileConfig.binPath).toUnixString()

    abstract fun generatePlatformFiles()

    abstract fun doCompile(context: LFGeneratorContext, onlyGenerateBuildFiles: Boolean = false): Boolean

    protected val cmakeArgs: List<String>
        get() = listOf(
            "-DCMAKE_BUILD_TYPE=${targetConfig.get(BuildTypeProperty.INSTANCE)}",
        )

}
