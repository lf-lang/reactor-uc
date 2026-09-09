package org.lflang.generator.uc

import java.nio.file.Path
import org.apache.commons.lang3.tuple.MutablePair
import org.eclipse.emf.ecore.EObject
import org.eclipse.emf.ecore.resource.Resource
import org.eclipse.xtext.xbase.lib.IteratorExtensions
import org.lflang.AttributeUtils
import org.lflang.allInstantiations
import org.lflang.allReactions
import org.lflang.generator.*
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcModeGenerator.Companion.usesModes
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasStartup
import org.lflang.lf.Instantiation
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.PlatformType

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

  // Contains the maximum number of pending events required by each reactor.
  // Is updated as reactors are analyzed and code-generated.
  val maxNumPendingEvents = mutableMapOf<Reactor, Int>()
  val numberOfOutputConnections = mutableMapOf<Reactor, Int>()

  // Compute the total number of events and reactions within an instance (and its children)
  // Also returns whether there is any startup event within the instance.
  private fun totalNumEventsAndReactions(inst: Instantiation): Triple<Int, Int, Boolean> {
    var numEvents = 0
    var numReactions = 0
    var hasStartup = false
    val remaining = mutableListOf<Instantiation>()
    remaining.addAll(inst.reactor.allInstantiations)
    while (remaining.isNotEmpty()) {
      val child = remaining.removeFirst()
      val childRes = totalNumEventsAndReactions(child)

      numEvents += childRes.first * child.width
      numReactions += childRes.second * child.width
      hasStartup = hasStartup or childRes.third
    }
    numEvents += maxNumPendingEvents[inst.reactor]!!
    numReactions += inst.reactor.allReactions.size
    hasStartup = hasStartup or inst.reactor.hasStartup
    return Triple(numEvents, numReactions, hasStartup)
  }

  // Compute the total number of events and reactions for a top-level reactor.
  fun totalNumEventsAndReactions(main: Reactor): Pair<Int, Int> {
    val res = MutablePair(maxNumPendingEvents[main]!!, main.allReactions.size)
    var hasStartup = main.hasStartup
    for (inst in main.allInstantiations) {
      val childRes = totalNumEventsAndReactions(inst)
      res.left += childRes.first * inst.width
      res.right += childRes.second * inst.width
      hasStartup = hasStartup or childRes.third
    }
    if (hasStartup) res.left += 1
    return res.toPair()
  }

  /**
   * Reports every modal construct in [reactors] that the uC backend cannot express yet (see
   * [UcValidator.validateModes]) as a generator-stage error.
   */
  protected fun validateModalReactors() {
    for (reactor in reactors) {
      for (error in UcValidator.validateModes(reactor)) {
        messageReporter.nowhere().error(error)
      }
    }
    // RIOT and Patmos build from the Makefile UcMakeGenerator emits, not from the CMakeLists
    // beside it. A modal
    // program built through it would fail at the C compiler on the first lf_mode_t, with
    // nothing in the generator having said why. Refuse it instead. 
    val main: Reactor? = mainDef?.reactor
    if (main != null && main.usesModes) {
      val platform = AttributeUtils.getPlatform(main)
      if (platform == PlatformType.Platform.RIOT || platform == PlatformType.Platform.PATMOS) {
        messageReporter
            .nowhere()
            .error(
                "modes are not supported on the '$platform' platform, which builds from the " +
                    "generated Makefile. That Makefile names the reactor-uc runtime alone and " +
                    "has no way to reach micromode, which every modal program links. Use a " +
                    "CMake-driven platform, or remove the modes.")
      }
    }
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

  // Returns a list of all instantiated reactors within a top-level reactor.
  protected fun getAllInstantiatedReactors(top: Reactor): List<Reactor> {
    val res = mutableListOf<Reactor>()
    for (inst in top.allInstantiations) {
      res.add(inst.reactor)
      res.addAll(getAllInstantiatedReactors(inst.reactor))
    }
    return res.distinct()
  }

  protected fun getAllImportedResources(resource: Resource): Set<Resource> {
    val resources: MutableSet<Resource> = scopeProvider.getImportedResources(resource)
    val importedRresources = resources.subtract(setOf(resource))
    resources.addAll(importedRresources.map { getAllImportedResources(it) }.flatten())
    resources.add(resource)
    return resources
  }
}
