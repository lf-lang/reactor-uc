package org.lflang.generator.uc

import java.util.concurrent.atomic.AtomicInteger
import org.lflang.generator.uc.NetworkChannelType.*

enum class NetworkChannelType {
    TcpIp
}

// Singleton object to manage port numbers
object IpPortManager {
    private val currentPort = AtomicInteger(1024) // Starting port number
    private val usedPorts = mutableSetOf<Int>()

    @Synchronized
    fun acquirePortNumber(): Int {

        while (true) {
            val port = currentPort.getAndIncrement()
            if (port in 1024..65535 && usedPorts.add(port)) {
                return port
            }
        }
    }

    @Synchronized
    fun reservePortNumber(port: Int) {
        assert(port < 65535)
        assert(!usedPorts.contains(port))
        usedPorts.add(port)
    }

}

abstract class UcNetworkEndpoint(val iface: UcNetworkInterface)

class UcTcpIpEndpoint(val ipAddress: String, val port: Int, iface: UcTcpIpInterface): UcNetworkEndpoint(iface) {

}

abstract class UcNetworkInterface(val type: NetworkChannelType) {
    abstract fun connectTo(other: UcNetworkInterface): UcNetworkChannel
    val endpoints = mutableListOf<UcNetworkEndpoint>()
}

class UcTcpIpInterface(ipAddress: String? = null) : UcNetworkInterface(TcpIp) {

    val ipAddress = ipAddress ?: "127.0.0.1"

    fun createEndpoint(port: Int?): UcTcpIpEndpoint {
        val portNum = if (port != null) {
            IpPortManager.reservePortNumber(port)
            port
        } else {
            IpPortManager.acquirePortNumber()
        }
        val ep = UcTcpIpEndpoint(ipAddress, portNum, this)
        endpoints.add(ep)
        return ep
    }

    override fun connectTo(other: UcNetworkInterface): UcNetworkChannel {
        TODO("Not yet implemented")
    }
}

abstract class UcNetworkChannel(
    val src: UcNetworkEndpoint,
    val dest: UcNetworkEndpoint,
    val serverLhs: Boolean,
) {
    abstract fun generateChannelCtorSrc(): String
    abstract fun generateChannelCtorDest(): String
    abstract val codeType: String
}

class UcTcpIpChannel(
    val _src: UcTcpIpEndpoint,
    val _dest: UcTcpIpEndpoint,
    serverLhs: Boolean = true,
) : UcNetworkChannel(_src, _dest, serverLhs) {

    override fun generateChannelCtorSrc() =
        "TcpIpChannel_ctor(&self->channel, parent->env, \"${if (serverLhs) _src.ipAddress else _dest.ipAddress}\", ${if (serverLhs) _src.port else _dest.port}, AF_INET, ${serverLhs});"

    override fun generateChannelCtorDest() =
        "TcpIpChannel_ctor(&self->channel, parent->env, \"${if (serverLhs) _src.ipAddress else _dest.ipAddress}\", ${if (serverLhs) _src.port else _dest.port}, AF_INET, ${!serverLhs});"

    override val codeType: String
        get() = "TcpIpChannel"
}
