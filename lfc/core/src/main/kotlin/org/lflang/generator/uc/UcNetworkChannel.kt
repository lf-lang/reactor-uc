package org.lflang.generator.uc

import org.lflang.AttributeUtils.getInterfaceAttributes
import org.lflang.AttributeUtils.getLinkAttribute
import java.util.concurrent.atomic.AtomicInteger
import org.lflang.generator.uc.NetworkChannelType.*
import org.lflang.lf.Attribute
import java.math.BigInteger

enum class NetworkChannelType {
    TCP_IP, CUSTOM, COAP_UDP_IP, NONE
}

abstract class UcNetworkEndpoint(val iface: UcNetworkInterface)

object UcNetworkInterfaceFactory {
    private val creators: Map<NetworkChannelType, (federate: UcFederate, attr: Attribute) -> UcNetworkInterface> =
        mapOf(
            Pair(TCP_IP, { federate, attr -> UcTcpIpInterface.fromAttribute(federate, attr) }),
            Pair(COAP_UDP_IP, { federate, attr -> UcCoapUdpIpInterface.fromAttribute(federate, attr) }),
            Pair(CUSTOM, { federate, attr -> UcCustomInterface.fromAttribute(federate, attr) })
        )

    fun createInterfaces(federate: UcFederate): List<UcNetworkInterface> {
        val attrs: List<Attribute> = getInterfaceAttributes(federate.inst)
        return if (attrs.isEmpty()) {
            listOf(createDefaultInterface())
        } else {
            attrs.map { createInterface(federate, it) }
        }
    }

    fun createInterface(federate: UcFederate, attr: Attribute): UcNetworkInterface {
        val protocol = attr.attrName.substringAfter("_")
        return when (protocol) {
            "tcp" -> creators.get(TCP_IP)!!.invoke(federate, attr)
            "coap" -> creators.get(COAP_UDP_IP)!!.invoke(federate, attr)
            "custom" -> creators.get(CUSTOM)!!.invoke(federate, attr)
            else -> throw IllegalArgumentException("Unrecognized interface attribute $attr")
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

class UcTcpIpInterface(private val ipAddress: IPAddress, name: String? = null) :
    UcNetworkInterface(TCP_IP, name ?: "tcp") {
    private val portManager = IpAddressManager.getPortManager(ipAddress)
    override val includeHeaders: String = ""
    override val compileDefs: String = "NETWORK_CHANNEL_TCP_POSIX"

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
            val address = attr.getParamString("address")
            val name = attr.getParamString("name")
            val ip = if (address != null) {
                var address = IPAddress.fromString(address)

                if (federate.isBank) {
                    address = IPAddress.increment(address, federate.bankIdx - 1)
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

class UcCoapUdpIpInterface(private val ipAddress: IPAddress, name: String? = null) :
    UcNetworkInterface(COAP_UDP_IP, name ?: "coap") {
    override val includeHeaders: String = ""
    override val compileDefs: String = "NETWORK_CHANNEL_COAP_UDP"

    fun createEndpoint(): UcCoapUdpIpEndpoint {
        val ep = UcCoapUdpIpEndpoint(ipAddress, this)
        endpoints.add(ep)
        return ep
    }

    companion object {
        fun fromAttribute(federate: UcFederate, attr: Attribute): UcCoapUdpIpInterface {
            val address = attr.getParamString("address")
            val name = attr.getParamString("name")
            val ip = if (address != null) {
                var address = IPAddress.fromString(address)

                if (federate.isBank) {
                    address = IPAddress.increment(address, federate.bankIdx - 1)
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

class UcCustomInterface(name: String, val include: String, val args: String? = null) :
    UcNetworkInterface(CUSTOM, name) {
    override val compileDefs = ""
    override val includeHeaders: String = "#include \"$include\""

    fun createEndpoint(): UcCustomEndpoint {
        val ep = UcCustomEndpoint(this)
        endpoints.add(ep)
        return ep
    }

    companion object {
        fun fromAttribute(federate: UcFederate, attr: Attribute): UcCustomInterface {
            val name = attr.getParamString("name")
            val include = attr.getParamString("include")
            val args = attr.getParamString("args")
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

    companion object {
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
                val srcIfName = attr.getParamString("left")
                val destIfName = attr.getParamString("right")
                val serverSideAttr = attr.getParamString("server_side")
                serverPort = attr.getParamInt("server_port")
                srcIf =
                    if (srcIfName != null) bundle.src.getInterface(srcIfName!!) else bundle.src.getDefaultInterface()
                destIf =
                    if (destIfName != null) bundle.dest.getInterface(destIfName!!) else bundle.dest.getDefaultInterface()
                serverLhs = if (serverSideAttr == null) true else !serverSideAttr!!.equals("right")
            }

            require(srcIf.type == destIf.type)
            when (srcIf.type) {
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
                NONE -> throw IllegalArgumentException("Tried creating network channel with type=NONE")
            }
            return channel
        }
    }
}

class UcTcpIpChannel(
    src: UcTcpIpEndpoint,
    dest: UcTcpIpEndpoint,
    serverLhs: Boolean = true,
) : UcNetworkChannel(TCP_IP, src, dest, serverLhs) {
    private val srcTcp = src
    private val destTcp = dest

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
    private val srcAddr = src.ipAddress.address
    private val destAddr = dest.ipAddress.address

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
    private val srcName = src.iface.name
    private val destName = dest.iface.name
    private val srcArgs = if (src._iface.args != null) ", ${src._iface.args}" else ""
    private val destArgs = if (dest._iface.args != null) ", ${dest._iface.args}" else ""

    override fun generateChannelCtorSrc() =
        "${srcName}_ctor(&self->channel, parent->env ${srcArgs});"

    override fun generateChannelCtorDest() =
        "${destName}_ctor(&self->channel, parent->env ${destArgs});"

    override val codeType: String
        get() = srcName
}
