package org.lflang.generator.uc

import java.nio.file.Path
import org.eclipse.emf.ecore.resource.Resource
import org.lflang.AttributeUtils
import org.lflang.generator.CodeMap
import org.lflang.generator.GeneratorResult
import org.lflang.generator.GeneratorUtils.canGenerate
import org.lflang.generator.LFGeneratorContext
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.PlatformType
import org.lflang.util.FileUtil

class UcGeneratorNonFederated(context: LFGeneratorContext, scopeProvider: LFGlobalScopeProvider) :
    UcGenerator(context, scopeProvider) {
  fun doGenerateReactor(
      resource: Resource,
      context: LFGeneratorContext,
      srcGenPath: Path,
  ): GeneratorResult.Status {
    if (!canGenerate(errorsOccurred(), mainDef, messageReporter, context))
        return GeneratorResult.Status.FAILED

    if (context.args.generateFedTemplates) {
      messageReporter
          .nowhere()
          .error("Cannot generate federate templates for non-federated program")
      return GeneratorResult.Status.FAILED
    }

    // Find all variants of reactors (with concrete widths and types) in the program.
    if (!analyzeVariants(mainDef.reactor)) return GeneratorResult.Status.FAILED

    // Generate header and source files for all instantiated reactor variants.
    variants.instantiatedBelow(variants.top).map { generateReactorFiles(it, srcGenPath) }

    // Generate header and source files for the main reactor.
    generateReactorFiles(variants.top, srcGenPath)

    // Generate preambles for all reactors.
    for (r in getAllImportedResources(resource)) {
      val generator = UcPreambleGenerator(r, fileConfig, scopeProvider)
      val headerFile = fileConfig.getPreambleHeaderPath(r)
      val preambleCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
      codeMaps[srcGenPath.resolve(headerFile)] = preambleCodeMap
      FileUtil.writeToFile(preambleCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
    }
    return GeneratorResult.Status.SUCCESS
  }

  override fun doGenerate(resource: Resource, context: LFGeneratorContext) {
    super.doGenerate(resource, context)

    if (getAllFederates().isNotEmpty()) {
      context.errorReporter.nowhere().error("Federated program detected in non-federated generator")
      return
    }

    val res =
        doGenerateReactor(
            resource,
            context,
            fileConfig.srcGenPath,
        )

    if (res == GeneratorResult.Status.SUCCESS) {
      // generate platform specific files
      val platformGenerator = UcPlatformGeneratorNonFederated(this, fileConfig.srcGenPath)
      platformGenerator.generatePlatformFiles()

      if (AttributeUtils.getPlatform(mainDef.reactor) == PlatformType.Platform.NATIVE &&
          !context.args.noCompile) {
        if (platformGenerator.doCompile(context)) {
          context.finish(GeneratorResult.Status.SUCCESS, codeMaps)
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

  fun generateReactorFiles(variant: UcReactorVariant, srcGenPath: Path) {
    val generator = UcReactorGenerator(variant, fileConfig, messageReporter)
    maxNumPendingEvents.set(variant, generator.getMaxNumPendingEvents())

    val headerFile = fileConfig.getReactorHeaderPath(variant)
    val sourceFile = fileConfig.getReactorSourcePath(variant)
    val reactorCodeMap = CodeMap.fromGeneratedCode(generator.generateSource())
    ucSources.add(sourceFile)
    codeMaps[srcGenPath.resolve(sourceFile)] = reactorCodeMap
    val headerCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
    codeMaps[srcGenPath.resolve(headerFile)] = headerCodeMap

    FileUtil.writeToFile(headerCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
    FileUtil.writeToFile(reactorCodeMap.generatedCode, srcGenPath.resolve(sourceFile), true)
  }
}
