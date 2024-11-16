package org.lflang.generator.uc

import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.generator.orNever
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.*


// This class is an abstraction over LF connections. Basically we take
// the LF connections and split them up into UcConnections, each of which has
// a single upstream port and possibly several downstream ports. We can then
// generate Connection objects as expected by reactor-uc
class UcGroupedConnection(val srcVar: VarRef, val srcPort: Port, val conn: Connection, val connId: Int) {
    private val dests = mutableListOf<VarRef>();

    val bufSize = 12 // FIXME: Set this somehow, probably an annotation?

    val srcParentName = if (srcVar.container != null) srcVar.container.name else ""
    val srcPortName = srcPort.name
    val uniqueName = "conn_${srcParentName}_${srcPortName}_${connId}"

    val isLogical = !conn.isPhysical && conn.delay == null

    fun addDst(port: VarRef) {
        dests.add(port)
    }

    fun getDests() : List<VarRef> {
        return dests
    }

    companion object {
        fun findIdenticalConnectionFromPort(conns: List<UcGroupedConnection>, port: Port, connInfo: Connection): UcGroupedConnection? {
            return conns.find {c -> c.srcPort == port && c.conn.isPhysical== connInfo.isPhysical && c.conn.delay == connInfo.delay}
        }
        fun findAllConnectionFromPort(conns: List<UcGroupedConnection>, port: Port): List<UcGroupedConnection>? {
            return conns.filter{c -> c.srcPort == port}
        }
    }
}

class UcConnectionGenerator(private val reactor: Reactor) {

    fun getUcConnections(): List<UcGroupedConnection> {
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
                // We need to assign a unique ID to each UcGroupedConnection with the same srcPort.
                // we do this by figuring out how many such connections we already have.
                val connsFromPort = UcGroupedConnection.findAllConnectionFromPort(res, lhsPort)?.size

                var ucConn = UcGroupedConnection.findIdenticalConnectionFromPort(res, lhsPort, conn)
                if (ucConn == null) {
                    ucConn = UcGroupedConnection(lhs, getPort(lhs), conn, connsFromPort?:0)
                    res.add(ucConn)
                }
                ucConn.addDst(rhs)
            }
        }
        return res
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


    // The number of connections coming out of this input/output port. This is a very inefficient way of searching for it.
    // TODO: Fix this
    fun getNumConnectionsFromPort(p: Port): Int {
        val num = UcGroupedConnection.findAllConnectionFromPort(getUcConnections(), p)?.size
        return num?:0
    }
    fun generateLogicalSelfStruct(conn: UcGroupedConnection) = "DEFINE_LOGICAL_CONNECTION_STRUCT(${reactor.codeType},  ${conn.uniqueName}, ${conn.getDests().size})";
    fun generateLogicalCtor(conn: UcGroupedConnection) = "DEFINE_LOGICAL_CONNECTION_CTOR(${reactor.codeType},  ${conn.uniqueName}, ${conn.getDests().size})";

    fun generateDelayedSelfStruct(conn: UcGroupedConnection) = "DEFINE_DELAYED_CONNECTION_STRUCT(${reactor.codeType}, ${conn.uniqueName}, ${conn.getDests().size}, ${conn.srcPort.type.toText()}, ${conn.bufSize}, ${conn.conn.delay.orNever().toCCode()})";
    fun generateDelayedCtor(conn: UcGroupedConnection) = "DEFINE_DELAYED_CONNECTION_CTOR(${reactor.codeType}, ${conn.uniqueName}, ${conn.getDests().size}, ${conn.srcPort.type.toText()}, ${conn.bufSize}, ${conn.conn.delay.orNever().toCCode()}, ${conn.conn.isPhysical})";

    fun generateSelfStructs() = getUcConnections().joinToString(prefix = "// Connection structs\n", separator = "\n", postfix = "\n") {
        if (it.isLogical) generateLogicalSelfStruct(it)
        else generateDelayedSelfStruct(it)
    }
    fun generateReactorStructFields() =
        getUcConnections().joinToString(prefix = "// Connections \n", separator = "\n", postfix = "\n") {
            if (it.isLogical) "LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName});"
            else "DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.uniqueName});"
        }

    fun generateReactorCtorCode(conn: UcGroupedConnection)  =  with(PrependOperator) {
        """
            |${if (conn.isLogical) "INITIALIZE_LOGICAL_CONNECTION(" else "INITIALIZE_DELAYED_CONNECTION("}${reactor.codeType}, ${conn.uniqueName});
        ${" |   "..generateConnectionStatements(conn)}
            |
            """.trimMargin()
    };
    fun generateConnectionStatements(conn: UcGroupedConnection) =
            "CONN_REGISTER_UPSTREAM(self->${conn.uniqueName}, self->${getPortCodeName(conn.srcVar)});\n" +
            conn.getDests().joinToString(separator = "\n") {
            "CONN_REGISTER_DOWNSTREAM(self->${conn.uniqueName}, self->${getPortCodeName(it)});"}

    fun generateReactorCtorCodes() = getUcConnections().joinToString(prefix = "// Initialize connections\n", separator = "\n", postfix = "\n") { generateReactorCtorCode(it)}

    fun generateCtors() = getUcConnections().joinToString(prefix = "// Connection constructors\n", separator = "\n", postfix = "\n"){
        if(it.conn.isPhysical || it.conn.delay != null) generateDelayedCtor(it)
        else generateLogicalCtor(it)
    }

}

