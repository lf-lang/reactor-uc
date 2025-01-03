package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcConnectionGenerator.Companion.isFederated
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeTypeFederate
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*


class UcFederate(val federate: Instantiation, bankIdx: Int) {
    val isBank = federate.isBank
}

class UcConnectionChannel(val src: UcChannel, val dest: UcChannel, val conn: Connection) {
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

class UcGroupedConnection(val src: VarRef, val channels: List<UcConnectionChannel>, val lfConn: Connection, val uid: Int) {
    val delay = lfConn.delay.orNever().toCCode()
    val isPhysical = lfConn.isPhysical
    val isLogical = lfConn.isLogical
    val srcInst = src.container
    val srcPort = src.variable as Port
    val isFederated = lfConn.isFederated && channels.first().dest.varRef.container != srcInst
    val isDelayed = lfConn.isPhysical || !lfConn.isLogical

    val serializeFunc = "serialize_payload_default"
    val deserializeFunc = "deserialize_payload_default"
    val bankWidth = srcInst?.width ?: 1
    val portWidth = srcPort.width
    val uniqueName = "conn_${srcInst?.name ?: ""}_${srcPort.name}_${uid}"
    val numDownstreams = {
        val frequencyMap = channels.groupingBy { Pair(it.src.port_idx, it.src.bank_idx)}.eachCount()
        frequencyMap.values.maxOrNull() ?:0
    }
    val maxNumPendingEvents = 16 // FIXME: Must be derived from the program


    companion object {
        private val Connection.delayString
            get(): String = this.delay.orNever().toCCode()
        private val Connection.isLogical
            get(): Boolean = !this.isPhysical && this.delay == null

        fun groupConnections(channels: List<UcConnectionChannel>): List<UcGroupedConnection> {
            val res = mutableListOf<UcGroupedConnection>()
            val channels = HashSet(channels)

            while (channels.isNotEmpty()) {
                val c = channels.first()!!
                val others = if (c.conn.isFederated) {
                    channels.filter {
                                it.conn.delayString == c.conn.delayString &&
                                it.conn.isPhysical == c.conn.isPhysical &&
                                it.dest.varRef == c.dest.varRef
                                it.src.varRef == c.src.varRef
                    }
                } else {
                    channels.filter {
                        it.conn.delayString == c.conn.delayString &&
                                it.conn.isPhysical == c.conn.isPhysical &&
                                it.src.varRef == c.src.varRef
                    }
                }

                val groupedChannels = others.plus(c)
                val groupedConnection = UcGroupedConnection(c.src.varRef, groupedChannels, c.conn, res.size)
                res.add(groupedConnection)
                channels.removeAll(groupedChannels)
            }
            return res
        }
    }
}

class UcFederatedConnectionBundle2(val src: UcFederate, val dest: UcFederate, val netChannel: UcNetworkChannel) {
}


class UcFederatedConnectionBundle(
    val src: Instantiation,
    val dst: Instantiation,
    val groupedConnections: List<UcGroupedConnection>
) {
    val channelCodeType = "TcpIpChannel"

    fun numInputs(federate: Instantiation) = groupedConnections.count { it.channels.first().dest.varRef.container == federate }

    fun numOutputs(federate: Instantiation) = groupedConnections.count { it.srcInst == federate }

    fun generateChannelCtor(federate: Instantiation) =
        "TcpIpChannel_ctor(&self->channel, parent->env, \"127.0.0.1\", 10000 + ${groupedConnections.first().uid}, AF_INET, ${(federate == src).toString()});"

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

//
//// A class for maintaining all the runtime UcConnections within a reactor.
//class UcConnections() {
//    val connections = mutableListOf<UcGroupedConnection2>();
//
//    // Parse a connection and update the list of UcGroupedConnection. This is non-trivial since a single variable
//    // in the connection can have several banks and multiports. In a multi-connection, we might connect some channels
//    // to one variable and others to another.
//    fun addConnection(conn: Connection) {
//        // First translate the variables into our UcPort which also has information of channels (banks x multiports)
//        val rhsPorts = conn.rightPorts.map { UcPort(it) }
//        var rhsPortIndex = 0
//        var lhsPorts = conn.leftPorts.map { UcPort(it) }
//        var lhsPortIndex = 0
//
//        // Keep parsing out connections until we are out of right-hand-side (rhs) ports
//        while (rhsPortIndex < rhsPorts.size) {
//            // First get the current lhs and rhs port and UcGroupedConnection that we are working with
//            val lhsPort = lhsPorts.get(lhsPortIndex)
//            val rhsPort = rhsPorts.get(rhsPortIndex)
//            val ucConnection = getOrCreateNewGroupedConnection(lhsPort.varRef, rhsPort.varRef, conn)
//
//            if (rhsPort.channelsLeft() > lhsPort.channelsLeft()) {
//                // If we have more channels left in the rhs variable, then we "complete" a downstreamSet fo
//                // the lhs, commit it, and move to the next lhs variable
//                val rhsChannelsToAdd = rhsPort.takeChannels(lhsPort.channelsLeft())
//                val lhsChannelsToAdd = lhsPort.takeRemainingChannels()
//                lhsChannelsToAdd.zip(rhsChannelsToAdd).forEach { ucConnection.addDest(it) }
//                ucConnection.commit()
//                lhsPortIndex += 1
//            } else if (rhsPort.channelsLeft() < lhsPort.channelsLeft()) {
//                // If we have more channels left in the lhs variable, we dont complete the downstreamSet yet,
//                // we move to the next rhsChannel. Only if this was the very last rhs variable do we commit.
//                val numRhsChannelsToAdd = rhsPort.channelsLeft()
//                val rhsChannelsToAdd = rhsPort.takeRemainingChannels()
//                val lhsChannelsToAdd = lhsPort.takeChannels(numRhsChannelsToAdd)
//                lhsChannelsToAdd.zip(rhsChannelsToAdd).forEach { ucConnection.addDest(it) }
//                rhsPortIndex += 1
//                if (rhsPortIndex >= rhsPorts.size) {
//                    ucConnection.commit()
//                }
//            } else {
//                // Channels are same size
//                lhsPort.takeRemainingChannels().zip(rhsPort.takeRemainingChannels())
//                    .forEach { ucConnection.addDest(it) }
//                ucConnection.commit()
//                rhsPortIndex += 1
//                lhsPortIndex += 1
//            }
//
//            // If we are out of lhs variables, but not rhs, then there should be an iterated connection.
//            // We handle it by resetting the lhsChannels variable and index and continuing until
//            // we have been thorugh all rhs channels.
//            if (lhsPortIndex >= lhsPorts.size && rhsPortIndex < rhsPorts.size) {
//                assert(conn.isIterated)
//                lhsPorts = conn.leftPorts.map { UcPort(it) }
//                lhsPortIndex = 0
//            }
//        }
//    }
//
//    /** Finds an existing GroupedConnection from srcVarRef with matchin connection properties (physical and delay). */
//    fun findExistingGroupedConnection(srcVarRef: VarRef, dstVarRef: VarRef, conn: Connection): UcGroupedConnection2? {
//        if (conn.isFederated) {
//            return connections.find { c ->
//                c.srcVarRef == srcVarRef && c.isPhysical == conn.isPhysical && c.delay == conn.delay.orNever().toCCode()
//            }
//        } else {
//            return connections.find { c ->
//                c.srcVarRef == srcVarRef && c.dstInst == dstVarRef.container && c.isPhysical == conn.isPhysical && c.delay == conn.delay.orNever()
//                    .toCCode()
//            }
//        }
//    }
//
//    /** Finds an existing grouped connection, or creates a new.*/
//    fun getOrCreateNewGroupedConnection(srcVarRef: VarRef, dstVarRef: VarRef, conn: Connection): UcGroupedConnection2 {
//        var res = findExistingGroupedConnection(srcVarRef, dstVarRef, conn)
//        if (res == null) {
//            res = UcGroupedConnection2(srcVarRef, dstVarRef, conn, connections.size)
//            connections.add(res)
//        }
//        return res
//    }
//
//    /** Find the number of grouped connections coming out of a particular port. This is needed to know how many
//     * Connection pointers to allocated on the self-struct of a reactor containing another reactor with an output port. */
//    fun findNumGroupedConnectionsFromPort(srcInst: Instantiation?, srcPort: Port) =
//        connections.filter { c -> c.srcPort == srcPort && c.srcInst == srcInst }.size
//
//    fun getFederatedConnectionBundles(): List<UcFederatedConnectionBundle> {
//        val res = mutableListOf<UcFederatedConnectionBundle>()
//        val connectionsGroupedByBundle: Map<Pair<Instantiation, Instantiation>, List<UcGroupedConnection2>> =
//            connections.groupBy { Pair(it.srcInst!!, it.dstInst!!) }
//
//        for ((key, value) in connectionsGroupedByBundle) {
//            val bundle = UcFederatedConnectionBundle(key.first, key.second, value)
//            res.add(bundle)
//        }
//        return res
//    }
//}

class UcConnectionGenerator(private val reactor: Reactor, private val federate: Instantiation?) {
    private val ucGroupedConnections: List<UcGroupedConnection>
    private val ucFederatedConnectionBundles: List<UcFederatedConnectionBundle>

    init {
        val channels = mutableListOf<UcConnectionChannel>()
        reactor.allConnections.forEach { channels.addAll(UcConnectionChannel.parseConnectionChannels(it)) }
        ucGroupedConnections = UcGroupedConnection.groupConnections(channels)

        ucFederatedConnectionBundles = emptyList()
    }

    companion object {
        val Connection.isFederated
            get(): Boolean = (this.eContainer() as Reactor).isFederated
    }

    fun getNumFederatedConnectionBundles() = ucFederatedConnectionBundles.size

    fun getNumConnectionsFromPort(instantiation: Instantiation?, port: Port): Int {
        var count = 0
        for (groupedConn in ucGroupedConnections) {
            if (groupedConn.srcInst == instantiation && groupedConn.srcPort == port) {
                count += 1
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

    private fun generateFederatedConnectionSelfStruct(conn: UcGroupedConnection) =
        if (conn.srcInst == federate) generateFederatedOutputSelfStruct(conn) else generateFederatedInputSelfStruct(conn)

    private fun generateFederatedConnectionCtor(conn: UcGroupedConnection) =
        if (conn.srcInst == federate) generateFederatedOutputCtor(conn) else generateFederatedInputCtor(conn)

    private fun generateFederatedOutputInstance(conn: UcGroupedConnection) =
        "LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(${reactor.codeType}, ${conn.uniqueName});"

    private fun generateFederatedInputInstance(conn: UcGroupedConnection) =
        "LF_FEDERATED_INPUT_CONNECTION_INSTANCE(${reactor.codeType}, ${conn.uniqueName});"

    private fun generateFederatedConnectionInstance(conn: UcGroupedConnection) =
        if (conn.srcInst == federate) generateFederatedOutputInstance(conn) else generateFederatedInputInstance(conn)

    private fun generateInitializeFederatedOutput(conn: UcGroupedConnection) =
        "LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(${reactor.codeType}, ${conn.uniqueName}, ${conn.serializeFunc});"

    private fun generateInitializeFederatedInput(conn: UcGroupedConnection) =
        "LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(${reactor.codeType}, ${conn.uniqueName}, ${conn.deserializeFunc});"

    private fun generateInitializeFederatedConnection(conn: UcGroupedConnection) =
        if (conn.srcInst == federate) generateInitializeFederatedOutput(conn) else generateInitializeFederatedInput(conn)


    private fun generateReactorCtorCode(conn: UcGroupedConnection) = with(PrependOperator) {
        """
            |${if (conn.isLogical) "LF_INITIALIZE_LOGICAL_CONNECTION(" else "LF_INITIALIZE_DELAYED_CONNECTION("}${reactor.codeType}, ${conn.uniqueName}, ${conn.bankWidth}, ${conn.portWidth});
            """.trimMargin()
    };

    private fun generateFederateCtorCode(conn: UcFederatedConnectionBundle) =
        "LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(${conn.src.codeTypeFederate}, ${conn.dst.codeTypeFederate});"

    private fun generateConnectChannel(groupedConn: UcGroupedConnection, channel: UcConnectionChannel) =
        with(PrependOperator) {
            """|lf_connect((Connection *) &self->${groupedConn.uniqueName}[${channel.src.bank_idx}][${channel.src.port_idx}], (Port *) ${channel.src.generateChannelPointer()}, (Port *) ${channel.dest.generateChannelPointer()});
        """.trimMargin()
        }

    private fun generateConnectionStatements(conn: UcGroupedConnection) = with(PrependOperator) {
        conn.channels
            .joinToString(separator = "\n") { generateConnectChannel(conn, it) }
    }

    private fun generateConnectFederateOutputChannel(bundle: UcFederatedConnectionBundle, conn: UcGroupedConnection) =
        conn.channels.joinWithLn {
            "lf_connect_federated_output((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeTypeFederate}, ${bundle.dst.codeTypeFederate}).${conn.uniqueName}, (Port*) &self->${bundle.src.name}[0].${it.src.varRef.name}[0]);"
        }

    private fun generateConnectFederateInputChannel(bundle: UcFederatedConnectionBundle, conn: UcGroupedConnection) =
        conn.channels.joinWithLn {
            "lf_connect_federated_input((Connection *)&self->LF_FEDERATED_CONNECTION_BUNDLE_NAME(${bundle.src.codeTypeFederate}, ${bundle.dst.codeTypeFederate}).${conn.uniqueName}, (Port*) &self->${bundle.dst.name}[0].${it.dest.varRef.name}[0]);"
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
        ucFederatedConnectionBundles.joinToString(
            prefix = "// Initialize connection bundles\n",
            separator = "\n",
            postfix = "\n"
        ) { generateFederateCtorCode(it) } +
                ucFederatedConnectionBundles.joinToString(
                    prefix = "// Do connections \n",
                    separator = "\n",
                    postfix = "\n"
                ) { generateFederateConnectionStatements(it) }

    fun generateReactorCtorCodes() =
        ucGroupedConnections.joinToString(
            prefix = "// Initialize connections\n",
            separator = "\n",
            postfix = "\n"
        ) { generateReactorCtorCode(it) } +
                ucGroupedConnections.joinToString(
                    prefix = "// Do connections \n",
                    separator = "\n",
                    postfix = "\n"
                ) { generateConnectionStatements(it) }

    fun generateCtors() = ucGroupedConnections.joinToString(
        prefix = "// Connection constructors\n",
        separator = "\n",
        postfix = "\n"
    ) {
        if (it.isFederated) generateFederatedConnectionCtor(it)
        else if (it.isDelayed) generateDelayedCtor(it)
        else generateLogicalCtor(it)
    }

    fun generateSelfStructs() =
        ucGroupedConnections.joinToString(prefix = "// Connection structs\n", separator = "\n", postfix = "\n") {
            if (it.isFederated) generateFederatedConnectionSelfStruct(it)
            else if (it.isLogical) generateLogicalSelfStruct(it)
            else generateDelayedSelfStruct(it)
        }


    fun generateFederatedConnectionBundleSelfStruct(bundle: UcFederatedConnectionBundle) = with(PrependOperator) {
        """ |typedef struct {
            |  FederatedConnectionBundle super;
       ${"  |  "..bundle.channelCodeType} channel;
       ${"  |  "..bundle.groupedConnections.joinWithLn { generateFederatedConnectionInstance(it) }}
            |  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(${bundle.numInputs(federate!!)}, ${
            bundle.numOutputs(
                federate!!
            )
        });
            |} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(${bundle.src.codeTypeFederate}, ${bundle.dst.codeTypeFederate});
        """.trimMargin()
    }

    fun generateFederatedConnectionBundleCtor(bundle: UcFederatedConnectionBundle) = with(PrependOperator) {
        """ |LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(${bundle.src.codeTypeFederate}, ${bundle.dst.codeTypeFederate}) {
            |  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
            |  ${bundle.generateChannelCtor(federate!!)}
            |  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
       ${"  |  "..bundle.groupedConnections.joinWithLn { generateInitializeFederatedConnection(it) }}
            |}
        """.trimMargin()
    }

    fun generateFederatedSelfStructs() = ucFederatedConnectionBundles.joinToString(
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

    fun generateFederatedCtors() = ucFederatedConnectionBundles.joinToString(
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


    fun generateFederateStructFields() = ucFederatedConnectionBundles.joinToString(
        prefix = "// Federated Connections\n",
        separator = "\n",
        postfix = "\n"
    ) {
        "LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(${it.src.codeTypeFederate}, ${it.dst.codeTypeFederate});"
    }

    fun generateReactorStructFields() =
        ucGroupedConnections.joinToString(prefix = "// Connections \n", separator = "\n", postfix = "\n") {
            if (it.isLogical) "LF_LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName}, ${it.bankWidth}, ${it.portWidth});"
            else "LF_DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName}, ${it.bankWidth}, ${it.portWidth});"
        }

    fun getMaxNumPendingEvents(): Int {
        var res = 0
        for (conn in ucGroupedConnections) {
            if (!conn.isLogical) {
                res += conn.maxNumPendingEvents
            }
        }
        return res
    }
}
