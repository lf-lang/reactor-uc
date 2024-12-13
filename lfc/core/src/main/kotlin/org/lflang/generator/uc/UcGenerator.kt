package org.lflang.generator.uc

import org.eclipse.emf.ecore.resource.Resource
import org.lflang.allInstantiations
import org.lflang.generator.*
import org.lflang.generator.GeneratorUtils.canGenerate
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.isGeneric
import org.lflang.lf.Instantiation
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.Target
import org.lflang.target.property.*
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil
import java.nio.file.Path

@Suppress("unused")
class UcGenerator(
    val context: LFGeneratorContext,
    private val scopeProvider: LFGlobalScopeProvider
) :
    GeneratorBase(context) {

    // keep a list of all source files we generate
    val ucSources = mutableListOf<Path>()
    val codeMaps = mutableMapOf<Path, CodeMap>()

    val fileConfig: UcFileConfig = context.fileConfig as UcFileConfig
    val platform = targetConfig.get(PlatformProperty.INSTANCE)

    companion object {
        const val libDir = "/lib/c"
        const val MINIMUM_CMAKE_VERSION = "3.5"
    }



    // Returns a possibly empty list of the federates in the current program
    private fun getAllFederates(): List<Instantiation> {
        val res = mutableListOf<Instantiation>()
        for (reactor in reactors) {
            if (reactor.isFederated) {
                res.addAll(reactor.allInstantiations)
            }
        }
        return res
    }

    private fun getAllInstantiatedReactors(top: Reactor): List<Reactor> {
        val res = mutableListOf<Reactor>()
        for (inst in top.allInstantiations) {
            res.add(inst.reactor)
            res.addAll(getAllInstantiatedReactors(inst.reactor))
        }
        return res.distinct()
    }

    fun doGenerateTopLevel(resource: Resource, context: LFGeneratorContext, srcGenPath: Path, isFederated: Boolean): GeneratorResult.Status {
        if (!canGenerate(errorsOccurred(), mainDef, messageReporter, context)) return GeneratorResult.Status.FAILED

        // create a platform-specific generator
        val platformGenerator: UcPlatformGenerator = getPlatformGenerator(srcGenPath)

        // generate all core files
        generateFiles(mainDef, srcGenPath, getAllImportedResources(mainDef.eResource()))

        // generate platform specific files
        platformGenerator.generatePlatformFiles()

        if (platform.platform == PlatformType.Platform.NATIVE) {
            if (platformGenerator.doCompile(context)) {
                return GeneratorResult.Status.COMPILED
            } else {
                return GeneratorResult.Status.FAILED
            }
        } else {
            return GeneratorResult.Status.GENERATED
        }
    }


    override fun doGenerate(resource: Resource, context: LFGeneratorContext) {
        super.doGenerate(resource, context)

        val federates = getAllFederates();
        if (federates.isEmpty()) {
            val res = doGenerateTopLevel(resource, context, fileConfig.srcGenPath, false)
            if (res == GeneratorResult.Status.FAILED) {
                context.unsuccessfulFinish()
                return
            }
        } else {
            for (federate in federates) {
                mainDef = federate
                val res = doGenerateTopLevel(resource, context, fileConfig.srcGenPath.resolve(federate.name), true)

                if (res == GeneratorResult.Status.FAILED) {
                    context.unsuccessfulFinish()
                    return
                }
            }
        }
        if (platform.platform == PlatformType.Platform.NATIVE) {
            context.finish(GeneratorResult.Status.COMPILED, codeMaps)
        } else {
            context.finish(GeneratorResult.GENERATED_NO_EXECUTABLE.apply(context, codeMaps))
        }
    }

    private fun getAllImportedResources(resource: Resource): Set<Resource> {
        val resources: MutableSet<Resource> = scopeProvider.getImportedResources(resource)
        val importedRresources = resources.subtract(setOf(resource))
        resources.addAll(importedRresources.map { getAllImportedResources(it) }.flatten())
        resources.add(resource)
        return resources
    }

    private fun generateReactorFiles(reactor: Reactor, srcGenPath: Path) {
        val generator = UcReactorGenerator(reactor, fileConfig, messageReporter)
        val headerFile = fileConfig.getReactorHeaderPath(reactor)
        val sourceFile = fileConfig.getReactorSourcePath(reactor)
        val reactorCodeMap = CodeMap.fromGeneratedCode(generator.generateSource())
        if (!reactor.isGeneric)
            ucSources.add(sourceFile)
        codeMaps[srcGenPath.resolve(sourceFile)] = reactorCodeMap
        val headerCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
        codeMaps[srcGenPath.resolve(headerFile)] = headerCodeMap

        FileUtil.writeToFile(headerCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
        FileUtil.writeToFile(reactorCodeMap.generatedCode, srcGenPath.resolve(sourceFile), true)
    }

    private fun generateFederateFiles(federate: Instantiation, srcGenPath: Path) {
        // First thing is that we need to also generate the top-level federate reactor files
        generateReactorFiles(federate.reactor, srcGenPath)

        // Then we generate a reactor which wraps around the top-level reactor in the federate.
        val generator = UcFederateGenerator(federate, fileConfig, messageReporter)

        val headerFile = srcGenPath.resolve("Federate.h")
        val sourceFile = srcGenPath.resolve("Federate.c")
        val federateCodeMap = CodeMap.fromGeneratedCode(generator.generateSource())
        ucSources.add(sourceFile)
        codeMaps[srcGenPath.resolve(sourceFile)] = federateCodeMap
        val headerCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
        codeMaps[srcGenPath.resolve(headerFile)] = headerCodeMap

        FileUtil.writeToFile(headerCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
        FileUtil.writeToFile(federateCodeMap.generatedCode, srcGenPath.resolve(sourceFile), true)
    }

    private fun generateFiles(mainDef: Instantiation, srcGenPath: Path, resources: Set<Resource>) {
        // generate header and source files for all reactors
        getAllInstantiatedReactors(mainDef.reactor).map {generateReactorFiles(it, srcGenPath)}

        if (mainDef.isAFederate) {
            generateFederateFiles(mainDef, srcGenPath)
        }

        for (r in resources) {
            val generator = UcPreambleGenerator(r, fileConfig, scopeProvider)
            val headerFile = fileConfig.getPreambleHeaderPath(r);
            val preambleCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
            codeMaps[srcGenPath.resolve(headerFile)] = preambleCodeMap
            FileUtil.writeToFile(preambleCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
        }
    }

    private fun getPlatformGenerator(srcGenPath: Path) = UcStandaloneGenerator(this, srcGenPath)

    override fun getTarget() = Target.UC

    override fun getTargetTypes(): TargetTypes = UcTypes

}
