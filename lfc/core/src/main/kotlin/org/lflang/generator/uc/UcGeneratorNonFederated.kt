package org.lflang.generator.uc

import org.eclipse.emf.ecore.resource.Resource
import org.lflang.generator.CodeMap
import org.lflang.generator.GeneratorResult
import org.lflang.generator.GeneratorUtils.canGenerate
import org.lflang.generator.LFGeneratorContext
import org.lflang.isGeneric
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil
import java.nio.file.Path

class UcGeneratorNonFederated(
    context: LFGeneratorContext, scopeProvider: LFGlobalScopeProvider
) : UcGenerator(context, scopeProvider) {
    fun doGenerateReactor(
        resource: Resource,
        context: LFGeneratorContext,
        srcGenPath: Path,
    ): GeneratorResult.Status {
        if (!canGenerate(errorsOccurred(), mainDef, messageReporter, context)) return GeneratorResult.Status.FAILED

        // Generate header and source files for all instantiated reactors.
        getAllInstantiatedReactors(mainDef.reactor).map { generateReactorFiles(it, srcGenPath) }

        // Generate header and source files for the main reactor.
        generateReactorFiles(mainDef.reactor, srcGenPath)

        // Generate preambles for all reactors.
        for (r in getAllImportedResources(resource)) {
            val generator = UcPreambleGenerator(r, fileConfig, scopeProvider)
            val headerFile = fileConfig.getPreambleHeaderPath(r);
            val preambleCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
            codeMaps[srcGenPath.resolve(headerFile)] = preambleCodeMap
            FileUtil.writeToFile(preambleCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
        }
        return GeneratorResult.Status.GENERATED
    }


    override fun doGenerate(resource: Resource, context: LFGeneratorContext) {
        super.doGenerate(resource, context)

        if (getAllFederates().isNotEmpty()) {
            context.errorReporter.nowhere().error("Federated program detected in non-federated generator")
            return
        }

        val res = doGenerateReactor(
            resource,
            context,
            fileConfig.srcGenPath,
        )

        if (res == GeneratorResult.Status.GENERATED) {
            // generate platform specific files
            val platformGenerator = UcPlatformGeneratorNonFederated(this, fileConfig.srcGenPath)
            platformGenerator.generatePlatformFiles()

            if (platform.platform == PlatformType.Platform.NATIVE) {
                if (platformGenerator.doCompile(context)) {
                    context.finish(GeneratorResult.Status.COMPILED, codeMaps)
                } else {
                    context.finish(GeneratorResult.Status.FAILED, codeMaps)
                }
            } else {
                context.finish(GeneratorResult.GENERATED_NO_EXECUTABLE.apply(context, codeMaps))
            }
        } else {
            context.finish(GeneratorResult.Status.FAILED, codeMaps)
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

