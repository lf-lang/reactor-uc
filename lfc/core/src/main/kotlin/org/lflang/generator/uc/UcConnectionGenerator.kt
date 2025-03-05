package org.lflang.generator.uc

import java.util.*
import kotlin.collections.HashSet
import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

/**
 * This generator creates code for configuring the connections between reactors. This is perhaps the
 * most complicated part of the code-generator. This generator handles both federated and
 * non-federated programs
 */
class UcConnectionGenerator(
    private val reactor: Reactor, // The reactor to generator connections for
    private val currentFederate:
        UcFederate?, // The federate to generate connections for. If set then `reactor` should be
    // the top-level reactor.
    private val allFederates:
        List<UcFederate> // A list of all the federates in the program. Only used for federated
    // code-gen.
) {

  /** A list containing all non-federated gruoped connections within this reactor. */
  private val nonFederatedConnections: List<UcGroupedConnection>

  /**
   * A list containing all federated connection bundles (each containing grouped connections) within
   * this reactor, that has the current federate as a src or dest.
   */
  private val federatedConnectionBundles: List<UcFederatedConnectionBundle>

  private val isFederated = currentFederate != null

  /**
   * Given a LF connection and possibly the list of federates of the program. Create all the
   * ConnectionChannels found within the LF Connection. This must handle multiports, banks, iterated
   * connections and federated connections.
   */
  private fun parseConnectionChannels(
      conn: Connection,
      federates: List<UcFederate>
  ): List<UcConnectionChannel> {
    val res = mutableListOf<UcConnectionChannel>()
    val rhsPorts = conn.rightPorts.map { getChannelQueue(it, federates) }
    var rhsPortIndex = 0

    var lhsPorts = conn.leftPorts.map { getChannelQueue(it, federates) }
    var lhsPortIndex = 0

    // Keep parsing out connections until we are out of right-hand-side (rhs) ports
    while (rhsPortIndex < rhsPorts.size) {
      // First get the current lhs and rhs port and UcGroupedConnection that we are working with
      val lhsPort = lhsPorts[lhsPortIndex]
      val rhsPort = rhsPorts[rhsPortIndex]
      if (rhsPort.channelsLeft() > lhsPort.channelsLeft()) {
        val rhsChannelsToAdd = rhsPort.takeChannels(lhsPort.channelsLeft())
        val lhsChannelsToAdd = lhsPort.takeRemainingChannels()
        lhsChannelsToAdd.zip(rhsChannelsToAdd).forEach {
          res.add(UcConnectionChannel(it.first, it.second, conn))
        }
        lhsPortIndex += 1
      } else if (rhsPort.channelsLeft() < lhsPort.channelsLeft()) {
        val numRhsChannelsToAdd = rhsPort.channelsLeft()
        val rhsChannelsToAdd = rhsPort.takeRemainingChannels()
        val lhsChannelsToAdd = lhsPort.takeChannels(numRhsChannelsToAdd)
        lhsChannelsToAdd.zip(rhsChannelsToAdd).forEach {
          res.add(UcConnectionChannel(it.first, it.second, conn))
        }
        rhsPortIndex += 1
      } else {
        lhsPort.takeRemainingChannels().zip(rhsPort.takeRemainingChannels()).forEach {
          res.add(UcConnectionChannel(it.first, it.second, conn))
        }
        rhsPortIndex += 1
        lhsPortIndex += 1
      }

      // If we are out of lhs variables, but not rhs, then there should be an iterated connection.
      // We handle it by resetting the lhsChannels variable and index and continuing until
      // we have been through all rhs channels.
      if (lhsPortIndex >= lhsPorts.size && rhsPortIndex < rhsPorts.size) {
        assert(conn.isIterated)
        lhsPorts = conn.leftPorts.map { getChannelQueue(it, federates) }
        lhsPortIndex = 0
      }
    }
    return res
  }

  /**
   * Given a list of ConnectionChannels, group them together. How they are grouepd depends on
   * whether we are dealing with federated or non-federated reactors.
   */
  private fun groupConnections(channels: List<UcConnectionChannel>): List<UcGroupedConnection> {
    val res = mutableListOf<UcGroupedConnection>()
    val channels = HashSet(channels)

    while (channels.isNotEmpty()) {
      val c = channels.first()!!

      if (c.isFederated) {
        val grouped =
            channels.filter {
              it.conn.delayString == c.conn.delayString &&
                  it.conn.isPhysical == c.conn.isPhysical &&
                  it.src.varRef == c.src.varRef &&
                  it.src.federate == c.src.federate &&
                  it.dest.federate == c.dest.federate &&
                  it.getChannelType() == c.getChannelType()
            }

        val srcFed = allFederates.find { it == UcFederate(c.src.varRef.container, c.src.bankIdx) }!!
        val destFed =
            allFederates.find { it == UcFederate(c.dest.varRef.container, c.dest.bankIdx) }!!
        val groupedConnection =
            UcFederatedGroupedConnection(
                c.src.varRef,
                grouped,
                c.conn,
                srcFed,
                destFed,
            )

        res.add(groupedConnection)
        channels.removeAll(grouped.toSet())
      } else {
        val grouped =
            channels.filter {
              it.conn.delayString == c.conn.delayString &&
                  it.conn.isPhysical == c.conn.isPhysical &&
                  !it.isFederated &&
                  it.src.varRef == c.src.varRef &&
                  it.src.federate == c.src.federate
            }

        val groupedConnection = UcGroupedConnection(c.src.varRef, grouped, c.conn)
        res.add(groupedConnection)
        channels.removeAll(grouped.toSet())
      }
    }
    return res
  }

  /**
   * Given a port VarRef, and the list of federates. Create a channel queue. I.e. create all the
   * UcChannels and associate them with the correct federates.
   */
  private fun getChannelQueue(portVarRef: VarRef, federates: List<UcFederate>): UcChannelQueue {
    return if (portVarRef.container?.isAFederate ?: false) {
      val federates = allFederates.filter { it.inst == portVarRef.container }
      UcChannelQueue(portVarRef, federates)
    } else {
      UcChannelQueue(portVarRef, emptyList())
    }
  }

  companion object {
    private val Connection.delayString
      get(): String = this.delay.orNever().toCCode()

    /**
     * Whether we have initialized the UcFederates with NetworkInterfaces. This is only done once.
     */
    private var federateInterfacesInitialized = false
    /**
     * A global list of FederatedConnectionBundles. It is computed once and reused when
     * code-generating
     */
    private var allFederatedConnectionBundles: List<UcFederatedConnectionBundle> = emptyList()

    /**
     * This function takes a list of grouped connections and creates the necessary
     * FederatedConnectionBundles. The bundles are written to the global variable
     * allFederatedConnectionBundles and shared accross federates. Thus, this function should only
     * be called once during code-gen.
     */
    private fun createFederatedConnectionBundles(groupedConnections: List<UcGroupedConnection>) {
      val groupedSet = HashSet(groupedConnections)
      val bundles = mutableListOf<UcFederatedConnectionBundle>()

      // This while loop bundles GroupedConnections togheter until there are no more left.
      while (groupedSet.isNotEmpty()) {
        // Get an unbundled GroupedConnection
        val g = groupedSet.first()!!
        val toRemove = mutableListOf(g)
        if (g is UcFederatedGroupedConnection) {
          // Find all other unbundled FederatedGroupedConnection which go between the same two
          // federates
          val group =
              groupedSet.filterIsInstance<UcFederatedGroupedConnection>().filter {
                (it.srcFed == g.srcFed && it.destFed == g.destFed) ||
                    (it.srcFed == g.destFed && it.destFed == g.srcFed)
              }

          // Create a new ConnectionBundle with these groups
          bundles.add(UcFederatedConnectionBundle(g.srcFed, g.destFed, group))

          // Remove all those grouped connections.
          toRemove.addAll(group)
        }
        groupedSet.removeAll(toRemove.toSet())
      }
      allFederatedConnectionBundles = bundles
    }
  }

  init {
    // Only pass through all federates and add NetworkInterface objects to them once.
    if (isFederated && !federateInterfacesInitialized) {
      for (fed in allFederates) {
        UcNetworkInterfaceFactory.createInterfaces(fed).forEach { fed.addInterface(it) }
      }
      federateInterfacesInitialized = true
    }

    // Parse out all GroupedConnections. Note that this is repeated for each federate.
    val channels = mutableListOf<UcConnectionChannel>()
    reactor.allConnections.forEach { channels.addAll(parseConnectionChannels(it, allFederates)) }
    val grouped = groupConnections(channels)
    nonFederatedConnections = mutableListOf()
    federatedConnectionBundles = mutableListOf()

    if (isFederated) {
      // Only parse out federated connection bundles once for the very first federate
      if (allFederatedConnectionBundles.isEmpty()) {
        createFederatedConnectionBundles(grouped)
      }

      // Filter out the relevant bundles for this federate
      federatedConnectionBundles.addAll(
          allFederatedConnectionBundles.filter {
            it.src == currentFederate || it.dest == currentFederate
          })
      // Add all non-federated connections (e.g. a loopback connection)
      nonFederatedConnections.addAll(
          grouped
              .filterNot { it is UcFederatedGroupedConnection }
              .filter {
                it.channels.fold(true) { acc, c -> acc && (c.src.federate == currentFederate) }
              })
    } else {
      // In the non-federated case, all grouped connections are handled togehter.
      nonFederatedConnections.addAll(grouped)
    }

    // Assign a unique ID to each connection to avoid possible naming conflicts in the generated
    // code.
    val allGroupedConnections =
        federatedConnectionBundles
            .map { it.groupedConnections }
            .flatten()
            .plus(nonFederatedConnections)
    allGroupedConnections.forEachIndexed { idx, el -> el.assignUid(idx) }
  }

  fun getNumFederatedConnectionBundles() = federatedConnectionBundles.size

  fun getNumConnectionsFromPort(instantiation: Instantiation?, port: Port): Int {
    var count = 0
    // Find all outgoing non-federated grouped connections from this port
    for (groupedConn in nonFederatedConnections) {
      if (groupedConn.srcInst == instantiation && groupedConn.srcPort == port) {
        count += 1
      }
    }

    // Find all outgoing federated grouped connections from this port.
    for (federatedConnectionBundle in federatedConnectionBundles) {
      for (groupedConn in federatedConnectionBundle.groupedConnections) {
        if (groupedConn.srcFed == currentFederate &&
            groupedConn.srcInst == instantiation &&
            groupedConn.srcPort == port) {
          count += 1
        }
      }
    }
    return count
  }

  private fun generateLogicalSelfStruct(conn: UcGroupedConnection) =
      "LF_DEFINE_LOGICAL_CONNECTION_STRUCT(${reactor.codeType},  ${conn.getUniqueName()}, ${conn.numDownstreams()});"

  private fun generateLogicalCtor(conn: UcGroupedConnection) =
      "LF_DEFINE_LOGICAL_CONNECTION_CTOR(${reactor.codeType},  ${conn.getUniqueName()}, ${conn.numDownstreams()});"

  private fun generateDelayedSelfStruct(conn: UcGroupedConnection) =
      "LF_DEFINE_DELAYED_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay});"

  private fun generateFederatedInputSelfStruct(conn: UcFederatedGroupedConnection) =
      "LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents});"

  private fun generateFederatedInputCtor(conn: UcFederatedGroupedConnection) =
      "LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay}, ${conn.isPhysical}, ${conn.getMaxWait().toCCode()});"

  private fun generateDelayedCtor(conn: UcGroupedConnection) =
      "LF_DEFINE_DELAYED_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay}, ${conn.isPhysical});"

  private fun generateFederatedOutputSelfStruct(conn: UcFederatedGroupedConnection) =
      "LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()});"

  private fun generateFederatedOutputCtor(conn: UcFederatedGroupedConnection) =
      "LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()}, ${conn.getDestinationConnectionId()});"

  private fun generateFederatedConnectionSelfStruct(conn: UcFederatedGroupedConnection) =
      if (conn.srcFed == currentFederate) generateFederatedOutputSelfStruct(conn)
      else generateFederatedInputSelfStruct(conn)

  private fun generateFederatedConnectionCtor(conn: UcFederatedGroupedConnection) =
      if (conn.srcFed == currentFederate) generateFederatedOutputCtor(conn)
      else generateFederatedInputCtor(conn)

  private fun generateFederatedOutputInstance(conn: UcGroupedConnection) =
      "LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(${reactor.codeType}, ${conn.getUniqueName()});"

  private fun generateFederatedInputInstance(conn: UcGroupedConnection) =
      "LF_FEDERATED_INPUT_CONNECTION_INSTANCE(${reactor.codeType}, ${conn.getUniqueName()});"

  private fun generateFederatedConnectionInstance(conn: UcFederatedGroupedConnection) =
      if (conn.srcFed == currentFederate) generateFederatedOutputInstance(conn)
      else generateFederatedInputInstance(conn)

  private fun generateInitializeFederatedOutput(conn: UcFederatedGroupedConnection) =
      "LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.serializeFunc});"

  private fun generateInitializeFederatedInput(conn: UcFederatedGroupedConnection) =
      "LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.deserializeFunc});"

  private fun generateInitializeFederatedConnection(conn: UcFederatedGroupedConnection) =
      if (conn.srcFed == currentFederate) generateInitializeFederatedOutput(conn)
      else generateInitializeFederatedInput(conn)

  private fun generateReactorCtorCode(conn: UcGroupedConnection) =
      """
        |${if (conn.isLogical) "LF_INITIALIZE_LOGICAL_CONNECTION(" else "LF_INITIALIZE_DELAYED_CONNECTION("}${reactor.codeType}, ${conn.getUniqueName()}, ${conn.bankWidth}, ${conn.portWidth});
        """
          .trimMargin()

  private fun generateFederateCtorCode(conn: UcFederatedConnectionBundle) =
      "LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(${conn.src.codeType}, ${conn.dest.codeType});"

  private fun generateConnectChannel(
      groupedConn: UcGroupedConnection,
      channel: UcConnectionChannel
  ) =
      """|lf_connect((Connection *) &self->${groupedConn.getUniqueName()}[${channel.src.getCodeBankIdx()}][${channel.src.getCodePortIdx()}], (Port *) ${channel.src.generateChannelPointer()}, (Port *) ${channel.dest.generateChannelPointer()});
    """
          .trimMargin()

  private fun generateConnectionStatements(conn: UcGroupedConnection) =
      conn.channels.joinToString(separator = "\n") { generateConnectChannel(conn, it) }

  private fun generateConnectFederateOutputChannel(
      bundle: UcFederatedConnectionBundle,
      conn: UcFederatedGroupedConnection
  ) =
      conn.channels.joinWithLn {
        "lf_connect_federated_output((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeType}, ${bundle.dest.codeType}).${conn.getUniqueName()}, (Port*) &self->${it.src.federate!!.inst.name}[0].${it.src.varRef.name}[${it.src.portIdx}]);"
      }

  private fun generateConnectFederateInputChannel(
      bundle: UcFederatedConnectionBundle,
      conn: UcGroupedConnection
  ) =
      conn.channels.joinWithLn {
        "lf_connect_federated_input((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeType}, ${bundle.dest.codeType}).${conn.getUniqueName()}, (Port*) &self->${it.dest.federate!!.inst.name}[0].${it.dest.varRef.name}[${it.dest.portIdx}]);"
      }

  private fun generateFederateConnectionStatements(conn: UcFederatedConnectionBundle) =
      conn.groupedConnections.joinWithLn {
        if (it.srcFed == currentFederate) {
          generateConnectFederateOutputChannel(conn, it)
        } else {
          generateConnectFederateInputChannel(conn, it)
        }
      }

  fun generateFederateCtorCodes() =
      federatedConnectionBundles.joinToString(
          prefix = "// Initialize connection bundles\n", separator = "\n", postfix = "\n") {
            generateFederateCtorCode(it)
          } +
          federatedConnectionBundles.joinToString(
              prefix = "// Do connections \n", separator = "\n", postfix = "\n") {
                generateFederateConnectionStatements(it)
              }

  fun generateReactorCtorCodes() =
      nonFederatedConnections.joinToString(
          prefix = "// Initialize connections\n", separator = "\n", postfix = "\n") {
            generateReactorCtorCode(it)
          } +
          nonFederatedConnections.joinToString(
              prefix = "// Do connections \n", separator = "\n", postfix = "\n") {
                generateConnectionStatements(it)
              }

  fun generateCtors() =
      nonFederatedConnections.joinToString(
          prefix = "// Connection constructors\n", separator = "\n", postfix = "\n") {
            if (it.isDelayed) generateDelayedCtor(it) else generateLogicalCtor(it)
          }

  fun generateSelfStructs() =
      nonFederatedConnections.joinToString(
          prefix = "// Connection structs\n", separator = "\n", postfix = "\n") {
            if (it.isLogical) generateLogicalSelfStruct(it) else generateDelayedSelfStruct(it)
          }

  private fun generateFederatedConnectionBundleSelfStruct(bundle: UcFederatedConnectionBundle) =
      with(PrependOperator) {
        """ |typedef struct {
            |  FederatedConnectionBundle super;
       ${"  |  "..bundle.networkChannel.codeType} channel;
       ${"  |  "..bundle.groupedConnections.joinWithLn { generateFederatedConnectionInstance(it) }}
            |  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(${bundle.numInputs(currentFederate!!)}, ${
                bundle.numOutputs(
                    currentFederate
                )
            });
            |} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(${bundle.src.codeType}, ${bundle.dest.codeType});
            |
        """
            .trimMargin()
      }

  private fun generateFederatedConnectionBundleCtor(bundle: UcFederatedConnectionBundle) =
      with(PrependOperator) {
        """ |LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(${bundle.src.codeType}, ${bundle.dest.codeType}) {
            |  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
            |  ${bundle.generateNetworkChannelCtor(currentFederate!!)}
            |  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
       ${"  |  "..bundle.groupedConnections.joinWithLn { generateInitializeFederatedConnection(it) }}
            |}
        """
            .trimMargin()
      }

  fun generateFederatedSelfStructs() =
      federatedConnectionBundles.joinToString(
          prefix = "// Federated Connections\n", separator = "\n", postfix = "\n") { itOuter ->
            itOuter.groupedConnections.joinToString(
                prefix = "// Federated input and output connection self structs\n",
                separator = "\n",
                postfix = "\n") {
                  generateFederatedConnectionSelfStruct(it)
                } + generateFederatedConnectionBundleSelfStruct(itOuter)
          }

  fun generateFederatedCtors() =
      federatedConnectionBundles.joinToString(
          prefix = "// Federated Connections\n", separator = "\n", postfix = "\n") { itOuter ->
            itOuter.groupedConnections.joinToString(
                prefix = "// Federated input and output connection constructors\n",
                separator = "\n",
                postfix = "\n") {
                  generateFederatedConnectionCtor(it)
                } + generateFederatedConnectionBundleCtor(itOuter)
          }

  fun generateFederateStructFields() =
      federatedConnectionBundles.joinToString(
          prefix = "// Federated Connections\n", separator = "\n", postfix = "\n") {
            "LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(${it.src.codeType}, ${it.dest.codeType});"
          }

  fun generateReactorStructFields() =
      nonFederatedConnections.joinToString(
          prefix = "// Connections \n", separator = "\n", postfix = "\n") {
            if (it.isLogical)
                "LF_LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName()}, ${it.bankWidth}, ${it.portWidth});"
            else
                "LF_DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName()}, ${it.bankWidth}, ${it.portWidth});"
          }

  fun getMaxNumPendingEvents(): Int {
    var res = 0
    for (conn in nonFederatedConnections) {
      if (!conn.isLogical) {
        res += conn.maxNumPendingEvents
      }
    }
    for (bundle in federatedConnectionBundles) {
      for (conn in bundle.groupedConnections) {
        if (conn.destFed == currentFederate) {
          res += conn.maxNumPendingEvents
        }
      }
    }
    return res
  }

  fun generateNetworkChannelIncludes(): String =
      federatedConnectionBundles
          .distinctBy { it.networkChannel.type }
          .joinWithLn { it.networkChannel.src.iface.includeHeaders }

  // Finds the longest path through the federation. Performs
  // two breadt-first-searches looking for the longest path.
  // see: https://www.geeksforgeeks.org/longest-path-undirected-tree/
  fun getLongestFederatePath(): Int {
    data class Graph(val nodes: Int, val adj: List<Set<Int>>)

    // Return the furthest node and its distance from u
    fun breadthFirstSearch(u: Int, graph: Graph): Pair<Int, Int> {
      val visited = allFederates.map { false }.toMutableList()
      val distance = allFederates.map { -1 }.toMutableList()
      distance[u] = 0
      visited[u] = true
      val queue: Queue<Int> = LinkedList<Int>()
      queue.add(u)

      while (queue.isNotEmpty()) {
        val front = queue.poll()
        for (i in graph.adj[front]) {
          if (!visited[i]) {
            visited[i] = true
            distance[i] = distance[front] + 1
            queue.add(i)
          }
        }
      }
      var maxDist = -1
      var nodeIdx = -1
      for (i in 0..<graph.nodes) {
        if (distance[i] > maxDist) {
          maxDist = distance[i]
          nodeIdx = i
        }
      }
      return Pair(nodeIdx, maxDist)
    }
    // Build adjacency matrix
    val adjacency = allFederates.map { mutableSetOf<Int>() }
    for (bundle in allFederatedConnectionBundles) {
      val src = allFederates.indexOf(bundle.src)
      val dest = allFederates.indexOf(bundle.dest)
      adjacency[src].add(dest)
      adjacency[dest].add(src)
    }
    val graph = Graph(allFederates.size, adjacency)
    val firstEndPoint = breadthFirstSearch(0, graph)
    val actualLength = breadthFirstSearch(firstEndPoint.first, graph)
    return actualLength.second
  }
}
