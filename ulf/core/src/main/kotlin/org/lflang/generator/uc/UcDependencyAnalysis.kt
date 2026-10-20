package org.lflang.generator.uc

import org.eclipse.emf.ecore.EObject
import org.lflang.allInputs
import org.lflang.allOutputs
import org.lflang.ast.ASTUtils
import org.lflang.getWidth
import org.lflang.lf.BuiltinTrigger
import org.lflang.lf.BuiltinTriggerRef
import org.lflang.lf.Instantiation
import org.lflang.lf.Mode
import org.lflang.lf.Port
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.lf.VarRef
import org.lflang.lf.Variable
import org.lflang.reactor

/**
 * The dependencies between the reactions of a program, following the rules the runtime uses to
 * assign levels (`Reaction_calculate_level` in `src/reaction.c`):
 * - each reaction depends on the reaction declared before it in the same reactor;
 * - a reaction that reads a port depends on every reaction that writes a port on the chain of
 *   logical connections leading to it, intermediate ports included. Delayed, physical and
 *   cross-federate connections end the chain;
 * - except that a reaction never depends on a writer declared in another mode of the same reactor
 *   instance (see [areModeExclusive]): the two never execute in the same tag.
 *
 * Banks and multiports are expanded per instance and per channel, as at runtime. Widths are
 * evaluated as the generator does, if one cannot be determined, the analysis is [incomplete].
 * Members declared in modes are included at their textual position, as LF orders them.
 *
 * @param top The main or federated reactor of the program.
 */
class UcDependencyAnalysis(top: Reactor) {

  /** Reaction definitions with at least one instance on a dependency cycle. */
  val cycleReactions = mutableSetOf<Reaction>()

  /** Port definitions with at least one channel through which a cycle passes. */
  val cyclePorts = mutableSetOf<Variable>()

  /** Why the analysis could not cover the whole program, or null if it did. */
  var incomplete: String? = null
    private set

  private class Instance(val definition: Reactor) {
    val children = mutableMapOf<Instantiation, List<Instance>>()
    lateinit var reactions: List<ReactionNode>
  }

  private data class PortNode(val instance: Instance, val port: Port, val channel: Int)

  private class ReactionNode(val instance: Instance, val reaction: Reaction)

  /** A data dependency, with the ports it goes through as written and as read. */
  private class DataEdge(
      val writer: ReactionNode,
      val reader: ReactionNode,
      val written: Port,
      val read: Port
  )

  private val instances = mutableListOf<Instance>()
  private val writers = mutableMapOf<PortNode, MutableList<ReactionNode>>()
  private val upstream = mutableMapOf<PortNode, MutableList<PortNode>>()
  private val successors = mutableMapOf<ReactionNode, MutableSet<ReactionNode>>()
  private val dataEdges = mutableListOf<DataEdge>()

  init {
    val root = Instance(top)
    if (expand(root, listOf(top))) {
      instances.forEach { registerConnections(it) }
      instances.forEach { registerReactions(it) }
      findCycles()
    } else {
      cycleReactions.clear()
      cyclePorts.clear()
    }
  }

  /** Creates the instances below [instance]. Returns false if the program cannot be expanded. */
  private fun expand(instance: Instance, path: List<Reactor>): Boolean {
    instances.add(instance)
    instance.reactions =
        ASTUtils.allReactions(instance.definition).map { ReactionNode(instance, it) }
    for (instantiation in ASTUtils.allInstantiations(instance.definition)) {
      val definition = instantiation.reactor
      if (definition in path) {
        incomplete = "reactor '${definition.name}' instantiates itself"
        return false
      }
      val width = instantiation.widthSpec?.getWidth() ?: 1
      if (width < 0) {
        incomplete = "the width of '${instantiation.name}' cannot be determined statically"
        return false
      }
      instance.children[instantiation] = List(width) { Instance(definition) }
      for (child in instance.children[instantiation]!!) {
        if (!expand(child, path + definition)) return false
      }
    }
    for (port in instance.definition.allInputs + instance.definition.allOutputs) {
      if (port.widthSpec != null && port.widthSpec.getWidth() < 0) {
        incomplete = "the width of port '${port.name}' cannot be determined statically"
        return false
      }
    }
    return true
  }

  private fun widthOf(port: Port) = port.widthSpec?.getWidth() ?: 1

  /** All channels of the port [ref] refers to, from the point of view of [instance]. */
  private fun channelsOf(instance: Instance, ref: VarRef): List<PortNode> {
    val port = ref.variable as? Port ?: return emptyList()
    val owners = if (ref.container == null) listOf(instance) else instance.children[ref.container]
    return owners.orEmpty().flatMap { owner ->
      (0 until widthOf(port)).map { PortNode(owner, port, it) }
    }
  }

  /** Records, for each port channel, the channel its logical connection comes from. */
  private fun registerConnections(instance: Instance) {
    for (connection in ASTUtils.allConnections(instance.definition)) {
      if (connection.isPhysical || connection.delay != null) continue
      for (channel in UcConnectionChannels.parse(connection, emptyList())) {
        val crossFederate =
            instance.definition.isFederated &&
                (channel.src.varRef.container != channel.dest.varRef.container ||
                    channel.src.bankIdx != channel.dest.bankIdx)
        if (crossFederate) continue
        val src = nodeOf(instance, channel.src) ?: continue
        val dest = nodeOf(instance, channel.dest) ?: continue
        upstream.getOrPut(dest) { mutableListOf() }.add(src)
      }
    }
  }

  private fun nodeOf(instance: Instance, channel: UcChannel): PortNode? {
    val port = channel.varRef.variable as? Port ?: return null
    val owner =
        if (channel.varRef.container == null) instance
        else instance.children[channel.varRef.container]?.getOrNull(channel.bankIdx) ?: return null
    return PortNode(owner, port, channel.portIdx)
  }

  private fun registerReactions(instance: Instance) {
    for (node in instance.reactions) {
      for (effect in node.reaction.effects) {
        for (port in channelsOf(instance, effect)) {
          writers.getOrPut(port) { mutableListOf() }.add(node)
        }
      }
    }
    instance.reactions.zipWithNext { before, after -> addEdge(before, after) }
  }

  private fun addEdge(from: ReactionNode, to: ReactionNode) {
    successors.getOrPut(from) { mutableSetOf() }.add(to)
  }

  /** Adds the data dependencies of every reader, then finds the strongly connected components. */
  private fun findCycles() {
    for (instance in instances) {
      for (reader in instance.reactions) {
        val reads = reader.reaction.triggers.filterIsInstance<VarRef>() + reader.reaction.sources
        for (ref in reads) {
          for (channel in channelsOf(instance, ref)) {
            addDataEdges(reader, channel, ref.variable as Port)
          }
        }
      }
    }

    val components = stronglyConnectedComponents()
    val componentOf = mutableMapOf<ReactionNode, Int>()
    components.forEachIndexed { index, component -> component.forEach { componentOf[it] = index } }
    val cyclic =
        components.indices
            .filter { index ->
              val component = components[index]
              component.size > 1 ||
                  successors[component.first()]?.contains(component.first()) == true
            }
            .toSet()

    for (index in cyclic) {
      components[index].forEach { cycleReactions.add(it.reaction) }
    }
    for (edge in dataEdges) {
      val index = componentOf[edge.writer]
      if (index != null && index in cyclic && index == componentOf[edge.reader]) {
        cyclePorts.add(edge.written)
        cyclePorts.add(edge.read)
      }
    }
  }

  /**
   * Makes [reader] depend on every writer of a port on the connection chain ending at [channel].
   */
  private fun addDataEdges(reader: ReactionNode, channel: PortNode, read: Port) {
    val visited = mutableSetOf(channel)
    val pending = ArrayDeque(listOf(channel))
    while (pending.isNotEmpty()) {
      val port = pending.removeFirst()
      for (writer in writers[port].orEmpty()) {
        if (writer.instance === reader.instance &&
            areModeExclusive(writer.reaction, reader.reaction)) {
          continue
        }
        addEdge(writer, reader)
        dataEdges.add(DataEdge(writer, reader, port.port, read))
      }
      for (source in upstream[port].orEmpty()) {
        if (visited.add(source)) pending.add(source)
      }
    }
  }

  /** Tarjan's algorithm, iterative so that large programs cannot overflow the stack. */
  private fun stronglyConnectedComponents(): List<List<ReactionNode>> {
    val nodes = instances.flatMap { it.reactions }
    val index = mutableMapOf<ReactionNode, Int>()
    val lowLink = mutableMapOf<ReactionNode, Int>()
    val onStack = mutableSetOf<ReactionNode>()
    val stack = ArrayDeque<ReactionNode>()
    val components = mutableListOf<List<ReactionNode>>()
    var counter = 0

    for (start in nodes) {
      if (start in index) continue
      val work = ArrayDeque<Pair<ReactionNode, Iterator<ReactionNode>>>()
      fun visit(node: ReactionNode) {
        index[node] = counter
        lowLink[node] = counter
        counter++
        stack.add(node)
        onStack.add(node)
        work.add(node to successors[node].orEmpty().iterator())
      }
      visit(start)
      while (work.isNotEmpty()) {
        val (node, next) = work.last()
        if (next.hasNext()) {
          val successor = next.next()
          if (successor !in index) {
            visit(successor)
          } else if (successor in onStack) {
            lowLink[node] = minOf(lowLink[node]!!, index[successor]!!)
          }
          continue
        }
        work.removeLast()
        if (work.isNotEmpty()) {
          val parent = work.last().first
          lowLink[parent] = minOf(lowLink[parent]!!, lowLink[node]!!)
        }
        if (lowLink[node] == index[node]) {
          val component = mutableListOf<ReactionNode>()
          do {
            val member = stack.removeLast()
            onStack.remove(member)
            component.add(member)
          } while (member !== node)
          components.add(component)
        }
      }
    }
    return components
  }

  companion object {
    private fun modeOf(element: EObject): Mode? = element.eContainer() as? Mode

    private fun isShutdownReaction(element: EObject) =
        element is Reaction &&
            element.triggers.any { it is BuiltinTriggerRef && it.type == BuiltinTrigger.SHUTDOWN }

    /**
     * Whether two elements of the same reactor (reactions or connections) never act in the same
     * tag: they are declared in different modes of that reactor, and neither is a reaction
     * triggered by `shutdown`, which can run in an inactive mode at the stop tag.
     */
    @JvmStatic
    fun areModeExclusive(first: EObject, second: EObject): Boolean {
      val firstMode = modeOf(first) ?: return false
      val secondMode = modeOf(second) ?: return false
      if (firstMode === secondMode || firstMode.eContainer() !== secondMode.eContainer()) {
        return false
      }
      return !isShutdownReaction(first) && !isShutdownReaction(second)
    }
  }
}
