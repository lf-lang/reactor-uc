package org.lflang.generator.uc2

import java.nio.file.Path
import org.apache.commons.lang3.tuple.MutablePair
import org.lflang.MessageReporter
import org.lflang.generator.LFGeneratorContext
import org.lflang.ir.Environment
import org.lflang.ir.Federate
import org.lflang.ir.File
import org.lflang.ir.ReactorInstantiation

import org.lflang.ir.Reactor
import org.lflang.scoping.LFGlobalScopeProvider

/** Creates either a Federated or NonFederated generator depending on the type of LF program */
public fun createUcGenerator(
  context: LFGeneratorContext,
  scopeProvider: LFGlobalScopeProvider,
  env: Environment,
  federate: Federate,
  messageReporter: MessageReporter,
): UcGenerator {
  if (env.isFederated) {
    // return UcGeneratorFederated(context, scopeProvider)
  } else {
    // return UcGeneratorNonFederated(context, scopeProvider)
  }
  return UcGeneratorNonFederated(env, federate, messageReporter, context, scopeProvider)
}

@Suppress("unused")
abstract class UcGenerator(
    val context: LFGeneratorContext,
    protected val scopeProvider: LFGlobalScopeProvider
) {

  // keep a list of all source files we generate
  val ucSources = mutableListOf<Path>()
  val fileConfig: UcFileConfig = context.fileConfig as UcFileConfig

  // Contains the maximum number of pending events required by each reactor.
  // Is updated as reactors are analyzed and code-generated.
  val maxNumPendingEvents = mutableMapOf<Reactor, Int>()

  // Compute the total number of events and reactions within an instance (and its children)
  // Also returns whether there is any startup event within the instance.
  private fun totalNumEventsAndReactions(inst: ReactorInstantiation): Triple<Int, Int, Boolean> {
    var numEvents = 0
    var numReactions = 0
    var hasStartup = false
    val remaining = mutableListOf<ReactorInstantiation>()
    remaining.addAll(inst.reactor.childReactors)
    while (remaining.isNotEmpty()) {
      val child = remaining.removeFirst()
      val childRes = totalNumEventsAndReactions(child)

      numEvents += childRes.first * child.codeWidth
      numReactions += childRes.second * child.codeWidth
      hasStartup = hasStartup or childRes.third
    }
    numEvents += maxNumPendingEvents[inst.reactor]!!
    numReactions += inst.reactor.reactions.size
    hasStartup = hasStartup or inst.reactor.hasStartup
    return Triple(numEvents, numReactions, hasStartup)
  }

  // Compute the total number of events and reactions for a top-level reactor.
  fun totalNumEventsAndReactions(main: Reactor): Pair<Int, Int> {
    val res = MutablePair(maxNumPendingEvents[main]!!, main.reactions.size)
    var hasStartup = main.hasStartup
    for (inst in main.childReactors) {
      val childRes = totalNumEventsAndReactions(inst)
      res.left += childRes.first * inst.codeWidth
      res.right += childRes.second * inst.codeWidth
      hasStartup = hasStartup or childRes.third
    }
    if (hasStartup) res.left += 1
    return res.toPair()
  }

  companion object {
    const val libDir = "/lib/c"
    const val MINIMUM_CMAKE_VERSION = "3.5"
  }

  // Returns a list of all instantiated reactors within a top-level reactor.
  protected fun getAllInstantiatedReactors(top: Reactor): List<Reactor> {
    val res = mutableListOf<Reactor>()
    for (inst in top.childReactors) {
      res.add(inst.reactor)
      res.addAll(getAllInstantiatedReactors(inst.reactor))
    }
    return res.distinct()
  }

  protected fun getAllImportedResources(resource: File, resources: MutableSet<File>) {
    resource.imports.forEach {
      if (!resources.contains(it.file)) {
        resources.add(it.file)
        getAllImportedResources(it.file, resources)
      }
    }
  }
}
