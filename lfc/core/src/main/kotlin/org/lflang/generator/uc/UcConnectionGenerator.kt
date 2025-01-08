package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcConnectionGenerator.Companion.networkChannelType
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeTypeFederate
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*

class UcFederate(val inst: Instantiation, val bankIdx: Int) {
    val isBank = inst.isBank
    private val interfaces = mutableListOf<UcNetworkInterface>()

    val codeType = if (isBank) "${inst.codeTypeFederate}_${bankIdx}" else inst.codeTypeFederate

    fun addInterface(iface: UcNetworkInterface) {
        interfaces.add(iface)
    }

    fun getInterface(ifaceType: NetworkChannelType): UcNetworkInterface =
        interfaces.find{ it.type == ifaceType }!!

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is UcFederate) return false

        val sameInst = inst == other.inst
        val sameBank = bankIdx == other.bankIdx
        return if (isBank) sameInst && sameBank else sameInst
    }
}

class UcConnectionChannel(val src: UcChannel, val dest: UcChannel, val conn: Connection) {
    val isFederated = (src.varRef.container != null) && (src.varRef.container.isAFederate) && (dest.varRef.container != null) && dest.varRef.container.isAFederate && (src.varRef.container != dest.varRef.container)
    companion object {
        fun parseConnectionChannels(conn: Connection): List<UcConnectionChannel> {
            val res = mutableListOf<UcConnectionChannel>()
            val rhsPorts = conn.rightPorts.map { UcPort(it) }
            var rhsPortIndex = 0
            var lhsPorts = conn.leftPorts.map { UcPort(it) }
            var lhsPortIndex = 0

            // Keep parsing out connections until we are out of right-hand-side (rhs) ports
            while (rhsPortIndex < rhsPorts.size) {
                // First get the current lhs and rhs port and UcGroupedConnection that we are working with
                val lhsPort = lhsPorts.get(lhsPortIndex)
                val rhsPort = rhsPorts.get(rhsPortIndex)
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
                // we have been thorugh all rhs channels.
                if (lhsPortIndex >= lhsPorts.size && rhsPortIndex < rhsPorts.size) {
                    assert(conn.isIterated)
                    lhsPorts = conn.leftPorts.map { UcPort(it) }
                    lhsPortIndex = 0
                }
            }
            return res
        }
    }
}

open class UcGroupedConnection(
    val src: VarRef,
    val channels: List<UcConnectionChannel>,
    val lfConn: Connection,
    uid: Int
) {
    val delay = lfConn.delay.orNever().toCCode()
    val isPhysical = lfConn.isPhysical
    val isLogical = !lfConn.isPhysical && lfConn.delay == null
    val srcInst = src.container
    val srcPort = src.variable as Port
    val isDelayed = lfConn.isPhysical || !isLogical // We define physical connections as delayed.

    val bankWidth = srcInst?.width ?: 1
    val portWidth = srcPort.width
    val uniqueName = "conn_${srcInst?.name ?: ""}_${srcPort.name}_${uid}"
    val numDownstreams = {
        val frequencyMap = channels.groupingBy { Pair(it.src.port_idx, it.src.bank_idx) }.eachCount()
        frequencyMap.values.maxOrNull() ?: 0
    }
    val maxNumPendingEvents = 16 // FIXME: Must be derived from the program
}

class UcFederatedGroupedConnection(
    src: VarRef,
    channels: List<UcConnectionChannel>,
    lfConn: Connection,
    uid: Int,
    val srcFed: UcFederate,
    val destFed: UcFederate,
) : UcGroupedConnection(src, channels, lfConn, uid) {

    val serializeFunc = "serialize_payload_default"
    val deserializeFunc = "deserialize_payload_default"
}

class UcFederatedConnectionBundle(
    val src: UcFederate,
    val dest: UcFederate,
    val groupedConnections: List<UcFederatedGroupedConnection>
) {
    val networkChannel: UcNetworkChannel

    init {
        val conn = groupedConnections.first().lfConn
        val srcIf = src.getInterface(conn.networkChannelType)
        val destIf = dest.getInterface(conn.networkChannelType)

        networkChannel =
            when (conn.networkChannelType) {
                NetworkChannelType.TcpIp -> {
                    val srcEp = (srcIf as UcTcpIpInterface).createEndpoint(null)
                    val destEp = (destIf as UcTcpIpInterface).createEndpoint(null)
                    UcTcpIpChannel(srcEp, destEp)
                }
            }
    }

    fun numOutputs(federate: UcFederate) = groupedConnections.count { it.srcFed == federate }

    fun numInputs(federate: UcFederate) = groupedConnections.count { it.destFed == federate }

    fun generateNetworkChannelCtor(federate: UcFederate): String =
        if (federate == src) {
            networkChannel.generateChannelCtorSrc()
        } else {
            networkChannel.generateChannelCtorDest()
        }
}

// Convenience class around a port variable reference. It is used to encapsulate the management of multi-connections
// where a single lhs port has to
class UcChannel(val varRef: VarRef, val port_idx: Int, val bank_idx: Int) {
    private val portOfContainedReactor = varRef.container != null
    private val index = "${if (portOfContainedReactor) "[${bank_idx}]" else ""}[${port_idx}]"
    private val reactorInstance = if (portOfContainedReactor) "${varRef.container.name}[${bank_idx}]." else ""
    private val portInstance = "${varRef.name}[${port_idx}]"

    fun generateChannelPointer() = "&self->${reactorInstance}${portInstance}"
}

// Wrapper around a variable reference to a Port. Contains a channel for each bank/multiport within it.
// For each connection statement where a port is referenced, we create an UcPort and use this class
// to figure out how the individual channels are connect to other UcPorts.
class UcPort(val varRef: VarRef) {
    val bankWidth = varRef.container?.width ?: 1
    val portWidth = (varRef.variable as Port).width
    private val isInterleaved = varRef.isInterleaved
    private val channels = ArrayDeque<UcChannel>()

    // Construct the stack of channels belonging to this port. If this port is interleaved,
    // then we create channels first for ports then for banks.
    init {
        if (isInterleaved) {
            for (i in 0..portWidth - 1) {
                for (j in 0..bankWidth - 1) {
                    channels.add(UcChannel(varRef, i, j))
                }
            }
        } else {
            for (i in 0..bankWidth - 1) {
                for (j in 0..portWidth - 1) {
                    channels.add(UcChannel(varRef, j, i))
                }
            }
        }
    }

    fun takeRemainingChannels(): List<UcChannel> = takeChannels(channels.size)

    // Get a number of channels from this port. This has sideeffects and will remove these
    // channels from the port.
    fun takeChannels(numChannels: Int): List<UcChannel> {
        assert(numChannels >= channels.size)
        val res = mutableListOf<UcChannel>()
        for (i in 1..numChannels) {
            res.add(channels.removeFirst())
        }
        return res
    }

    fun channelsLeft(): Int = channels.size
}

class UcConnectionGenerator(private val reactor: Reactor, private val federate: UcFederate?) {

    private val nonFederatedConnections: List<UcGroupedConnection>
    private val federatedConnectionBundles: List<UcFederatedConnectionBundle>
    private val isFederated = federate != null

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
                                it.dest.varRef == c.dest.varRef &&
                                it.src.varRef == c.src.varRef &&
                                it.src.bank_idx == c.src.bank_idx &&
                                it.dest.bank_idx == c.dest.bank_idx &&
                                it.conn.networkChannelType == c.conn.networkChannelType
                    }

                val srcFed = allFederates.find {it == UcFederate(c.src.varRef.container, c.src.bank_idx)}!!
                val destFed = allFederates.find {it == UcFederate(c.dest.varRef.container, c.dest.bank_idx)}!!
                val groupedConnection = UcFederatedGroupedConnection(
                        c.src.varRef,
                        grouped,
                        c.conn,
                        res.size,
                        srcFed,
                        destFed,
                    )

                res.add(groupedConnection)
                channels.removeAll(grouped)

            } else {
                val grouped =
                    channels.filter {
                        it.conn.delayString == c.conn.delayString &&
                                it.conn.isPhysical == c.conn.isPhysical &&
                                it.src.varRef == c.src.varRef
                    }

                val groupedConnection = UcGroupedConnection(c.src.varRef, grouped, c.conn, res.size)
                res.add(groupedConnection)
                channels.removeAll(grouped)
            }
        }
        return res
    }
    companion object {
        private val Connection.delayString
            get(): String = this.delay.orNever().toCCode()

        val Connection.networkChannelType
            get(): NetworkChannelType = NetworkChannelType.TcpIp

        private var allFederatedConnectionBundles: List<UcFederatedConnectionBundle> = emptyList()
        private var allFederates: Set<UcFederate> = emptySet()


        private fun createAllFederates(top: Reactor) {
            val feds = mutableSetOf<UcFederate>()
            for (inst in top.allInstantiations) {
                for (bankIdx in 0..inst.width) {
                    val fed = UcFederate(inst, bankIdx)
                    fed.addInterface(UcTcpIpInterface())
                    feds.add(fed)
                }
            }
            allFederates = feds
        }

        private fun createFederatedConnectionBundles(groupedConnections: List<UcGroupedConnection>) {
            val groupedSet = HashSet(groupedConnections)
            val bundles = mutableListOf<UcFederatedConnectionBundle>()

            while (groupedSet.isNotEmpty()) {
                val g = groupedSet.first()!!
                val toRemove = mutableListOf(g)
                when (g) {
                    is UcFederatedGroupedConnection -> {
                        val group = groupedSet.filterIsInstance<UcFederatedGroupedConnection>().filter {
                            it.srcFed == g.srcFed &&
                                    it.destFed == g.destFed
                        }

                        bundles.add(
                            UcFederatedConnectionBundle(
                                g.srcFed, g.destFed, group
                            )
                        )

                        toRemove.addAll(group)
                    }
                }
                groupedSet.removeAll(toRemove)
            }
            allFederatedConnectionBundles = bundles
        }
    }


    init {
        if (isFederated && allFederates.isEmpty()) {
            createAllFederates(reactor)
        }

        // Only parse out federated connection bundles once for the very first federate
        val channels = mutableListOf<UcConnectionChannel>()
        reactor.allConnections.forEach { channels.addAll(UcConnectionChannel.parseConnectionChannels(it)) }
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
                allFederatedConnectionBundles.filter { it.src == federate || it.dest == federate }
            )
            // Add all non-federated conncetions (e.g. a loopback connection)
            // FIXME: How can we handle banks here?
            nonFederatedConnections.addAll(
                grouped
                    .filterNot{ it is UcFederatedGroupedConnection}
                    .filter{it.srcInst == federate!!.inst}
            )
        } else {
            nonFederatedConnections.addAll(grouped)
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
            if (federatedConnectionBundle.src == federate) {
                for (groupedConn in federatedConnectionBundle.groupedConnections) {
                    if (groupedConn.srcInst == instantiation && groupedConn.srcPort == port) {
                        count += 1
                    }
                }
            }
        }
        return count
    }

    private fun generateLogicalSelfStruct(conn: UcGroupedConnection) =
        "LF_DEFINE_LOGICAL_CONNECTION_STRUCT(${reactor.codeType},  ${conn.uniqueName}, ${conn.numDownstreams()});"

    private fun generateLogicalCtor(conn: UcGroupedConnection) =
        "LF_DEFINE_LOGICAL_CONNECTION_CTOR(${reactor.codeType},  ${conn.uniqueName}, ${conn.numDownstreams()});"

    private fun generateDelayedSelfStruct(conn: UcGroupedConnection) =
        "LF_DEFINE_DELAYED_CONNECTION_STRUCT(${reactor.codeType}, ${conn.uniqueName}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay});"

    private fun generateDelayedCtor(conn: UcGroupedConnection) =
        "LF_DEFINE_DELAYED_CONNECTION_CTOR(${reactor.codeType}, ${conn.uniqueName}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay}, ${conn.isPhysical});"

    private fun generateFederatedInputSelfStruct(conn: UcGroupedConnection) =
        "LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(${reactor.codeType}, ${conn.uniqueName}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents});"

    private fun generateFederatedInputCtor(conn: UcGroupedConnection) =
        "LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(${reactor.codeType}, ${conn.uniqueName}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay}, ${conn.isPhysical});"

    private fun generateFederatedOutputSelfStruct(conn: UcGroupedConnection) =
        "LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT(${reactor.codeType}, ${conn.uniqueName}, ${conn.srcPort.type.toText()});"

    private fun generateFederatedOutputCtor(conn: UcGroupedConnection) =
        "LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_CTOR(${reactor.codeType}, ${conn.uniqueName}, ${conn.srcPort.type.toText()});"

    private fun generateFederatedConnectionSelfStruct(conn: UcFederatedGroupedConnection) =
        if (conn.srcFed == federate) generateFederatedOutputSelfStruct(conn) else generateFederatedInputSelfStruct(conn)

    private fun generateFederatedConnectionCtor(conn: UcFederatedGroupedConnection) =
        if (conn.srcFed == federate) generateFederatedOutputCtor(conn) else generateFederatedInputCtor(conn)

    private fun generateFederatedOutputInstance(conn: UcGroupedConnection) =
        "LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(${reactor.codeType}, ${conn.uniqueName});"

    private fun generateFederatedInputInstance(conn: UcGroupedConnection) =
        "LF_FEDERATED_INPUT_CONNECTION_INSTANCE(${reactor.codeType}, ${conn.uniqueName});"

    private fun generateFederatedConnectionInstance(conn: UcFederatedGroupedConnection) =
        if (conn.srcFed == federate) generateFederatedOutputInstance(conn) else generateFederatedInputInstance(conn)

    private fun generateInitializeFederatedOutput(conn: UcFederatedGroupedConnection) =
        "LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(${reactor.codeType}, ${conn.uniqueName}, ${conn.serializeFunc});"

    private fun generateInitializeFederatedInput(conn: UcFederatedGroupedConnection) =
        "LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(${reactor.codeType}, ${conn.uniqueName}, ${conn.deserializeFunc});"

    private fun generateInitializeFederatedConnection(conn: UcFederatedGroupedConnection) =
        if (conn.srcFed == federate) generateInitializeFederatedOutput(conn) else generateInitializeFederatedInput(conn)


    private fun generateReactorCtorCode(conn: UcGroupedConnection) = with(PrependOperator) {
        """
            |${if (conn.isLogical) "LF_INITIALIZE_LOGICAL_CONNECTION(" else "LF_INITIALIZE_DELAYED_CONNECTION("}${reactor.codeType}, ${conn.uniqueName}, ${conn.bankWidth}, ${conn.portWidth});
            """.trimMargin()
    };

    private fun generateFederateCtorCode(conn: UcFederatedConnectionBundle) =
        "LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(${conn.src.codeType}, ${conn.dest.codeType});"

    private fun generateConnectChannel(groupedConn: UcGroupedConnection, channel: UcConnectionChannel) =
        with(PrependOperator) {
            """|lf_connect((Connection *) &self->${groupedConn.uniqueName}[${channel.src.bank_idx}][${channel.src.port_idx}], (Port *) ${channel.src.generateChannelPointer()}, (Port *) ${channel.dest.generateChannelPointer()});
        """.trimMargin()
        }

    private fun generateConnectionStatements(conn: UcGroupedConnection) = with(PrependOperator) {
        conn.channels
            .joinToString(separator = "\n") { generateConnectChannel(conn, it) }
    }

    private fun generateConnectFederateOutputChannel(
        bundle: UcFederatedConnectionBundle,
        conn: UcFederatedGroupedConnection
    ) =
        conn.channels.joinWithLn {
            "lf_connect_federated_output((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeType}, ${bundle.dest.codeType}).${conn.uniqueName}, (Port*) &self->${bundle.src.inst.name}[0].${it.src.varRef.name}[0]);"
        }

    private fun generateConnectFederateInputChannel(bundle: UcFederatedConnectionBundle, conn: UcGroupedConnection) =
        conn.channels.joinWithLn {
            "lf_connect_federated_input((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeType}, ${bundle.dest.codeType}).${conn.uniqueName}, (Port*) &self->${bundle.dest.inst.name}[0].${it.dest.varRef.name}[0]);"
        }


    private fun generateFederateConnectionStatements(conn: UcFederatedConnectionBundle) =
        conn.groupedConnections.joinWithLn {
            if (conn.src == federate) {
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

    fun generateFederatedConnectionBundleSelfStruct(bundle: UcFederatedConnectionBundle) = with(PrependOperator) {
        """ |typedef struct {
            |  FederatedConnectionBundle super;
       ${"  |  "..bundle.networkChannel.codeType} channel;
       ${"  |  "..bundle.groupedConnections.joinWithLn { generateFederatedConnectionInstance(it) }}
            |  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(${bundle.numInputs(federate!!)}, ${
            bundle.numOutputs(
                federate !!
            )
        });
            |} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(${bundle.src.codeType}, ${bundle.dest.codeType});
            |
        """.trimMargin()
    }

    fun generateFederatedConnectionBundleCtor(bundle: UcFederatedConnectionBundle) = with(PrependOperator) {
        """ |LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(${bundle.src.codeType}, ${bundle.dest.codeType}) {
            |  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
            |  ${bundle.generateNetworkChannelCtor(federate!!)}
            |  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
       ${"  |  "..bundle.groupedConnections.joinWithLn { generateInitializeFederatedConnection(it) }}
            |}
        """.trimMargin()
    }

    fun generateFederatedSelfStructs() = federatedConnectionBundles.joinToString(
        prefix = "// Federated Connections\n",
        separator = "\n",
        postfix = "\n"
    ) {
        it.groupedConnections.joinToString(
            prefix = "// Federated input and output connection self structs\n",
            separator = "\n",
            postfix = "\n"
        ) { generateFederatedConnectionSelfStruct(it) } +
                generateFederatedConnectionBundleSelfStruct(it)
    }

    fun generateFederatedCtors() = federatedConnectionBundles.joinToString(
        prefix = "// Federated Connections\n",
        separator = "\n",
        postfix = "\n"
    ) {
        it.groupedConnections.joinToString(
            prefix = "// Federated input and output connection constructors\n",
            separator = "\n",
            postfix = "\n"
        ) { generateFederatedConnectionCtor(it) } +
                generateFederatedConnectionBundleCtor(it)
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
            if (it.isLogical) "LF_LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName}, ${it.bankWidth}, ${it.portWidth});"
            else "LF_DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName}, ${it.bankWidth}, ${it.portWidth});"
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
                if (conn.destFed == federate) {
                    res += conn.maxNumPendingEvents
                }
            }
        }
        return res
    }
}
