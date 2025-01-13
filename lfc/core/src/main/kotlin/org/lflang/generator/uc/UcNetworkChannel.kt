package org.lflang.generator.uc

import org.lflang.AttributeUtils.getInterfaceAttributes
import org.lflang.AttributeUtils.getLinkAttribute
import java.util.concurrent.atomic.AtomicInteger
import org.lflang.generator.uc.NetworkChannelType.*
import org.lflang.lf.Attribute
import java.math.BigInteger

enum class NetworkChannelType {
    TCP_IP, CUSTOM, COAP_UDP_IP
}

sealed class IPAddress {
    abstract val address: String

    override fun toString(): String = address
    override fun equals(other: Any?): Boolean {
        return address.equals(other)
    }


    data class IPv4(override val address: String) : IPAddress() {
        init {
            require(isValidIPv4(address)) { "Invalid IPv4 address: $address" }
        }

        companion object {
            fun isValidIPv4(address: String): Boolean {
                val regex = Regex("^([0-9]{1,3}\\.){3}[0-9]{1,3}$")
                return regex.matches(address) &&
                        address.split(".").all { it.toIntOrNull() in 0..255 }
            }
        }
    }

    data class IPv6(override val address: String) : IPAddress() {
        init {
            require(isValidIPv6(address)) { "Invalid IPv6 address: $address" }
        }

        companion object {
            fun isValidIPv6(address: String): Boolean {
                val regex = Regex("(([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4})|(::)")
                return regex.matches(address)
            }
        }
    }

    companion object {
        fun fromString(address: String): IPAddress {
            return if (IPv4.isValidIPv4(address)) {
                IPv4(address)
            } else if (IPv6.isValidIPv6(address)) {
                IPv6(address)
            } else {
                throw IllegalArgumentException("Invalid IP address: $address")
            }
        }

        // Increment the last octet or segment of the IP
        fun increment(address: IPAddress, count: Int): IPAddress {
            return when (address) {
                is IPv4 -> {
                    val octets = address.address.split(".")
                    val lastOctet = octets.last().toInt()
                    require(lastOctet < (255-count)) { "Cannot increment the last octet of an IPv4 address beyond 255" }
                    IPv4(octets.dropLast(1).joinToString(".") + "." + (lastOctet + count))
                }
                is IPv6 -> {
                    val segments = address.address.split(":")
                    val lastSegment = BigInteger(segments.last(), 16)
                    val incremented = lastSegment + count.toBigInteger()
                    require(incremented <= BigInteger("FFFF", 16)) { "Cannot increment the last segment of an IPv6 address beyond FFFF" }
                    IPv6(segments.dropLast(1).joinToString(":") + ":" + incremented.toString(16))
                }
            }
        }
    }
}
fun getAttrParamString(attr: Attribute, param: String): String? = attr.attrParms.find {it.name == param}?.value?.trim('"')
fun getAttrParamInt(attr: Attribute, param: String): Int? = attr.attrParms.find {it.name == param}?.value?.toInt()

fun createNetworkChannelForBundle(bundle: UcFederatedConnectionBundle): UcNetworkChannel {
    val attr: Attribute? = getLinkAttribute(bundle.groupedConnections.first().lfConn)
    var srcIf: UcNetworkInterface
    var destIf: UcNetworkInterface
    var channel: UcNetworkChannel
    var serverLhs: Boolean = true
    var serverPort: Int? = null

    if (attr == null) {
        srcIf = bundle.src.getDefaultInterface()
        destIf = bundle.dest.getDefaultInterface()
    } else {
        val srcIfName = getAttrParamString(attr, "left")
        val destIfName = getAttrParamString(attr, "right")
        val serverSideAttr = getAttrParamString(attr, "server_side")
        serverPort = getAttrParamInt(attr, "server_port")
        srcIf = if (srcIfName != null) bundle.src.getInterface(srcIfName!!) else bundle.src.getDefaultInterface()
        destIf = if (destIfName != null) bundle.dest.getInterface(destIfName!!) else bundle.dest.getDefaultInterface()
        serverLhs = if (serverSideAttr == null) true else !serverSideAttr!!.equals("right")
    }

    require(srcIf.type == destIf.type)
    when(srcIf.type) {
        TCP_IP -> {
            val srcEp = (srcIf as UcTcpIpInterface).createEndpoint(if (serverLhs) serverPort else null)
            val destEp = (destIf as UcTcpIpInterface).createEndpoint(if (!serverLhs) serverPort else null)
            channel = UcTcpIpChannel(srcEp, destEp, serverLhs)
        }
        COAP_UDP_IP -> {
            val srcEp = (srcIf as UcCoapUdpIpInterface).createEndpoint()
            val destEp = (destIf as UcCoapUdpIpInterface).createEndpoint()
            channel = UcCoapUdpIpChannel(srcEp, destEp, serverLhs)
        }
        CUSTOM -> {
            val srcEp = (srcIf as UcCustomInterface).createEndpoint()
            val destEp = (destIf as UcCustomInterface).createEndpoint()
            channel = UcCustomChannel(srcEp, destEp)
        }
    }

    return channel

}

fun createInterfacesForFederate(federate: UcFederate): List<UcNetworkInterface> {
    val attrs: List<Attribute> = getInterfaceAttributes(federate.inst)
    if (attrs.isEmpty()) {
        return listOf(UcNetworkInterfaceFactory.createDefaultInterface())
    } else {
        return attrs.map { UcNetworkInterfaceFactory.createInterface(federate, it) }
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

object IpAddressManager {
    private val usedIps = mutableSetOf<IPAddress>()
    private val portManagers = mutableMapOf<IPAddress, IpPortManager>()

    @Synchronized
    fun acquireIp(ip: IPAddress) {
        if (ip != IPAddress.fromString("127.0.0.1")) {
            require(usedIps.contains(ip) == false)
            usedIps.add(ip)
        }
    }

    fun getPortManager(ip: IPAddress): IpPortManager {
        if (portManagers.contains(ip)) {
            return portManagers[ip]!!
        } else {
            val newManager = IpPortManager()
            portManagers.put(ip, newManager)
            return newManager
        }
    }
}

abstract class UcNetworkEndpoint(val iface: UcNetworkInterface)

object UcNetworkInterfaceFactory {
    private val creators: Map<NetworkChannelType, (federate: UcFederate, attr: Attribute) -> UcNetworkInterface> = mapOf(
        Pair(TCP_IP, { federate, attr -> UcTcpIpInterface.fromAttribute(federate, attr) }),
        Pair(COAP_UDP_IP, { federate, attr -> UcCoapUdpIpInterface.fromAttribute(federate, attr) }),
        Pair(CUSTOM, { federate, attr -> UcCustomInterface.fromAttribute(federate, attr) })
    )

    fun createInterface(federate: UcFederate, attr: Attribute): UcNetworkInterface {
        val protocol = attr.attrName.substringAfter("_")
        when (protocol) {
            "tcp" -> return creators.get(TCP_IP)!!.invoke(federate, attr)
            "coap" -> return creators.get(COAP_UDP_IP)!!.invoke(federate, attr)
            "custom" -> return creators.get(CUSTOM)!!.invoke(federate, attr)
            else -> throw IllegalArgumentException("Unrecognized interface attribute ${attr}")
        }
    }
    fun createDefaultInterface(): UcNetworkInterface = UcTcpIpInterface(ipAddress = IPAddress.fromString("127.0.0.1"))
}

class UcTcpIpEndpoint(val ipAddress: IPAddress, val port: Int, iface: UcTcpIpInterface) : UcNetworkEndpoint(iface) {}
class UcCoapUdpIpEndpoint(val ipAddress: IPAddress, iface: UcCoapUdpIpInterface) : UcNetworkEndpoint(iface) {}
class UcCustomEndpoint(val _iface: UcCustomInterface) : UcNetworkEndpoint(_iface) {}

abstract class UcNetworkInterface(val type: NetworkChannelType, val name: String) {
    val endpoints = mutableListOf<UcNetworkEndpoint>()
    abstract val includeHeaders: String
    abstract val compileDefs: String
}

class UcTcpIpInterface(private val ipAddress: IPAddress, name: String? = null) : UcNetworkInterface(TCP_IP, name?:"tcp") {
    private val portManager = IpAddressManager.getPortManager(ipAddress)
    override val includeHeaders: String = ""
    override val compileDefs: String= "NETWORK_CHANNEL_TCP_POSIX"

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

    companion object {
        fun fromAttribute(federate: UcFederate, attr: Attribute): UcTcpIpInterface {
            val address = getAttrParamString(attr, "address")
            val name = getAttrParamString(attr, "name")
            val ip = if (address != null) {
                var address = IPAddress.fromString(address)

                if (federate.isBank) {
                    address = IPAddress.increment(address, federate.bankIdx-1)
                }
                address
            } else {
                IPAddress.fromString("127.0.0.1")
            }
            IpAddressManager.acquireIp(ip)
            return UcTcpIpInterface(ip, name)
        }
    }
}

class UcCoapUdpIpInterface(private val ipAddress: IPAddress, name: String? = null) : UcNetworkInterface(COAP_UDP_IP, name?:"coap") {
    private val portManager = IpAddressManager.getPortManager(ipAddress)
    override val includeHeaders: String = ""
    override val compileDefs: String= "NETWORK_CHANNEL_COAP_UDP"

    fun createEndpoint(): UcCoapUdpIpEndpoint {
        val ep = UcCoapUdpIpEndpoint(ipAddress, this)
        endpoints.add(ep)
        return ep
    }

    companion object {
        fun fromAttribute(federate: UcFederate, attr: Attribute): UcCoapUdpIpInterface {
            val address = getAttrParamString(attr, "address")
            val name = getAttrParamString(attr, "name")
            val ip = if (address != null) {
                var address = IPAddress.fromString(address)

                if (federate.isBank) {
                    address = IPAddress.increment(address, federate.bankIdx-1)
                }
                address
            } else {
                IPAddress.fromString("127.0.0.1")
            }
            IpAddressManager.acquireIp(ip)
            return UcCoapUdpIpInterface(ip, name)
        }
    }
}

class UcCustomInterface(name: String, val include: String, val args: String?=null) : UcNetworkInterface(CUSTOM, name) {
    override val compileDefs = ""
    override val includeHeaders: String = "#include \"$include\""

    fun createEndpoint(): UcCustomEndpoint {
        val ep = UcCustomEndpoint(this)
        endpoints.add(ep)
        return ep
    }

    companion object {
        fun fromAttribute(federate: UcFederate, attr: Attribute): UcCustomInterface{
            val name = getAttrParamString(attr, "name")
            val include = getAttrParamString(attr, "include")
            val args = getAttrParamString(attr, "args")
            return UcCustomInterface(name!!, include!!, args)
        }
    }
}

abstract class UcNetworkChannel(
    val type: NetworkChannelType,
    val src: UcNetworkEndpoint,
    val dest: UcNetworkEndpoint,
    val serverLhs: Boolean,
) {
    abstract fun generateChannelCtorSrc(): String
    abstract fun generateChannelCtorDest(): String
    abstract val codeType: String
}

class UcTcpIpChannel(
    src: UcTcpIpEndpoint,
    dest: UcTcpIpEndpoint,
    serverLhs: Boolean = true,
) : UcNetworkChannel(TCP_IP, src, dest, serverLhs) {
    val srcTcp = src
    val destTcp = dest

    override fun generateChannelCtorSrc() =
        "TcpIpChannel_ctor(&self->channel, parent->env, \"${if (serverLhs) srcTcp.ipAddress.address else destTcp.ipAddress.address}\", ${if (serverLhs) srcTcp.port else destTcp.port}, AF_INET, ${serverLhs});"

    override fun generateChannelCtorDest() =
        "TcpIpChannel_ctor(&self->channel, parent->env, \"${if (serverLhs) srcTcp.ipAddress.address else destTcp.ipAddress.address}\", ${if (serverLhs) srcTcp.port else destTcp.port}, AF_INET, ${!serverLhs});"

    override val codeType: String
        get() = "TcpIpChannel"
}

class UcCoapUdpIpChannel(
    src: UcCoapUdpIpEndpoint,
    dest: UcCoapUdpIpEndpoint,
    serverLhs: Boolean = true,
) : UcNetworkChannel(COAP_UDP_IP, src, dest, serverLhs) {
    val srcAddr = src.ipAddress.address
    val destAddr = dest.ipAddress.address

    override fun generateChannelCtorSrc() =
        "CoapUdpIpChannel_ctor(&self->channel, parent->env, \"${if (serverLhs) srcAddr else destAddr}\", AF_INET, ${serverLhs});"

    override fun generateChannelCtorDest() =
        "CoapUdpIpChannel_ctor(&self->channel, parent->env, \"${if (serverLhs) srcAddr else destAddr}\" AF_INET, ${!serverLhs});"


    override val codeType: String
        get() = "CoapUdpIpChannel"
}

class UcCustomChannel(
    src: UcCustomEndpoint,
    dest: UcCustomEndpoint,
    serverLhs: Boolean = true,
) : UcNetworkChannel(CUSTOM, src, dest, serverLhs) {
    val srcName = src.iface.name
    val destName = dest.iface.name
    val srcArgs = if (src._iface.args != null) ", ${src._iface.args}" else ""
    val destArgs = if (dest._iface.args != null) ", ${dest._iface.args}" else ""

    override fun generateChannelCtorSrc() =
        "${srcName}_ctor(&self->channel, parent->env ${srcArgs});"

    override fun generateChannelCtorDest() =
        "${destName}_ctor(&self->channel, parent->env ${destArgs});"

    override val codeType: String
        get() = "${srcName}"
}
