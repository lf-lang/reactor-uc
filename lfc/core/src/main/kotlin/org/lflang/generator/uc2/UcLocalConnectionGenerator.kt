package org.lflang.generator.uc2

import org.lflang.ir.Connection
import org.lflang.ir.ConnectionKind
import org.lflang.ir.Port
import org.lflang.ir.ReactorInstantiation
import org.lflang.ir.Reactor
import org.lflang.ir.TimeValue
import org.lflang.ir.VariableContainedTriggerRef
import org.lflang.ir.VariableNameTriggerRef
import kotlin.collections.mutableListOf

class ConnectionCharacteristics(
    val src: Port,
    val delay: TimeValue,
    val isPhysical: Boolean,
    val kind: ConnectionKind,
)

class UcLocalConnectionGenerator(val reactor: Reactor) {

    val groupedConnections = generateGroupConnections(reactor.connections)

    val Connection.getUniqueName get() : String = when (sourceRef) {
        is VariableContainedTriggerRef -> "conn_${(this as VariableNameTriggerRef).container.instantiation.name} _${(this as VariableNameTriggerRef).name}_${uid}"
        is VariableNameTriggerRef -> "conn__${(this as VariableNameTriggerRef).name}_${uid}"
        else -> throw IllegalStateException("unreachable")
    }

    fun generateGroupConnections(connections: List<Connection>): List<Connection> {
        var res = mutableMapOf<ConnectionCharacteristics, List<Connection>>()

        for (connection in connections) {
            val characteristics = ConnectionCharacteristics(
                src = connection.source,
                delay = connection.delay,
                isPhysical = connection.isPhysical,
                kind = connection.kind,
            );
            res.getOrPut(characteristics) { mutableListOf() }.also { res[characteristics]?.plus(connection) }
        }

        var results = mutableListOf<Connection>()
        for (pair in res) {
            val targets = pair.value
                .fold(mutableListOf<Port>()) { acc, it ->
                        acc.plus(it.targets)
                        acc.toMutableList()
                };


            results.plus(Connection(
                sourceRef = pair.key.src.triggerRef(pair.value[0].container),
                targetPortRefs = targets.map { it.triggerRef(it.container) },
                kind = pair.key.kind,
                delay = pair.key.delay,
                bufferSize = 10,
                width = 1,
                isIterated = false
            ))
        }

        return results
    }

    fun getNumConnectionsFromPort(instantiation: ReactorInstantiation?, port: Port): Int {
        var count = groupedConnections.first { it.source == port && it.container == instantiation?.reactor }.targetPortRefs.size
        return count
    }

    private fun generateLogicalSelfStruct(conn: Connection) =
        "LF_DEFINE_LOGICAL_CONNECTION_STRUCT(${reactor.codeType},  ${conn.getUniqueName}, ${conn.numDownstreams});"

    private fun generateLogicalCtor(conn: Connection) =
        "LF_DEFINE_LOGICAL_CONNECTION_CTOR(${reactor.codeType},  ${conn.getUniqueName}, ${conn.numDownstreams});"

    private fun generateDelayedSelfStruct(conn: Connection) =
        if (conn.source.dataType.isArray)
            "LF_DEFINE_DELAYED_CONNECTION_STRUCT_ARRAY(${reactor.codeType}, ${conn.getUniqueName}, ${conn.numDownstreams}, ${conn.source.dataType}, ${conn.maxNumPendingEvents}, ${conn.source.dataType.arrayLength});"
        else
            "LF_DEFINE_DELAYED_CONNECTION_STRUCT(${reactor.codeType}, ${conn.getUniqueName}, ${conn.numDownstreams}, ${conn.source.dataType}, ${conn.maxNumPendingEvents});"

    private fun generateDelayedCtor(conn: Connection) =
        "LF_DEFINE_DELAYED_CONNECTION_CTOR(${reactor.codeType}, ${conn.getUniqueName}, ${conn.numDownstreams}, ${conn.maxNumPendingEvents}, ${conn.isPhysical});"


    private fun generateReactorCtorCode(conn: Connection) =
        if (conn.isLogical) {
            "LF_INITIALIZE_LOGICAL_CONNECTION(${reactor.codeType}, ${conn.getUniqueName}, ${conn.width}, ${conn.source.width})"
        } else {
            "LF_INITIALIZE_DELAYED_CONNECTION(${reactor.codeType}, ${conn.getUniqueName}, ${conn.delay}, ${conn.width}, ${conn.source.width})"
        }

    fun generateCtors() =
        groupedConnections.joinToString(
            prefix = "// Connection constructors\n", separator = "\n", postfix = "\n") {
            if (it.isDelayed) generateDelayedCtor(it) else generateLogicalCtor(it)
        }

    fun generateSelfStructs() =
        groupedConnections.joinToString(
            prefix = "// Connection structs\n", separator = "\n", postfix = "\n") {
            if (it.isLogical) generateLogicalSelfStruct(it) else generateDelayedSelfStruct(it)
        }

    fun generateReactorStructFields() =
        groupedConnections.joinToString(
            prefix = "// Connections \n", separator = "\n", postfix = "\n") {
            if (it.isLogical)
                "LF_LOGICAL_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName}, ${it.width}, ${it.width});" //TODO:
            else
                "LF_DELAYED_CONNECTION_INSTANCE(${reactor.codeType}, ${it.getUniqueName}, ${it.width}, ${it.width});" //TODO:
        }

    //private fun generateConnectionStatements(conn: Connection) =
    //    conn.channels.joinToString(separator = "\n") { generateConnectChannel(conn, it) }

    fun generateReactorCtorCodes() =
        groupedConnections.joinToString(
            prefix = "// Initialize connections\n", separator = "\n", postfix = "\n") {
            generateReactorCtorCode(it)
        }
        // +
        //        groupedConnections.joinToString(
        //            prefix = "// Do connections \n", separator = "\n", postfix = "\n") {
        //            generateConnectionStatements(it)
        //        }
}