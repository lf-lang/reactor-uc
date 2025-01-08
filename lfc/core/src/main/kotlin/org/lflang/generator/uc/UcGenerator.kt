package org.lflang.generator.uc

import org.apache.commons.lang3.tuple.MutablePair
import org.eclipse.emf.ecore.EObject
import org.eclipse.emf.ecore.resource.Resource
import org.eclipse.xtext.xbase.lib.IteratorExtensions
import org.lflang.allInstantiations
import org.lflang.allReactions
import org.lflang.generator.*
import org.lflang.generator.GeneratorUtils.canGenerate
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasStartup
import org.lflang.isGeneric
import org.lflang.lf.Instantiation
import org.lflang.lf.LfFactory
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.Target
import org.lflang.target.property.*
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil
import java.nio.file.Path

fun createUcGenerator(context: LFGeneratorContext, scopeProvider: LFGlobalScopeProvider): UcGenerator {
    val nodes: Iterable<EObject> = IteratorExtensions.toIterable(context.getFileConfig().resource.getAllContents());
    for (reactor in nodes.filterIsInstance<Reactor>()) {
        if (reactor.isFederated) {
            return UcFederatedGenerator(context, scopeProvider)
        }
    }
    return UcNonFederatedGenerator(context, scopeProvider)
}

@Suppress("unused")
abstract class UcGenerator(
    val context: LFGeneratorContext, protected val scopeProvider: LFGlobalScopeProvider
) : GeneratorBase(context) {

    // keep a list of all source files we generate
    val ucSources = mutableListOf<Path>()
    val codeMaps = mutableMapOf<Path, CodeMap>()

    val fileConfig: UcFileConfig = context.fileConfig as UcFileConfig
    val platform = targetConfig.get(PlatformProperty.INSTANCE)

    val maxNumPendingEvents = mutableMapOf<Reactor, Int>()


    private fun _totalNumEventsAndReactions(inst: Instantiation): Triple<Int, Int, Boolean> {
        var numEvents = 0
        var numReactions = 0
        var hasStartup = false
        val remaining = mutableListOf<Instantiation>()
        remaining.addAll(inst.reactor.allInstantiations)
        while (remaining.isNotEmpty()) {
            val child = remaining.removeFirst()
            val childRes = _totalNumEventsAndReactions(child)

            numEvents += childRes.first * child.width
            numReactions += childRes.second * child.width
            hasStartup = hasStartup or childRes.third
        }
        numEvents += maxNumPendingEvents[inst.reactor]!!
        numReactions += inst.reactor.allReactions.size
        hasStartup = hasStartup or inst.reactor.hasStartup
        return Triple(numEvents, numReactions, hasStartup)
    }

    fun totalNumEventsAndReactions(main: Reactor): Pair<Int, Int> {
        val res = MutablePair(maxNumPendingEvents[main]!!, main.allReactions.size)
        var hasStartup = main.hasStartup
        for (inst in main.allInstantiations) {
            val childRes = _totalNumEventsAndReactions(inst)
            res.left += childRes.first * inst.width
            res.right += childRes.second * inst.width
            hasStartup = hasStartup or childRes.third
        }
        if (hasStartup) res.left += 1
        return res.toPair()
    }


    companion object {
        const val libDir = "/lib/c"
        const val MINIMUM_CMAKE_VERSION = "3.5"
    }

    // Returns a possibly empty list of the federates in the current program

    fun getAllFederates(): List<Instantiation> {
        val res = mutableListOf<Instantiation>()
        for (reactor in reactors) {
            if (reactor.isFederated) {
                res.addAll(reactor.allInstantiations)
            }
        }
        return res
    }

    fun getAllUcFederates(): List<UcFederate> {
        val res = mutableListOf<UcFederate>()
        for (inst in getAllFederates()) {
            for (bankIdx in 0..<inst.width) {
                res.add(UcFederate(inst, bankIdx))
            }
        }
        return res
    }

    fun getAllInstantiatedReactors(top: Reactor): List<Reactor> {
        val res = mutableListOf<Reactor>()
        for (inst in top.allInstantiations) {
            res.add(inst.reactor)
            res.addAll(getAllInstantiatedReactors(inst.reactor))
        }
        return res.distinct()
    }
}

class UcNonFederatedGenerator(
    context: LFGeneratorContext, scopeProvider: LFGlobalScopeProvider
) : UcGenerator(context, scopeProvider) {


    fun doGenerateReactor(
        resource: Resource,
        context: LFGeneratorContext,
        srcGenPath: Path,
    ): GeneratorResult.Status {
        if (!canGenerate(errorsOccurred(), mainDef, messageReporter, context)) return GeneratorResult.Status.FAILED

        // generate header and source files for all reactors
        getAllInstantiatedReactors(mainDef.reactor).map { generateReactorFiles(it, srcGenPath) }

        generateReactorFiles(mainDef.reactor, srcGenPath)

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
            val platformGenerator = UcStandalonePlatformGenerator(this, fileConfig.srcGenPath)
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

    private fun getAllImportedResources(resource: Resource): Set<Resource> {
        val resources: MutableSet<Resource> = scopeProvider.getImportedResources(resource)
        val importedRresources = resources.subtract(setOf(resource))
        resources.addAll(importedRresources.map { getAllImportedResources(it) }.flatten())
        resources.add(resource)
        return resources
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

    override fun getTarget() = Target.UC
    override fun getTargetTypes(): TargetTypes = UcTypes
}

class UcFederatedGenerator(
    context: LFGeneratorContext, scopeProvider: LFGlobalScopeProvider
) : UcGenerator(context, scopeProvider) {

    private val nonFederatedGenerator = UcNonFederatedGenerator(context, scopeProvider)

    fun totalNumEventsAndReactionsFederated(federate: UcFederate): Pair<Int, Int> {
        val eventsFromFederatedConncetions = maxNumPendingEvents[mainDef.reactor]!!
        val eventsAndReactionsInFederate = nonFederatedGenerator.totalNumEventsAndReactions(federate.inst.reactor)
        return Pair(
            eventsFromFederatedConncetions + eventsAndReactionsInFederate.first,
            eventsAndReactionsInFederate.second
        )
    }

    fun doGenerateFederate(
        resource: Resource,
        context: LFGeneratorContext,
        srcGenPath: Path,
        federate: UcFederate
    ): GeneratorResult.Status {
        if (!canGenerate(
                errorsOccurred(),
                federate.inst,
                messageReporter,
                context
            )
        ) return GeneratorResult.Status.FAILED

        // generate all core files
        generateFiles(federate, srcGenPath, getAllImportedResources(resource))
        return GeneratorResult.Status.GENERATED
    }

    private fun clearStateFromPreviousFederate() {
        codeMaps.clear()
        ucSources.clear()
        maxNumPendingEvents.clear()
        nonFederatedGenerator.maxNumPendingEvents.clear()
        nonFederatedGenerator.codeMaps.clear()
        nonFederatedGenerator.ucSources.clear()
    }

    private fun createMainDef() {
        for (reactor in reactors) {
            if (reactor.isFederated) {
                this.mainDef = LfFactory.eINSTANCE.createInstantiation()
                this.mainDef.name = reactor.name
                this.mainDef.reactorClass = reactor
            }
        }
    }

    override fun doGenerate(resource: Resource, context: LFGeneratorContext) {
        super.doGenerate(resource, context)
        createMainDef()
        for (ucFederate in getAllUcFederates()) {
            clearStateFromPreviousFederate()

            val srcGenPath = if (ucFederate.isBank) {
                fileConfig.srcGenPath.resolve("${ucFederate.inst.name}_${ucFederate.bankIdx}")
            } else {
                fileConfig.srcGenPath.resolve(ucFederate.inst.name)
            }

            val platformGenerator = UcFederatedPlatformGenerator(this, srcGenPath, ucFederate)
            val res = doGenerateFederate(
                ucFederate.inst.eResource()!!,
                context,
                srcGenPath,
                ucFederate
            )

            if (res == GeneratorResult.Status.FAILED) {
                context.unsuccessfulFinish()
                return
            } else {
                // generate platform specific files
                platformGenerator.generatePlatformFiles()

                if (platform.platform == PlatformType.Platform.NATIVE) {
                    if (!platformGenerator.doCompile(context)) {
                        context.finish(GeneratorResult.Status.FAILED, codeMaps)
                        return
                    }
                }
            }
        }
        if (platform.platform == PlatformType.Platform.NATIVE) {
            context.finish(GeneratorResult.Status.COMPILED, codeMaps)
        } else {
            context.finish(GeneratorResult.Status.GENERATED, codeMaps)
        }
        return
    }

    private fun getAllImportedResources(resource: Resource): Set<Resource> {
        val resources: MutableSet<Resource> = scopeProvider.getImportedResources(resource)
        val importedRresources = resources.subtract(setOf(resource))
        resources.addAll(importedRresources.map { getAllImportedResources(it) }.flatten())
        resources.add(resource)
        return resources
    }

    private fun generateFederateFiles(federate: UcFederate, srcGenPath: Path) {
        // First thing is that we need to also generate the top-level federate reactor files
        nonFederatedGenerator.generateReactorFiles(federate.inst.reactor, srcGenPath)

        // Then we generate a reactor which wraps around the top-level reactor in the federate.
        val generator = UcFederateGenerator(federate, fileConfig, messageReporter)
        val top = federate.inst.eContainer() as Reactor

        // Record the number of events and reactions in this reactor
        maxNumPendingEvents.set(top, generator.getMaxNumPendingEvents())

        val headerFile = srcGenPath.resolve("lf_federate.h")
        val sourceFile = srcGenPath.resolve("lf_federate.c")
        val federateCodeMap = CodeMap.fromGeneratedCode(generator.generateSource())
        ucSources.add(sourceFile)
        codeMaps[srcGenPath.resolve(sourceFile)] = federateCodeMap
        val headerCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
        codeMaps[srcGenPath.resolve(headerFile)] = headerCodeMap

        FileUtil.writeToFile(headerCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
        FileUtil.writeToFile(federateCodeMap.generatedCode, srcGenPath.resolve(sourceFile), true)
    }

    private fun generateFiles(federate: UcFederate, srcGenPath: Path, resources: Set<Resource>) {
        // generate header and source files for all reactors
        getAllInstantiatedReactors(federate.inst.reactor).map {
            nonFederatedGenerator.generateReactorFiles(
                it,
                srcGenPath
            )
        }

        generateFederateFiles(federate, srcGenPath)

        // Collect the info on the generated sources
        ucSources.addAll(nonFederatedGenerator.ucSources)
        codeMaps.putAll(nonFederatedGenerator.codeMaps)

        for (r in resources) {
            val generator = UcPreambleGenerator(r, fileConfig, scopeProvider)
            val headerFile = fileConfig.getPreambleHeaderPath(r);
            val preambleCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
            codeMaps[srcGenPath.resolve(headerFile)] = preambleCodeMap
            FileUtil.writeToFile(preambleCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
        }
    }

    override fun getTarget() = Target.UC

    override fun getTargetTypes(): TargetTypes = UcTypes
}
