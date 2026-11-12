package org.lflang.generator.uc

import java.nio.file.Path
import org.eclipse.emf.ecore.EObject
import org.eclipse.emf.ecore.resource.Resource
import org.eclipse.xtext.xbase.lib.IteratorExtensions
import org.lflang.allInstantiations
import org.lflang.allReactions
import org.lflang.generator.*
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasStartup
import org.lflang.lf.Instantiation
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider

/** Creates either a Federated or NonFederated generator depending on the type of LF program */
fun createUcGenerator(
    context: LFGeneratorContext,
    scopeProvider: LFGlobalScopeProvider
): UcGenerator {
  val nodes: Iterable<EObject> =
      IteratorExtensions.toIterable(context.getFileConfig().resource.getAllContents())
  for (reactor in nodes.filterIsInstance<Reactor>()) {
    if (reactor.isFederated) {
      return UcGeneratorFederated(context, scopeProvider)
    }
  }
  return UcGeneratorNonFederated(context, scopeProvider)
}

@Suppress("unused")
abstract class UcGenerator(
    val context: LFGeneratorContext,
    protected val scopeProvider: LFGlobalScopeProvider
) : GeneratorBase(context) {

  // keep a list of all source files we generate
  val ucSources = mutableListOf<Path>()
  val codeMaps = mutableMapOf<Path, CodeMap>()

  val fileConfig: UcFileConfig = context.fileConfig as UcFileConfig

  // Contains the maximum number of pending events required by each reactor variant.
  // Is updated as reactors are analyzed and code-generated.
  val maxNumPendingEvents = mutableMapOf<UcReactorVariant, Int>()
  val numberOfOutputConnections = mutableMapOf<Reactor, Int>()

  /**
   * The reactor variants (instantiations of reactors with concrete const-generic parameters and
   * type arguments) of the program. See [UcVariantAnalysis].
   */
  lateinit var variants: UcVariantAnalysis

  /** Analyze the reactor variants reachable from [top]. Returns false if errors were reported. */
  protected fun analyzeVariants(top: Reactor): Boolean {
    variants = UcVariantAnalysis(top, messageReporter)
    return !errorsOccurred()
  }

  // Compute the total number of events and reactions within a variant (and its children)
  // Also returns whether there is any startup event within the variant.
  private fun totalNumEventsAndReactionsRec(
      variant: UcReactorVariant
  ): Triple<Int, Int, Boolean> {
    var numEvents = 0
    var numReactions = 0
    var hasStartup = false
    for (inst in variant.reactor.allInstantiations) {
      val childRes = totalNumEventsAndReactionsRec(variant.child(inst))
      numEvents += childRes.first * variant.width(inst)
      numReactions += childRes.second * variant.width(inst)
      hasStartup = hasStartup or childRes.third
    }
    numEvents += maxNumPendingEvents[variant]!!
    numReactions += variant.reactor.allReactions.size
    hasStartup = hasStartup or variant.reactor.hasStartup
    return Triple(numEvents, numReactions, hasStartup)
  }

  // Compute the total number of events and reactions for a top-level reactor variant.
  fun totalNumEventsAndReactions(main: UcReactorVariant): Pair<Int, Int> {
    val res = totalNumEventsAndReactionsRec(main)
    return Pair(res.first + if (res.third) 1 else 0, res.second)
  }

  // Returns a possibly empty list of the federates in the current program.
  protected fun getAllFederates(): List<Instantiation> {
    val res = mutableListOf<Instantiation>()
    for (reactor in reactors) {
      if (reactor.isFederated) {
        res.addAll(reactor.allInstantiations)
      }
    }
    return res
  }

  protected fun getAllImportedResources(resource: Resource): Set<Resource> {
    val resources: MutableSet<Resource> = scopeProvider.getImportedResources(resource)
    val importedRresources = resources.subtract(setOf(resource))
    resources.addAll(importedRresources.map { getAllImportedResources(it) }.flatten())
    resources.add(resource)
    return resources
  }
}
