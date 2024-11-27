package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*


// Representing a runtime connection with a single upstream port. An optimization is to group
// all identical connections (with same delay and is_physical) in a single connection
class UcGroupedConnection(val varRef: VarRef, val conn: Connection, val uid: Int) {
    private val dests = mutableListOf<List<Pair<UcChannel, UcChannel>>>()
    private var destsBuffer = mutableListOf<Pair<UcChannel, UcChannel>>()
    val srcInst: Instantiation? = varRef.container
    val srcPort = varRef.variable as Port
    val bankWidth = srcInst?.width?:1
    val portWidth = srcPort.width
    val uniqueName = "conn_${srcInst?.name?:""}_${srcPort.name}_${uid}"
    val maxNumPendingEvents = 16 // FIXME: Must be derived from the program

    val delay = conn.delay.orNever().toCCode()
    val isPhysical = conn.isPhysical
    val isLogical = !isPhysical && conn.delay == null

    fun numDownstreams() = dests.size

    fun getDests() = dests

    fun addDest(channels: Pair<UcChannel, UcChannel>) {
        destsBuffer.add(channels)
    }

    fun commit() {
        dests.add(destsBuffer)
        destsBuffer = mutableListOf<Pair<UcChannel, UcChannel>>()
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
    val bankWidth = varRef.container?.width?:1
    val portWidth = (varRef.variable as Port).width
    private val channels = ArrayDeque<UcChannel>()

    // Construct the stack of channels belonging to this port.
    init {
        for (i in 0..bankWidth-1) {
            for (j in 0..portWidth-1) {
                channels.add(UcChannel(varRef, j, i))
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


// A class for maintaining all the runtime UcConnections within a reactor.
class UcConnections() {
    val connections = mutableListOf<UcGroupedConnection>();

    // Parse a connection and update the list of UcGroupedConnection. This is non-trivial since a single variable
    // in the connection can have several banks and multiports. In a multi-connection, we might connect some channels
    // to one variable and others to another.
    fun addConnection(conn: Connection) {
        // First translate the variables into our UcPort which also has information of channels (banks x multiports)
        val rhsPorts= conn.rightPorts.map {UcPort(it)}
        var rhsPortIndex = 0
        var lhsPorts= conn.leftPorts.map{UcPort(it)}
        var lhsPortIndex = 0

        // Keep parsing out connections until we are out of right-hand-side (rhs) ports
        while (rhsPortIndex < rhsPorts.size) {
            // First get the current lhs and rhs port and UcGroupedConnection that we are working with
            val lhsPort = lhsPorts.get(lhsPortIndex)
            val rhsPort = rhsPorts.get(rhsPortIndex)
            val ucConnection = getOrCreateNewGroupedConnection(lhsPort.varRef, conn)

            if (rhsPort.channelsLeft() > lhsPort.channelsLeft()) {
                // If we have more channels left in the rhs variable, then we "complete" a downstreamSet fo
                // the lhs, commit it, and move to the next lhs variable
                val rhsChannelsToAdd = rhsPort.takeChannels(lhsPort.channelsLeft())
                val lhsChannelsToAdd = lhsPort.takeRemainingChannels()
                lhsChannelsToAdd.zip(rhsChannelsToAdd).forEach{ ucConnection.addDest(it)}
                ucConnection.commit()
                lhsPortIndex += 1
            } else if (rhsPort.channelsLeft() < lhsPort.channelsLeft()) {
                // If we have more channels left in the lhs variable, we dont complete the downstreamSet yet,
                // we move to the next rhsChannel. Only if this was the very last rhs variable do we commit.
                val numRhsChannelsToAdd = rhsPort.channelsLeft()
                val rhsChannelsToAdd = rhsPort.takeRemainingChannels()
                val lhsChannelsToAdd = lhsPort.takeChannels(numRhsChannelsToAdd)
                lhsChannelsToAdd.zip(rhsChannelsToAdd).forEach{ ucConnection.addDest(it)}
                rhsPortIndex += 1
                if (rhsPortIndex >= rhsPorts.size) {
                    ucConnection.commit()
                }
            } else {
                // Channels are same size
                lhsPort.takeRemainingChannels().zip(rhsPort.takeRemainingChannels()).forEach { ucConnection.addDest(it) }
                ucConnection.commit()
                rhsPortIndex += 1
                lhsPortIndex += 1
            }

            // If we are out of lhs variables, but not rhs, then there should be an iterated connection.
            // We handle it by resetting the lhsChannels variable and index and continuing until
            // we have been thorugh all rhs channels.
            if (lhsPortIndex >= lhsPorts.size && rhsPortIndex < rhsPorts.size) {
                assert(conn.isIterated)
                lhsPorts = conn.leftPorts.map{UcPort(it)}
                lhsPortIndex = 0
            }
            }
        }

    /** Finds an existing GroupedConnection from srcVarRef with matchin connection properties (physical and delay). */
    fun findExistingGroupedConnection(srcVarRef: VarRef, conn: Connection): UcGroupedConnection? {
        return connections.find { c -> c.varRef == srcVarRef && c.isPhysical == conn.isPhysical && c.delay == conn.delay.orNever().toCCode()}
    }

    /** Finds an existing grouped connection, or creates a new.*/
    fun getOrCreateNewGroupedConnection(srcVarRef: VarRef, conn: Connection): UcGroupedConnection {
        var res = findExistingGroupedConnection(srcVarRef, conn)
        if (res == null) {
            res = UcGroupedConnection(srcVarRef, conn, connections.size)
            connections.add(res)
        }
        return res
    }

    /** Find the number of grouped connections coming out of a particular port. This is needed to know how many
     * Connection pointers to allocated on the self-struct of a reactor containing another reactor with an output port. */
    fun findNumGroupedConnectionsFromPort(srcInst: Instantiation?, srcPort: Port) =
        connections.filter{ c -> c.srcPort == srcPort && c.srcInst == srcInst}.size
}

class UcConnectionGenerator(private val reactor: Reactor) {
    private val ucConnections = UcConnections()
    init {
        reactor.connections.forEach { ucConnections.addConnection(it) }
    }

    fun getNumConnectionsFromPort(instantiation: Instantiation?, port: Port): Int {
        return ucConnections.findNumGroupedConnectionsFromPort(instantiation, port)
    }

    private fun generateLogicalSelfStruct(conn: UcGroupedConnection) = "DEFINE_LOGICAL_CONNECTION_STRUCT(${reactor.codeType},  ${conn.uniqueName}, ${conn.numDownstreams()})";
    private fun generateLogicalCtor(conn: UcGroupedConnection) = "DEFINE_LOGICAL_CONNECTION_CTOR(${reactor.codeType},  ${conn.uniqueName}, ${conn.numDownstreams()})";

    private fun generateDelayedSelfStruct(conn: UcGroupedConnection) = "DEFINE_DELAYED_CONNECTION_STRUCT(${reactor.codeType}, ${conn.uniqueName}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay})";
    private fun generateDelayedCtor(conn: UcGroupedConnection) = "DEFINE_DELAYED_CONNECTION_CTOR(${reactor.codeType}, ${conn.uniqueName}, ${conn.numDownstreams()}, ${conn.srcPort.type.toText()}, ${conn.maxNumPendingEvents}, ${conn.delay}, ${conn.isPhysical})";


    private fun generateReactorCtorCode(conn: UcGroupedConnection)  =  with(PrependOperator) {
        """
            |${if (conn.isLogical) "INITIALIZE_LOGICAL_CONNECTION(" else "INITIALIZE_DELAYED_CONNECTION("}${reactor.codeType}, ${conn.uniqueName}, ${conn.bankWidth}, ${conn.portWidth});
            """.trimMargin()
    };

    private fun generateConnectChannel(conn: UcGroupedConnection, channels: Pair<UcChannel, UcChannel> ) = with(PrependOperator) {
        """|lf_connect((Connection *) &self->${conn.uniqueName}[${channels.first.bank_idx}][${channels.first.port_idx}], (Port *) ${channels.first.generateChannelPointer()}, (Port *) ${channels.second.generateChannelPointer()});
        """.trimMargin()
    }

    private fun generateConnectionStatements(conn: UcGroupedConnection) = with(PrependOperator) {
        conn.getDests().joinToString(separator="\n"){it.joinToString(separator = "\n") {  generateConnectChannel(conn, it)}}
    }

    fun generateReactorCtorCodes() =
        ucConnections.connections.joinToString(prefix = "// Initialize connections\n", separator = "\n", postfix = "\n") { generateReactorCtorCode(it)} +
        ucConnections.connections.joinToString(prefix = "// Initialize connections\n", separator = "\n", postfix = "\n") { generateConnectionStatements(it)}

    fun generateCtors() = ucConnections.connections.joinToString(prefix = "// Connection constructors\n", separator = "\n", postfix = "\n"){
        if(it.conn.isPhysical || it.conn.delay != null) generateDelayedCtor(it)
        else generateLogicalCtor(it)
    }

    fun generateSelfStructs() = ucConnections.connections.joinToString(prefix = "// Connection structs\n", separator = "\n", postfix = "\n") {
        if (it.isLogical) generateLogicalSelfStruct(it)
        else generateDelayedSelfStruct(it)
    }
    fun generateReactorStructFields() =
        ucConnections.connections.joinToString(prefix = "// Connections \n", separator = "\n", postfix = "\n") {
            if (it.isLogical) "LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName}, ${it.bankWidth}, ${it.portWidth});"
            else "DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName}, ${it.bankWidth}, ${it.portWidth});"
        }

    fun getMaxNumPendingEvents(): Int {
        var res = 0
        for (conn in ucConnections.connections) {
            if (!conn.isLogical) {
                res += conn.maxNumPendingEvents
            }
        }
        return res
    }
}
