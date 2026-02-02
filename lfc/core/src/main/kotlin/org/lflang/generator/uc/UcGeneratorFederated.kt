package org.lflang.generator.uc

import java.nio.file.Path
import java.nio.file.Paths
import org.eclipse.emf.ecore.resource.Resource
import org.lflang.generator.CodeMap
import org.lflang.generator.GeneratorResult
import org.lflang.generator.GeneratorUtils.canGenerate
import org.lflang.generator.LFGeneratorContext
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.lf.LfFactory
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.property.ClockSyncModeProperty
import org.lflang.target.property.NoCompileProperty
import org.lflang.target.property.type.ClockSyncModeType
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil

class UcGeneratorFederated(context: LFGeneratorContext, scopeProvider: LFGlobalScopeProvider) :
    UcGenerator(context, scopeProvider) {

  private val nonFederatedGenerator = UcGeneratorNonFederated(context, scopeProvider)
  val federates = mutableListOf<UcFederate>()

  // Compute the total number of events and reactions within a federate. This reuses the
  // computation for the non-federated case, but federated connections are also considered.
  fun totalNumEventsAndReactionsFederated(federate: UcFederate): Pair<Int, Int> {
    val eventsFromFederatedConnections = maxNumPendingEvents[mainDef.reactor]!!
    val unaccountedNumberOfFlushReactions = numberOfOutputConnections[mainDef.reactor]!!
    val eventsAndReactionsInFederate =
        nonFederatedGenerator.totalNumEventsAndReactions(federate.inst.reactor)
    return Pair(
        eventsFromFederatedConnections + eventsAndReactionsInFederate.first,
        unaccountedNumberOfFlushReactions + eventsAndReactionsInFederate.second)
  }

  private fun doGenerateFederate(
      resource: Resource,
      context: LFGeneratorContext,
      srcGenPath: Path,
      federate: UcFederate
  ): GeneratorResult.Status {
    if (!canGenerate(errorsOccurred(), federate.inst, messageReporter, context))
        return GeneratorResult.Status.FAILED

    super.copyUserFiles(targetConfig, fileConfig)

    // generate header and source files for all reactors
    getAllInstantiatedReactors(federate.inst.reactor).map {
      nonFederatedGenerator.generateReactorFiles(it, srcGenPath)
    }

    generateFederateFiles(federate, srcGenPath)

    // Collect the info on the generated sources
    ucSources.addAll(nonFederatedGenerator.ucSources)
    codeMaps.putAll(nonFederatedGenerator.codeMaps)

    for (r in getAllImportedResources(resource)) {
      val generator = UcPreambleGenerator(r, fileConfig, scopeProvider)
      val headerFile = fileConfig.getPreambleHeaderPath(r)
      val preambleCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
      codeMaps[srcGenPath.resolve(headerFile)] = preambleCodeMap
      FileUtil.writeToFile(preambleCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
    }
    return GeneratorResult.Status.GENERATED
  }

  // The same UcGeneratorFederated is used to iteratively generated a project for each federate.
  // After generating we need to clear certain state variables before starting with the next
  // federate.
  private fun clearStateFromPreviousFederate() {
    codeMaps.clear()
    ucSources.clear()
    maxNumPendingEvents.clear()
    nonFederatedGenerator.maxNumPendingEvents.clear()
    nonFederatedGenerator.codeMaps.clear()
    nonFederatedGenerator.ucSources.clear()
  }

  // This function creates an instantiation for the top-level main reactor.
  private fun createMainDef() {
    for (reactor in reactors) {
      if (reactor
          .isFederated) { // Note that this will be the "main" top-level reactor. Not each federate.
        this.mainDef = LfFactory.eINSTANCE.createInstantiation()
        this.mainDef.name = reactor.name
        this.mainDef.reactorClass = reactor
      }
    }
  }

  private fun generateFederateTemplates() {

    // Generate top-level folder
    val projectsRoot = fileConfig.srcPkgPath.resolve(mainDef.name)
    FileUtil.createDirectoryIfDoesNotExist(projectsRoot.toFile())

    for (ucFederate in federates) {
      val projectTemplateGenerator =
          UcFederatedTemplateGenerator(
              mainDef, ucFederate, targetConfig, projectsRoot, messageReporter)
      projectTemplateGenerator.generateFiles()
    }
  }

  override fun doGenerate(resource: Resource, context: LFGeneratorContext) {
    super.doGenerate(resource, context)
    createMainDef()
    for (inst in getAllFederates()) {
      for (bankIdx in 0..<inst.width) {
        federates.add(UcFederate(inst, bankIdx))
      }
    }

    // Make sure we have a grandmaster
    if (targetConfig.getOrDefault(ClockSyncModeProperty.INSTANCE) !=
        ClockSyncModeType.ClockSyncMode.OFF &&
        federates.filter { it.clockSyncParams.grandmaster }.isEmpty()) {
      messageReporter
          .nowhere()
          .warning("No clock sync grandmaster specified. Selecting first federate as grandmaster.")
      federates.first().setGrandmaster()
    }

    if (context.args.generateFedTemplates) {
      generateFederateTemplates()
      return
    }

    for (ucFederate in federates) {
      clearStateFromPreviousFederate()

      val srcGenPath =
          if (ucFederate.isBank) {
            fileConfig.srcGenPath.resolve("${ucFederate.inst.name}_${ucFederate.bankIdx}")
          } else {
            fileConfig.srcGenPath.resolve(ucFederate.inst.name)
          }

      val platformGenerator = UcPlatformGeneratorFederated(this, srcGenPath, ucFederate)
      val res = doGenerateFederate(ucFederate.inst.eResource()!!, context, srcGenPath, ucFederate)

      if (res == GeneratorResult.Status.FAILED) {
        return
      } else {
        // generate platform specific files
        platformGenerator.generatePlatformFiles()

        if (platform.platform == PlatformType.Platform.NATIVE &&
            !targetConfig.get(NoCompileProperty.INSTANCE)) {

          if (!platformGenerator.doCompile(context)) {
            context.finish(GeneratorResult.Status.FAILED, codeMaps)
            return
          }
        }
      }
    }
    if (platform.platform == PlatformType.Platform.NATIVE &&
        !targetConfig.get(NoCompileProperty.INSTANCE)) {
      context.finish(GeneratorResult.Status.COMPILED, codeMaps)
    } else {
      context.finish(GeneratorResult.Status.GENERATED, codeMaps)
    }
    return
  }

  private fun generateFederateFiles(federate: UcFederate, srcGenPath: Path) {
    // First thing is that we need to also generate the top-level federate reactor files
    nonFederatedGenerator.generateReactorFiles(federate.inst.reactor, srcGenPath)

    // Then we generate a reactor which wraps around the top-level reactor in the federate.
    val generator =
        UcFederateGenerator(federate, federates, fileConfig, messageReporter, targetConfig)
    val top = federate.inst.eContainer() as Reactor

    // Record the number of events and reactions in this reactor
    maxNumPendingEvents[top] = generator.getMaxNumPendingEvents()
    numberOfOutputConnections[top] = generator.getNumberOfOutputConnections()

    val headerFile = Paths.get("lf_federate.h")
    val sourceFile = Paths.get("lf_federate.c")
    val federateCodeMap = CodeMap.fromGeneratedCode(generator.generateSource())
    ucSources.add(sourceFile)
    codeMaps[srcGenPath.resolve(sourceFile)] = federateCodeMap
    val headerCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
    codeMaps[srcGenPath.resolve(headerFile)] = headerCodeMap

    FileUtil.writeToFile(headerCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
    FileUtil.writeToFile(federateCodeMap.generatedCode, srcGenPath.resolve(sourceFile), true)
  }
}
