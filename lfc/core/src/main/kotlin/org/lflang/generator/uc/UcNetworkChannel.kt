package org.lflang.generator.uc

import org.lflang.AttributeUtils.getInterfaceAttributes
import java.util.concurrent.atomic.AtomicInteger
import org.lflang.generator.uc.NetworkChannelType.*
import org.lflang.lf.AttrParm
import org.lflang.lf.Attribute

enum class NetworkChannelType {
    TcpIp
}

// FIXME: Ipv6?
fun addToLastOctet(ip: String, add: Int): String {
    // Split the IP address into its components (octets)
    val octets = ip.split(".").map { it.toInt() }

    // Increment the last octet
    val newLastOctet = (octets[3] + add) % 256 // Wrap around if it exceeds 255

    // Reconstruct the IP address with the incremented last octet
    return "${octets[0]}.${octets[1]}.${octets[2]}.$newLastOctet"
}

fun createDefaultInterface(): UcNetworkInterface = UcTcpIpInterface(globalIpPortManager)

fun getTrimmedAttrParamString(attrParam: AttrParm) = attrParam.value.trim('"')

fun createInterfacesForFederate(federate: UcFederate) {
    val attrs: List<Attribute> = getInterfaceAttributes(federate.inst)
    if (attrs.isEmpty()) {
        federate.addInterface(createDefaultInterface())
    } else {
        val ipAddressManager = IpPortManager()
        for (attr in attrs) {
            val params = HashSet(attr.attrParms)
            val typeAttr = params.find { it.name == "type" }!!
            val addressAttr = params.find { it.name == "address" }
            val argsAttr = params.find { it.name == "address" }

            when (getTrimmedAttrParamString(typeAttr)) {
                "TcpIp" -> {
                    val address = if (addressAttr != null) addToLastOctet(getTrimmedAttrParamString(addressAttr), federate.bankIdx) else null
                    if (address != null) {
                        IpAddressManager.acquireIp(address)
                    }
                    val iface = UcTcpIpInterface(IpPortManager(), address)
                    federate.addInterface(iface)
                }

                else -> {
                    assert(false)
                }
            }
        }
    }
}

class IpPortManager {
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
val globalIpPortManager = IpPortManager()

object IpAddressManager {
    private var currentIp = 1
    private val usedIps = mutableSetOf<String>()

    @Synchronized
    fun acquireIp(ip: String) {
        val ip = "192.168.0.$currentIp"
        assert(usedIps.contains(ip) == false)
        usedIps.add(ip)
    }
}

abstract class UcNetworkEndpoint(val iface: UcNetworkInterface)

class UcTcpIpEndpoint(val ipAddress: String, val port: Int, iface: UcTcpIpInterface) : UcNetworkEndpoint(iface) {

}

abstract class UcNetworkInterface(val type: NetworkChannelType) {
    abstract fun connectTo(other: UcNetworkInterface): UcNetworkChannel
    val endpoints = mutableListOf<UcNetworkEndpoint>()
}

class UcTcpIpInterface(private val portManager: IpPortManager, ipAddress: String? = null) : UcNetworkInterface(TcpIp) {

    val ipAddress = ipAddress ?: "127.0.0.1"

    fun createEndpoint(port: Int?): UcTcpIpEndpoint {
        val portNum = if (port != null) {
            portManager.reservePortNumber(port)
            port
        } else {
            portManager.acquirePortNumber()
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
