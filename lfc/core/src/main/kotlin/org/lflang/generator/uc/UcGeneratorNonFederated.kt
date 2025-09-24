package org.lflang.generator.uc

import java.nio.file.Path
import org.lflang.generator.GeneratorResult
import org.lflang.generator.LFGeneratorContext
import org.lflang.ir.Environment
import org.lflang.ir.Federate
import org.lflang.ir.File
import org.lflang.ir.Reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.property.NoCompileProperty
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil

class UcGeneratorNonFederated(
    val env: Environment,
    val federate: Federate,
    context: LFGeneratorContext,
    scopeProvider: LFGlobalScopeProvider
) : UcGenerator(context, scopeProvider) {
  fun doGenerateReactor(
      file: File,
      context: LFGeneratorContext,
      srcGenPath: Path,
  ): GeneratorResult.Status {
    // if (!canGenerate(errorsOccurred(), mainDef, messageReporter, context))
    //    return GeneratorResult.Status.FAILED

    if (context.args.generateFedTemplates) {
      messageReporter
          .nowhere()
          .error("Cannot generate federate templates for non-federated program")
      return GeneratorResult.Status.FAILED
    }

    // Generate header and source files for all instantiated reactors.
    getAllInstantiatedReactors(federate.mainReactor).map { generateReactorFiles(it, srcGenPath) }

    // Generate header and source files for the main reactor.
    generateReactorFiles(federate.mainReactor, srcGenPath)

    // Generate preambles for all reactors.
    for (r in getAllImportedResources(resource)) {
      val generator = UcPreambleGenerator(r, fileConfig, scopeProvider)
      val headerFile = fileConfig.getPreambleHeaderPath(r)
      val preambleCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
      codeMaps[srcGenPath.resolve(headerFile)] = preambleCodeMap
      FileUtil.writeToFile(preambleCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
    }
    return GeneratorResult.Status.GENERATED
  }

  override fun doGenerate(file: File, context: LFGeneratorContext) {
    super.doGenerate(file, context)

    if (getAllFederates().isNotEmpty()) {
      context.errorReporter.nowhere().error("Federated program detected in non-federated generator")
      return
    }

    super.copyUserFiles(targetConfig, fileConfig)

    val res =
        doGenerateReactor(
            resource,
            context,
            fileConfig.srcGenPath,
        )

    if (res == GeneratorResult.Status.GENERATED) {
      // generate platform specific files
      val platformGenerator = UcPlatformGeneratorNonFederated(this, fileConfig.srcGenPath)
      platformGenerator.generatePlatformFiles()

      if (platform.platform == PlatformType.Platform.NATIVE &&
          !targetConfig.get(NoCompileProperty.INSTANCE)) {
        if (platformGenerator.doCompile(context)) {
          context.finish(GeneratorResult.Status.COMPILED, codeMaps)
        } else {
          context.finish(GeneratorResult.Status.FAILED, codeMaps)
        }
      } else {
        context.finish(GeneratorResult.GENERATED_NO_EXECUTABLE.apply(context, codeMaps))
      }
    } else {
      return
    }
  }

  fun generateReactorFiles(reactor: Reactor, srcGenPath: Path) {
    val generator = UcReactorGenerator(reactor, fileConfig, messageReporter)
    maxNumPendingEvents.set(reactor, generator.getMaxNumPendingEvents())

    val headerFile = fileConfig.getReactorHeaderPath(reactor)
    val sourceFile = fileConfig.getReactorSourcePath(reactor)
    val reactorCodeMap = CodeMap.fromGeneratedCode(generator.generateSource())
    if (!reactor.isGeneric) ucSources.add(sourceFile)
    codeMaps[srcGenPath.resolve(sourceFile)] = reactorCodeMap
    val headerCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
    codeMaps[srcGenPath.resolve(headerFile)] = headerCodeMap

    FileUtil.writeToFile(headerCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
    FileUtil.writeToFile(reactorCodeMap.generatedCode, srcGenPath.resolve(sourceFile), true)
  }
}
