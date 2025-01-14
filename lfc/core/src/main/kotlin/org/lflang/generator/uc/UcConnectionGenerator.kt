package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

class UcConnectionGenerator(
    private val reactor: Reactor,
    private val currentFederate: UcFederate?,
    private val allFederates: List<UcFederate>
) {

    private val nonFederatedConnections: List<UcGroupedConnection>
    private val federatedConnectionBundles: List<UcFederatedConnectionBundle>
    private val isFederated = currentFederate != null

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
                                it.src.federate == c.src.federate &&
                                it.dest.federate == c.dest.federate &&
                                it.getChannelType() == c.getChannelType()
                    }

                val srcFed = allFederates.find { it == UcFederate(c.src.varRef.container, c.src.bankIdx) }!!
                val destFed = allFederates.find { it == UcFederate(c.dest.varRef.container, c.dest.bankIdx) }!!
                val groupedConnection = UcFederatedGroupedConnection(
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

    private fun getUcPorts(portVarRefs: List<VarRef>, federates: List<UcFederate>): List<UcPort> {
        return portVarRefs.map { c ->
            if (c.container?.isAFederate ?: false) {
                val federates = allFederates.filter { it.inst == c.container }
                UcPort(c, federates)
            } else {
                UcPort(c, emptyList())
            }
        }

    }

    private fun parseConnectionChannels(conn: Connection, federates: List<UcFederate>): List<UcConnectionChannel> {
        val res = mutableListOf<UcConnectionChannel>()
        val rhsPorts = getUcPorts(conn.rightPorts, federates)
        var rhsPortIndex = 0

        var lhsPorts = getUcPorts(conn.leftPorts, federates)
        var lhsPortIndex = 0

        // Keep parsing out connections until we are out of right-hand-side (rhs) ports
        while (rhsPortIndex < rhsPorts.size) {
            // First get the current lhs and rhs port and UcGroupedConnection that we are working with
            val lhsPort = lhsPorts[lhsPortIndex]
            val rhsPort = rhsPorts[rhsPortIndex]
            if (rhsPort.channelsLeft() > lhsPort.channelsLeft()) {
                val rhsChannelsToAdd = rhsPort.takeChannels(lhsPort.channelsLeft())
                val lhsChannelsToAdd = lhsPort.takeRemainingChannels()
                lhsChannelsToAdd.zip(rhsChannelsToAdd)
                    .forEach { res.add(UcConnectionChannel(it.first, it.second, conn)) }
                lhsPortIndex += 1
            } else if (rhsPort.channelsLeft() < lhsPort.channelsLeft()) {
                val numRhsChannelsToAdd = rhsPort.channelsLeft()
                val rhsChannelsToAdd = rhsPort.takeRemainingChannels()
                val lhsChannelsToAdd = lhsPort.takeChannels(numRhsChannelsToAdd)
                lhsChannelsToAdd.zip(rhsChannelsToAdd)
                    .forEach { res.add(UcConnectionChannel(it.first, it.second, conn)) }
                rhsPortIndex += 1
            } else {
                lhsPort.takeRemainingChannels().zip(rhsPort.takeRemainingChannels())
                    .forEach { res.add(UcConnectionChannel(it.first, it.second, conn)) }
                rhsPortIndex += 1
                lhsPortIndex += 1
            }

            // If we are out of lhs variables, but not rhs, then there should be an iterated connection.
            // We handle it by resetting the lhsChannels variable and index and continuing until
            // we have been through all rhs channels.
            if (lhsPortIndex >= lhsPorts.size && rhsPortIndex < rhsPorts.size) {
                assert(conn.isIterated)
                lhsPorts = getUcPorts(conn.leftPorts, federates)
                lhsPortIndex = 0
            }
        }
        return res
    }

    companion object {
        private val Connection.delayString
            get(): String = this.delay.orNever().toCCode()

        private var federateInterfacesInitialized = false
        private var allFederatedConnectionBundles: List<UcFederatedConnectionBundle> = emptyList()

        private fun createFederatedConnectionBundles(groupedConnections: List<UcGroupedConnection>) {
            val groupedSet = HashSet(groupedConnections)
            val bundles = mutableListOf<UcFederatedConnectionBundle>()

            while (groupedSet.isNotEmpty()) {
                val g = groupedSet.first()!!
                val toRemove = mutableListOf(g)
                when (g) {
                    is UcFederatedGroupedConnection -> {
                        val group = groupedSet.filterIsInstance<UcFederatedGroupedConnection>().filter {
                            (it.srcFed == g.srcFed && it.destFed == g.destFed) ||
                                    (it.srcFed == g.destFed && it.destFed == g.srcFed)
                        }

                        bundles.add(
                            UcFederatedConnectionBundle(
                                g.srcFed, g.destFed, group
                            )
                        )

                        toRemove.addAll(group)
                    }
                }
                groupedSet.removeAll(toRemove.toSet())
            }
            allFederatedConnectionBundles = bundles
        }
    }

    init {
        if (isFederated && !federateInterfacesInitialized) {
            for (fed in allFederates) {
                UcNetworkInterfaceFactory.createInterfaces(fed).forEach { fed.addInterface(it) }
            }
            federateInterfacesInitialized = true
        }

        // Only parse out federated connection bundles once for the very first federate
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
                allFederatedConnectionBundles.filter { it.src == currentFederate || it.dest == currentFederate }
            )
            // Add all non-federated connections (e.g. a loopback connection)
            nonFederatedConnections.addAll(
                grouped
                    .filterNot { it is UcFederatedGroupedConnection }
                    .filter { it.channels.fold(true) { acc, c -> acc && (c.src.federate == currentFederate) } }
            )
        } else {
            nonFederatedConnections.addAll(grouped)
        }

        // Assign a unique ID to each connection to avoid possible naming conflicts.
        val allGroupedConnections =
            federatedConnectionBundles.map { it.groupedConnections }.flatten().plus(nonFederatedConnections)
        allGroupedConnections.forEachIndexed { idx, el ->
            el.assignUid(idx)
        }
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
                if (groupedConn.srcFed == currentFederate && groupedConn.srcInst == instantiation && groupedConn.srcPort == port) {
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

    private fun generateDelayedCtor(conn: UcGroupedConnection) =
        "LF_DEFINE_DELAYED_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay}, ${conn.isPhysical});"

    private fun generateFederatedInputSelfStruct(conn: UcGroupedConnection) =
        "LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents});"

    private fun generateFederatedInputCtor(conn: UcGroupedConnection) =
        "LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay}, ${conn.isPhysical});"

    private fun generateFederatedOutputSelfStruct(conn: UcGroupedConnection) =
        "LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()});"

    private fun generateFederatedOutputCtor(conn: UcGroupedConnection) =
        "LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.srcPort.type.toText()});"

    private fun generateFederatedConnectionSelfStruct(conn: UcFederatedGroupedConnection) =
        if (conn.srcFed == currentFederate) generateFederatedOutputSelfStruct(conn) else generateFederatedInputSelfStruct(
            conn
        )

    private fun generateFederatedConnectionCtor(conn: UcFederatedGroupedConnection) =
        if (conn.srcFed == currentFederate) generateFederatedOutputCtor(conn) else generateFederatedInputCtor(conn)

    private fun generateFederatedOutputInstance(conn: UcGroupedConnection) =
        "LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(${reactor.codeType}, ${conn.getUniqueName()});"

    private fun generateFederatedInputInstance(conn: UcGroupedConnection) =
        "LF_FEDERATED_INPUT_CONNECTION_INSTANCE(${reactor.codeType}, ${conn.getUniqueName()});"

    private fun generateFederatedConnectionInstance(conn: UcFederatedGroupedConnection) =
        if (conn.srcFed == currentFederate) generateFederatedOutputInstance(conn) else generateFederatedInputInstance(
            conn
        )

    private fun generateInitializeFederatedOutput(conn: UcFederatedGroupedConnection) =
        "LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.serializeFunc});"

    private fun generateInitializeFederatedInput(conn: UcFederatedGroupedConnection) =
        "LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(${reactor.codeType}, ${conn.getUniqueName()}, ${conn.deserializeFunc});"

    private fun generateInitializeFederatedConnection(conn: UcFederatedGroupedConnection) =
        if (conn.srcFed == currentFederate) generateInitializeFederatedOutput(conn) else generateInitializeFederatedInput(
            conn
        )


    private fun generateReactorCtorCode(conn: UcGroupedConnection) = """
        |${if (conn.isLogical) "LF_INITIALIZE_LOGICAL_CONNECTION(" else "LF_INITIALIZE_DELAYED_CONNECTION("}${reactor.codeType}, ${conn.getUniqueName()}, ${conn.bankWidth}, ${conn.portWidth});
        """.trimMargin()

    private fun generateFederateCtorCode(conn: UcFederatedConnectionBundle) =
        "LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(${conn.src.codeType}, ${conn.dest.codeType});"

    private fun generateConnectChannel(groupedConn: UcGroupedConnection, channel: UcConnectionChannel) =
        """|lf_connect((Connection *) &self->${groupedConn.getUniqueName()}[${channel.src.getCodeBankIdx()}][${channel.src.getCodePortIdx()}], (Port *) ${channel.src.generateChannelPointer()}, (Port *) ${channel.dest.generateChannelPointer()});
    """.trimMargin()

    private fun generateConnectionStatements(conn: UcGroupedConnection) = conn.channels
        .joinToString(separator = "\n") { generateConnectChannel(conn, it) }

    private fun generateConnectFederateOutputChannel(
        bundle: UcFederatedConnectionBundle,
        conn: UcFederatedGroupedConnection
    ) =
        conn.channels.joinWithLn {
            "lf_connect_federated_output((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeType}, ${bundle.dest.codeType}).${conn.getUniqueName()}, (Port*) &self->${bundle.src.inst.name}[0].${it.src.varRef.name}[${it.src.portIdx}]);"
        }

    private fun generateConnectFederateInputChannel(
        bundle: UcFederatedConnectionBundle,
        conn: UcGroupedConnection
    ) =
        conn.channels.joinWithLn {
            "lf_connect_federated_input((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeType}, ${bundle.dest.codeType}).${conn.getUniqueName()}, (Port*) &self->${bundle.dest.inst.name}[0].${it.dest.varRef.name}[${it.dest.portIdx}]);"
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
            prefix = "// Initialize connection bundles\n",
            separator = "\n",
            postfix = "\n"
        ) { generateFederateCtorCode(it) } +
                federatedConnectionBundles.joinToString(
                    prefix = "// Do connections \n",
                    separator = "\n",
                    postfix = "\n"
                ) { generateFederateConnectionStatements(it) }

    fun generateReactorCtorCodes() =
        nonFederatedConnections.joinToString(
            prefix = "// Initialize connections\n",
            separator = "\n",
            postfix = "\n"
        ) { generateReactorCtorCode(it) } +
                nonFederatedConnections.joinToString(
                    prefix = "// Do connections \n",
                    separator = "\n",
                    postfix = "\n"
                ) { generateConnectionStatements(it) }

    fun generateCtors() = nonFederatedConnections.joinToString(
        prefix = "// Connection constructors\n",
        separator = "\n",
        postfix = "\n"
    ) {
        if (it.isDelayed) generateDelayedCtor(it)
        else generateLogicalCtor(it)
    }

    fun generateSelfStructs() =
        nonFederatedConnections.joinToString(prefix = "// Connection structs\n", separator = "\n", postfix = "\n") {
            if (it.isLogical) generateLogicalSelfStruct(it)
            else generateDelayedSelfStruct(it)
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
        """.trimMargin()
        }

    private fun generateFederatedConnectionBundleCtor(bundle: UcFederatedConnectionBundle) = with(PrependOperator) {
        """ |LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(${bundle.src.codeType}, ${bundle.dest.codeType}) {
            |  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
            |  ${bundle.generateNetworkChannelCtor(currentFederate!!)}
            |  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
       ${"  |  "..bundle.groupedConnections.joinWithLn { generateInitializeFederatedConnection(it) }}
            |}
        """.trimMargin()
    }

    fun generateFederatedSelfStructs() = federatedConnectionBundles.joinToString(
        prefix = "// Federated Connections\n",
        separator = "\n",
        postfix = "\n"
    ) { itOuter ->
        itOuter.groupedConnections.joinToString(
            prefix = "// Federated input and output connection self structs\n",
            separator = "\n",
            postfix = "\n"
        ) { generateFederatedConnectionSelfStruct(it) } +
                generateFederatedConnectionBundleSelfStruct(itOuter)
    }

    fun generateFederatedCtors() = federatedConnectionBundles.joinToString(
        prefix = "// Federated Connections\n",
        separator = "\n",
        postfix = "\n"
    ) { itOuter ->
        itOuter.groupedConnections.joinToString(
            prefix = "// Federated input and output connection constructors\n",
            separator = "\n",
            postfix = "\n"
        ) { generateFederatedConnectionCtor(it) } +
                generateFederatedConnectionBundleCtor(itOuter)
    }


    fun generateFederateStructFields() = federatedConnectionBundles.joinToString(
        prefix = "// Federated Connections\n",
        separator = "\n",
        postfix = "\n"
    ) {
        "LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(${it.src.codeType}, ${it.dest.codeType});"
    }

    fun generateReactorStructFields() =
        nonFederatedConnections.joinToString(prefix = "// Connections \n", separator = "\n", postfix = "\n") {
            if (it.isLogical) "LF_LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName()}, ${it.bankWidth}, ${it.portWidth});"
            else "LF_DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName()}, ${it.bankWidth}, ${it.portWidth});"
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
        federatedConnectionBundles.distinctBy { it.networkChannel.type }
            .joinWithLn { it.networkChannel.src.iface.includeHeaders }
}
