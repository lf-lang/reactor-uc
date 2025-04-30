package org.lflang.generator.uc

import java.util.*
import kotlin.collections.HashSet
import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAnEnclave
import org.lflang.generator.uc.UcPortGenerator.Companion.arrayLength
import org.lflang.generator.uc.UcPortGenerator.Companion.isArray
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

/**
 * This generator creates code for configuring the connections between reactors. This is perhaps the
 * most complicated part of the code-generator. This generator handles both federated and
 * non-federated programs
 *
 * @param reactor The reactor declaration that we are generating connections for.
 * @param currentFederate The current federate. This is only used when generating for a federate. In
 *   which case the reactor is set to the top-level `federated reactor` and currentFederate is the
 *   federate instance.
 * @param allNodes A list of all the federates/enclaves
 */
class UcConnectionGenerator(
    private val reactor: Reactor,
    private val currentFederate: UcFederate?,
    private val allNodes: List<UcSchedulingNode>
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
   * Given a LF connection and possibly the list of federates/enclaves of the program. Create all
   * the ConnectionChannels found within the LF Connection. This must handle multiports, banks,
   * iterated connections, federated connections and enclaved connections.
   */
  private fun parseConnectionChannels(
      conn: Connection,
      nodes: List<UcSchedulingNode>
  ): List<UcConnectionChannel> {
    val res = mutableListOf<UcConnectionChannel>()
    val rhsPorts = conn.rightPorts.map { getChannelQueue(it, nodes) }
    var rhsPortIndex = 0

    var lhsPorts = conn.leftPorts.map { getChannelQueue(it, nodes) }
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
        lhsPorts = conn.leftPorts.map { getChannelQueue(it, nodes) }
        lhsPortIndex = 0
      }
    }
    return res
  }

  /**
   * Given a list of ConnectionChannels, group them together. How they are grouped depends on
   * whether we are dealing with federated, enclaved or normal reactors. For normal reactors, all
   * connections from the same port are grouped together into a single Connection object. For
   * federated or enclaved, only those that are from the same port and destined to the same remote
   * enclave/federate.
   *
   * @param channels The list of all the connection channels within this reactor declaration
   * @return A list of those connection channels grouped into Grouped Connections.
   */
  private fun groupConnections(channels: List<UcConnectionChannel>): List<UcGroupedConnection> {
    val res = mutableListOf<UcGroupedConnection>()
    val channels = HashSet(channels)

    while (channels.isNotEmpty()) {

      // Select a channel
      val nextChannel = channels.first()!!

      if (nextChannel.isFederated) {
        // Find all other channels that can be grouped with this one.
        val grouped =
            channels.filter {
              it.conn.delayString == nextChannel.conn.delayString &&
                  it.conn.isPhysical == nextChannel.conn.isPhysical &&
                  it.src.varRef == nextChannel.src.varRef &&
                  it.src.node == nextChannel.src.node &&
                  it.dest.node == nextChannel.dest.node &&
                  it.getChannelType() == nextChannel.getChannelType()
            }
        // Find the associated UcFederate
        val srcFed =
            allNodes.find {
              it == UcFederate(nextChannel.src.varRef.container, nextChannel.src.bankIdx)
            }!!
                as UcFederate
        val destFed =
            allNodes.find {
              it == UcFederate(nextChannel.dest.varRef.container, nextChannel.dest.bankIdx)
            }!!
                as UcFederate

        // Create the grouped connectino
        val groupedConnection =
            UcFederatedGroupedConnection(
                nextChannel.src.varRef,
                grouped,
                nextChannel.conn,
                srcFed,
                destFed,
            )

        res.add(groupedConnection)
        channels.removeAll(grouped.toSet())
      } else if (nextChannel.conn.isEnclaved) {
        val grouped =
            channels.filter {
              it.conn.delayString == nextChannel.conn.delayString &&
                  it.conn.isPhysical == nextChannel.conn.isPhysical &&
                  !it.isFederated &&
                  it.src.varRef == nextChannel.src.varRef &&
                  it.src.node == nextChannel.src.node &&
                  it.dest.node == nextChannel.dest.node &&
                  it.dest.varRef == nextChannel.dest.varRef
            }

        val groupedConnection =
            UcGroupedConnection(nextChannel.src.varRef, grouped, nextChannel.conn)
        res.add(groupedConnection)
        channels.removeAll(grouped.toSet())
      } else {
        val grouped =
            channels.filter {
              it.conn.delayString == nextChannel.conn.delayString &&
                  it.conn.isPhysical == nextChannel.conn.isPhysical &&
                  !it.isFederated &&
                  it.src.varRef == nextChannel.src.varRef &&
                  it.src.node == nextChannel.src.node
            }

        val groupedConnection =
            UcGroupedConnection(nextChannel.src.varRef, grouped, nextChannel.conn)
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
  private fun getChannelQueue(portVarRef: VarRef, nodes: List<UcSchedulingNode>): UcChannelQueue {
    return if (portVarRef.container?.isAFederate ?: false) {
      UcChannelQueue(portVarRef, nodes.filter { it.inst == portVarRef.container })
    } else if (portVarRef.container?.isAnEnclave ?: false) {
      UcChannelQueue(portVarRef, nodes.filter { it.inst == portVarRef.container })
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
      for (node in allNodes) {
        val fed = node as UcFederate
        UcNetworkInterfaceFactory.createInterfaces(fed).forEach { fed.addInterface(it) }
      }
      federateInterfacesInitialized = true
    }

    // Parse out all GroupedConnections. Note that this is repeated for each federate.
    val channels = mutableListOf<UcConnectionChannel>()
    reactor.allConnections.forEach { channels.addAll(parseConnectionChannels(it, allNodes)) }
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
                it.channels.fold(true) { acc, c -> acc && (c.src.node == currentFederate) }
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
      if (conn.srcPort.type.isArray)
          "LF_DEFINE_DELAYED_CONNECTION_STRUCT_ARRAY(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.srcPort.type.id}, ${conn.maxNumPendingEvents}, ${conn.srcPort.type.arrayLength});"
      else
          "LF_DEFINE_DELAYED_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents});"

  private fun generateEnclavedSelfStruct(conn: UcGroupedConnection) =
      if (conn.srcPort.type.isArray)
          "LF_DEFINE_ENCLAVED_CONNECTION_STRUCT_ARRAY(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.srcPort.type.id}, ${conn.maxNumPendingEvents}, ${conn.srcPort.type.arrayLength});"
      else
          "LF_DEFINE_ENCLAVED_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents});"

  private fun generateFederatedInputSelfStruct(conn: UcFederatedGroupedConnection) =
      if (conn.srcPort.type.isArray)
          "LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT_ARRAY(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.id}, ${conn.maxNumPendingEvents}, ${conn.srcPort.type.arrayLength});"
      else
          "LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents});"

  private fun generateFederatedInputCtor(conn: UcFederatedGroupedConnection) =
      "LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay}, ${conn.isPhysical}, ${conn.getMaxWait().toCCode()});"

  private fun generateDelayedCtor(conn: UcGroupedConnection) =
      "LF_DEFINE_DELAYED_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.maxNumPendingEvents}, ${conn.isPhysical});"

  private fun generateEnclavedCtor(conn: UcGroupedConnection) =
      "LF_DEFINE_ENCLAVED_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.maxNumPendingEvents}, ${conn.isPhysical});"

  private fun generateFederatedOutputSelfStruct(conn: UcFederatedGroupedConnection) =
      if (conn.srcPort.type.isArray)
          "LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT_ARRAY(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.id}, ${conn.srcPort.type.arrayLength});"
      else
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
      if (conn.isEnclaved) {
        "LF_INITIALIZE_ENCLAVED_CONNECTION(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.delay}, ${conn.bankWidth}, ${conn.portWidth})"
      } else if (conn.isLogical) {
        "LF_INITIALIZE_LOGICAL_CONNECTION(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.bankWidth}, ${conn.portWidth})"
      } else {
        "LF_INITIALIZE_DELAYED_CONNECTION(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.delay}, ${conn.bankWidth}, ${conn.portWidth})"
      }

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
        "lf_connect_federated_output((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeType}, ${bundle.dest.codeType}).${conn.getUniqueName()}, (Port*) &self->${it.src.node!!.inst.name}[0].${it.src.varRef.name}[${it.src.portIdx}]);"
      }

  private fun generateConnectFederateInputChannel(
      bundle: UcFederatedConnectionBundle,
      conn: UcGroupedConnection
  ) =
      conn.channels.joinWithLn {
        "lf_connect_federated_input((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeType}, ${bundle.dest.codeType}).${conn.getUniqueName()}, (Port*) &self->${it.dest.node!!.inst.name}[0].${it.dest.varRef.name}[${it.dest.portIdx}]);"
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
            if (it.isEnclaved) generateEnclavedCtor(it)
            else if (it.isDelayed) generateDelayedCtor(it) else generateLogicalCtor(it)
          }

  fun generateSelfStructs() =
      nonFederatedConnections.joinToString(
          prefix = "// Connection structs\n", separator = "\n", postfix = "\n") {
            if (it.isEnclaved) generateEnclavedSelfStruct(it)
            else if (it.isLogical) generateLogicalSelfStruct(it) else generateDelayedSelfStruct(it)
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
            if (it.isEnclaved)
                "LF_ENCLAVED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName()}, ${it.bankWidth}, ${it.portWidth});"
            else if (it.isLogical)
                "LF_LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName()}, ${it.bankWidth}, ${it.portWidth});"
            else
                "LF_DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName()}, ${it.bankWidth}, ${it.portWidth});"
          }

  /** Returns the number of events needed by the connections within this reactor declaration. */
  fun getNumEvents(): Int {
    var res = 0
    for (conn in nonFederatedConnections) {
      if (!conn.isLogical && !conn.isEnclaved) {
        res += conn.maxNumPendingEvents
      }
    }
    return res
  }

  /**
   * Returns the number of events needed by the enclave/federate [node] which is inside this reactor
   * declaration. This will only include events needed for any input connections to [node]
   */
  fun getNumEvents(node: UcSchedulingNode): Int {
    var res = 0

    if (node.nodeType == NodeType.FEDERATE) {
      for (bundle in federatedConnectionBundles) {
        for (conn in bundle.groupedConnections) {
          if (conn.destFed == currentFederate) {
            res += conn.maxNumPendingEvents
          }
        }
      }
    } else if (node.nodeType == NodeType.ENCLAVE) {
      for (conn in nonFederatedConnections) {
        if (conn.isEnclaved && conn.channels.first().dest.node == node) {
          res += conn.maxNumPendingEvents * conn.channels.size
        }
      }
    }
    return res
  }

  /**
   * Generate include statements for the NetworkChannels that are used in this reactor declaration.
   */
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
      val visited = allNodes.map { false }.toMutableList()
      val distance = allNodes.map { -1 }.toMutableList()
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
    val adjacency = allNodes.map { mutableSetOf<Int>() }
    for (bundle in allFederatedConnectionBundles) {
      val src = allNodes.indexOf(bundle.src)
      val dest = allNodes.indexOf(bundle.dest)
      adjacency[src].add(dest)
      adjacency[dest].add(src)
    }
    val graph = Graph(allNodes.size, adjacency)
    val firstEndPoint = breadthFirstSearch(0, graph)
    val actualLength = breadthFirstSearch(firstEndPoint.first, graph)
    return actualLength.second
  }

  /**
   * If this reactor declaration is federated. Then we return whether all the federates inside it
   * are fully connected, which is a requirement for e.g. startup tag coordination to work.
   */
  fun areFederatesFullyConnected(): Boolean {
    data class Graph(val nodes: Int, val adj: List<Set<Int>>)

    // Return the furthest node and its distance from u
    fun breadthFirstSearch(u: Int, graph: Graph): Boolean {
      val visited = allNodes.map { false }.toMutableList()
      visited[u] = true
      val queue: Queue<Int> = LinkedList<Int>()
      queue.add(u)

      while (queue.isNotEmpty()) {
        val front = queue.poll()
        for (i in graph.adj[front]) {
          if (!visited[i]) {
            visited[i] = true
            queue.add(i)
          }
        }
      }
      return visited.all { it }
    }

    // Build adjacency matrix
    val adjacency = allNodes.map { mutableSetOf<Int>() }
    for (bundle in allFederatedConnectionBundles) {
      val src = allNodes.indexOf(bundle.src)
      val dest = allNodes.indexOf(bundle.dest)
      adjacency[src].add(dest)
      adjacency[dest].add(src)
    }
    val graph = Graph(allNodes.size, adjacency)
    return breadthFirstSearch(0, graph)
  }
}
