package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.generator.uc.UcPortGenerator.Companion.width
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*



// This class is an abstraction over LF connections. We take
// the LF connections and split them up into UcGroupedConnections, each of which has
// a single upstream port and possibly several downstream ports. We can then
// generate Connection objects as expected by reactor-uc
class UcGroupedConnection(val srcInst: Instantiation?, val srcPort: Port, val conn: Connection, val connId: Int) {
    private val dests = mutableListOf<VarRef>();
    private val srcParentName: String = if (srcInst != null) srcInst.name else ""
    private val srcPortName: String = srcPort.name
    val bankWidth: Int = srcInst?.width?:1
    val portWidth: Int = srcPort.width

    val bufSize = 12 // FIXME: Set this somehow, probably an annotation?
    val uniqueName = "conn_${srcParentName}_${srcPortName}_${connId}"
    val isLogical = !conn.isPhysical && conn.delay == null

    fun addDst(port: VarRef) {
        dests.add(port)
    }

    fun getDests(): List<VarRef> {
        return dests
    }

    fun generateConnectUpstream(): String {
        if (srcInst != null) {
            return "CONN_REGISTER_UPSTREAM(${uniqueName}, self->${srcInst.name}, ${srcPort.name}, ${bankWidth}, ${portWidth});"
        } else {
            return "CONN_REGISTER_UPSTREAM(${uniqueName}, self, ${srcPort.name}, 1, ${portWidth});"
        }
    }

    private fun generateConnectDownstream(port: VarRef): String {
        if (port.container != null) {
            return "CONN_REGISTER_DOWNSTREAM(${uniqueName}, ${bankWidth}, ${portWidth}, self->${port.container.name}, ${port.name}, ${port.container.width}, ${(port.variable as Port).width});"
        } else {
            return "CONN_REGISTER_DOWNSTREAM(${uniqueName}, ${bankWidth}, ${portWidth}, self, ${port.name}, 1, ${portWidth});"
        }
    }

    fun generateConnectDownstreams() = dests.joinToString(separator = "\n") {generateConnectDownstream(it)}


    companion object {
        /** From a list of UcGroupedConnections, find one that has the same upstream port and delay/isPhysical setting*/
        fun findIdenticalConnectionFromPort(conns: List<UcGroupedConnection>, srcInst: Instantiation?, srcPort: Port, connInfo: Connection): UcGroupedConnection? {
            return conns.find {c -> c.srcInst == srcInst && c.srcPort == srcPort && c.conn.isPhysical== connInfo.isPhysical && c.conn.delay == connInfo.delay}
        }


        fun findIdenticalConnectionFromPortToDest(conns: List<UcGroupedConnection>, srcInst: Instantiation?, srcPort: Port, dest: Instantiation, connInfo: Connection): UcGroupedConnection? {
            return conns.find {c -> c.srcInst == srcInst && c.srcPort == srcPort && c.dests.get(0).container == dest && c.conn.isPhysical== connInfo.isPhysical && c.conn.delay == connInfo.delay}
        }

        /** From a list of UcGroupedConnection, find all that start from the same port.*/
        fun findAllConnectionFromPort(conns: List<UcGroupedConnection>, srcInst: Instantiation?, srcPort: Port): List<UcGroupedConnection>? {
            return conns.filter{c -> c.srcInst == srcInst && c.srcPort == srcPort}
        }
    }
}

open class UcConnectionGenerator(private val reactor: Reactor) {

    private val ucGroupedConnections: List<UcGroupedConnection>

    init {
        val res = mutableListOf<UcGroupedConnection>()
        for (conn: Connection in reactor.connections) {
            for ((index, rhs) in conn.rightPorts.withIndex()) {
                var lhs: VarRef;
                if (conn.isIterated) {
                    lhs = conn.leftPorts.get(0);
                    assert(conn.leftPorts.size == 1)
                } else {
                    assert(conn.leftPorts.size == conn.rightPorts.size)
                    lhs = conn.leftPorts.get(index)
                }
                val lhsPort = getPort(lhs)
                val lhsInst = lhs.container
                // We need to assign a unique ID to each UcGroupedConnection with the same srcPort.
                // we do this by figuring out how many such connections we already have.
                val connsFromPort = UcGroupedConnection.findAllConnectionFromPort(res, lhsInst, lhsPort)?.size

                var ucConn = UcGroupedConnection.findIdenticalConnectionFromPort(res, lhsInst, lhsPort, conn)
                if (ucConn == null) {
                    ucConn = UcGroupedConnection(lhsInst, getPort(lhs), conn, connsFromPort ?: 0)
                    res.add(ucConn)
                }
                ucConn.addDst(rhs)
            }
        }
        ucGroupedConnections = res;
    }

    fun getPort(p: VarRef): Port {
        var r: Reactor

        if (p.container != null) {
            r = p.container.reactor
        } else {
            r = reactor
        }
        val resPort = r.inputs.plus(r.outputs).filter{ it.name == p.name}
        assert(resPort.size == 1)
        return resPort.get(0)
    }

    fun getPortCodeName(p: VarRef): String {
        var res = ""
        if (p.container != null) {
            res = "${p.container.name}.";
        }
        res += p.name
        return res;
    }

    fun getNumConnectionsFromPort(instantiation: Instantiation?, port: Port): Int {
        return UcGroupedConnection.findAllConnectionFromPort(ucGroupedConnections, instantiation, port)?.size?:0
    }

    private fun generateLogicalSelfStruct(conn: UcGroupedConnection) = "DEFINE_LOGICAL_CONNECTION_STRUCT(${reactor.codeType},  ${conn.uniqueName}, ${conn.getDests().size})";
    private fun generateLogicalCtor(conn: UcGroupedConnection) = "DEFINE_LOGICAL_CONNECTION_CTOR(${reactor.codeType},  ${conn.uniqueName}, ${conn.getDests().size})";

    private fun generateDelayedSelfStruct(conn: UcGroupedConnection) = "DEFINE_DELAYED_CONNECTION_STRUCT(${reactor.codeType}, ${conn.uniqueName}, ${conn.getDests().size}, ${conn.srcPort.type.toText()}, ${conn.bufSize}, ${conn.conn.delay.orNever().toCCode()})";
    private fun generateDelayedCtor(conn: UcGroupedConnection) = "DEFINE_DELAYED_CONNECTION_CTOR(${reactor.codeType}, ${conn.uniqueName}, ${conn.getDests().size}, ${conn.srcPort.type.toText()}, ${conn.bufSize}, ${conn.conn.delay.orNever().toCCode()}, ${conn.conn.isPhysical})";


    private fun generateReactorCtorCode(conn: UcGroupedConnection)  =  with(PrependOperator) {
        """
            |${if (conn.isLogical) "INITIALIZE_LOGICAL_CONNECTION(" else "INITIALIZE_DELAYED_CONNECTION("}${reactor.codeType}, ${conn.uniqueName}, ${conn.srcInst?.width?:1}, ${conn.srcPort.width});
        ${" |  "..generateConnectionStatements(conn)}
            |
            """.trimMargin()
    };
    private fun generateConnectionStatements(conn: UcGroupedConnection) = with(PrependOperator) {
        """|
        ${"|"..conn.generateConnectUpstream()}
        ${"|"..conn.generateConnectDownstreams()}
        """.trimMargin()
    }

    open fun generateReactorCtorCodes() = ucGroupedConnections.joinToString(prefix = "// Initialize connections\n", separator = "\n", postfix = "\n") { generateReactorCtorCode(it)}

    open fun generateCtors() = ucGroupedConnections.joinToString(prefix = "// Connection constructors\n", separator = "\n", postfix = "\n"){
        if(it.conn.isPhysical || it.conn.delay != null) generateDelayedCtor(it)
        else generateLogicalCtor(it)
    }

    open fun generateSelfStructs() = ucGroupedConnections.joinToString(prefix = "// Connection structs\n", separator = "\n", postfix = "\n") {
        if (it.isLogical) generateLogicalSelfStruct(it)
        else generateDelayedSelfStruct(it)
    }
    open fun generateReactorStructFields() =
        ucGroupedConnections.joinToString(prefix = "// Connections \n", separator = "\n", postfix = "\n") {
            if (it.isLogical) "LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName}, ${it.bankWidth}, ${it.portWidth});"
            else "DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName}, ${it.bankWidth}, ${it.portWidth});"
        }

}

class UcConnectionBundle(val srcFederate: Instantiation, val destFederate: Instantiation) {
    private val groupedConnections = mutableListOf<UcGroupedConnection>();

}


class UcFederatedConnectionGenerator(private val reactor: Reactor) : UcConnectionGenerator(reactor) {

}
