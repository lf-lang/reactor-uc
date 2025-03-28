package org.lflang.generator.uc

import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.Paths
import kotlin.io.path.createSymbolicLinkPointingTo
import org.lflang.MessageReporter
import org.lflang.generator.CodeMap
import org.lflang.generator.GeneratorCommandFactory
import org.lflang.generator.LFGeneratorContext
import org.lflang.target.TargetConfig
import org.lflang.target.property.BuildTypeProperty
import org.lflang.target.property.type.BuildTypeType.BuildType
import org.lflang.toDefinition
import org.lflang.toUnixString
import org.lflang.util.FileUtil
import org.lflang.util.LFCommand

/** Abstract class for generating platform specific files and invoking the target compiler. */
abstract class UcPlatformGenerator(protected val generator: UcGenerator) {
  private val codeMaps = generator.codeMaps
  private val ucSources = generator.ucSources
  protected val messageReporter: MessageReporter = generator.messageReporter
  protected val fileConfig: UcFileConfig = generator.fileConfig
  protected val targetConfig: TargetConfig = generator.targetConfig
  private val commandFactory: GeneratorCommandFactory = generator.commandFactory
  protected val mainReactor = generator.mainDef.reactorClass.toDefinition()
  abstract val buildPath: Path
  abstract val srcGenPath: Path
  abstract val targetName: String

  private val relativeBinDir = fileConfig.outPath.relativize(fileConfig.binPath).toUnixString()

  abstract fun generatePlatformFiles()

  private val cmakeArgs: List<String>
    get() =
        listOf(
            "-DCMAKE_BUILD_TYPE=${targetConfig.get(BuildTypeProperty.INSTANCE)}",
        )

  companion object {
    fun buildTypeToCmakeConfig(type: BuildType) =
        when (type) {
          BuildType.TEST -> "Debug"
          else -> type.toString()
        }
  }

  fun doGeneratePlatformFiles(
      mainGenerator: UcMainGenerator,
      cmakeGenerator: UcCmakeGenerator,
      makeGenerator: UcMakeGenerator
  ) {
    val reactorUCEnvPath = System.getenv("REACTOR_UC_PATH")
    if (reactorUCEnvPath == null) {
      messageReporter
          .nowhere()
          .error(
              "REACTOR_UC_PATH environment variable not defined. Do source env.bash in reactor-uc")
      return
    }
    val runtimePath: Path = Paths.get(reactorUCEnvPath)

    val startSourceFile = Paths.get("lf_start.c")
    val startHeaderFile = Paths.get("lf_start.h")
    val mainSourceFile = Paths.get("lf_main.c")

    val startCodeMap = CodeMap.fromGeneratedCode(mainGenerator.generateStartSource())
    val mainCodeMap = CodeMap.fromGeneratedCode(mainGenerator.generateMainSource())

    ucSources.addAll(listOf(startSourceFile, mainSourceFile))
    codeMaps[srcGenPath.resolve(startSourceFile)] = startCodeMap
    codeMaps[srcGenPath.resolve(mainSourceFile)] = mainCodeMap

    FileUtil.writeToFile(startCodeMap.generatedCode, srcGenPath.resolve(startSourceFile), true)
    FileUtil.writeToFile(mainCodeMap.generatedCode, srcGenPath.resolve(mainSourceFile), true)
    FileUtil.writeToFile(
        mainGenerator.generateStartHeader(), srcGenPath.resolve(startHeaderFile), true)

    FileUtil.writeToFile(
        cmakeGenerator.generateIncludeCmake(ucSources), srcGenPath.resolve("Include.cmake"), true)
    FileUtil.writeToFile(
        cmakeGenerator.generateMainCmakeNative(), srcGenPath.resolve("CMakeLists.txt"), true)
    val runtimeDestinationPath: Path = srcGenPath.resolve("reactor-uc")
    if (fileConfig.runtimeSymlink) {
        runtimeDestinationPath.createSymbolicLinkPointingTo(runtimePath)
    } else {
      val entriesToCopy = listOf("src", "include", "external", "cmake", "make", "CMakeLists.txt")
      FileUtil.copyFilesOrDirectories(entriesToCopy.map { runtimePath.resolve(it).toString() }, runtimeDestinationPath, fileConfig, messageReporter, false)
    }

    FileUtil.writeToFile(
        makeGenerator.generateMake(ucSources), srcGenPath.resolve("Makefile"), true)
  }

  fun doCompile(context: LFGeneratorContext, onlyGenerateBuildFiles: Boolean = false): Boolean {
    // make sure the build directory exists
    Files.createDirectories(buildPath)

    val version = checkCmakeVersion()
    var parallelize = true
    if (version != null && version.compareVersion("3.12.0") < 0) {
      messageReporter
          .nowhere()
          .warning("CMAKE is older than version 3.12. Parallel building is not supported.")
      parallelize = false
    }

    if (version != null) {
      val cmakeReturnCode = runCmake(context)

      if (cmakeReturnCode == 0 && !onlyGenerateBuildFiles) {
        // If cmake succeeded, run make
        val makeCommand = createMakeCommand(buildPath, parallelize, targetName)
        val makeReturnCode =
            UcValidator(fileConfig, messageReporter, codeMaps)
                .run(makeCommand, context.cancelIndicator)
        var installReturnCode = 0
        if (makeReturnCode == 0) {
          val installCommand = createMakeCommand(buildPath, parallelize, "install")
          installReturnCode = installCommand.run(context.cancelIndicator)
          if (installReturnCode == 0) {
            println("SUCCESS (compiling generated C code)")
            println("Generated source code is in ${fileConfig.srcGenPath}")
            println("Compiled binary is in ${fileConfig.binPath}")
          }
        }
        if ((makeReturnCode != 0 || installReturnCode != 0) && !messageReporter.errorsOccurred) {
          // If errors occurred but none were reported, then the following message is the best we
          // can do.
          messageReporter.nowhere().error("make failed with error code $makeReturnCode")
        }
      }
      if (cmakeReturnCode != 0) {
        messageReporter.nowhere().error("cmake failed with error code $cmakeReturnCode")
      }
    }
    return !messageReporter.errorsOccurred
  }

  private fun checkCmakeVersion(): String? {
    // get the installed cmake version and make sure it is at least 3.5
    val cmd = commandFactory.createCommand("cmake", listOf("--version"), buildPath)
    var version: String? = null
    if (cmd != null && cmd.run() == 0) {
      val regex = "\\d+(\\.\\d+)+".toRegex()
      version = regex.find(cmd.output.toString())?.value
    }
    if (version == null || version.compareVersion("3.5.0") < 0) {
      messageReporter
          .nowhere()
          .error(
              "The uC target requires CMAKE >= 3.5.0 to compile the generated code. " +
                  "Auto-compiling can be disabled using the \"no-compile: true\" target property.")
      return null
    }

    return version
  }

  private fun runCmake(context: LFGeneratorContext): Int {
    val cmakeCommand = createCmakeCommand(buildPath, fileConfig.outPath)
    return cmakeCommand.run(context.cancelIndicator)
  }

  private fun String.compareVersion(other: String): Int {
    val a = this.split(".").map { it.toInt() }
    val b = other.split(".").map { it.toInt() }
    for (x in (a zip b)) {
      val res = x.first.compareTo(x.second)
      if (res != 0) return res
    }
    return 0
  }

  private fun getMakeArgs(buildPath: Path, parallelize: Boolean, target: String): List<String> {
    val cmakeConfig = buildTypeToCmakeConfig(targetConfig.get(BuildTypeProperty.INSTANCE))
    val makeArgs =
        mutableListOf(
            "--build", buildPath.fileName.toString(), "--config", cmakeConfig, "--target", target)

    if (parallelize) {
      makeArgs.addAll(listOf("--parallel", Runtime.getRuntime().availableProcessors().toString()))
    }

    return makeArgs
  }

  private fun createMakeCommand(buildPath: Path, parallelize: Boolean, target: String): LFCommand {
    val makeArgs = getMakeArgs(buildPath, parallelize, target)
    return commandFactory.createCommand("cmake", makeArgs, buildPath.parent)
  }

  private fun getCmakeArgs(buildPath: Path, outPath: Path, sourcesRoot: String? = null) =
      cmakeArgs +
          listOf(
              "-DCMAKE_INSTALL_PREFIX=${outPath.toUnixString()}",
              "-DCMAKE_INSTALL_BINDIR=$relativeBinDir",
              "--fresh",
              "-S",
              sourcesRoot ?: srcGenPath.toUnixString(),
              "-B",
              buildPath.fileName.toString())

  private fun createCmakeCommand(
      buildPath: Path,
      outPath: Path,
      sourcesRoot: String? = null
  ): LFCommand {
    val cmd =
        commandFactory.createCommand(
            "cmake", getCmakeArgs(buildPath, outPath, sourcesRoot), buildPath.parent)
    return cmd
  }
}
