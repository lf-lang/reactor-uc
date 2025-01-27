package org.lflang.generator.uc

import org.lflang.AttributeUtils.getInterfaceAttributes
import org.lflang.AttributeUtils.getLinkAttribute
import org.lflang.generator.uc.NetworkChannelType.*
import org.lflang.lf.Attribute

// An enumeration of the supported NetworkChannels
enum class NetworkChannelType {
  TCP_IP,
  CUSTOM,
  COAP_UDP_IP,
  UART,
  NONE
}

object UcNetworkInterfaceFactory {
  private val creators:
      Map<NetworkChannelType, (federate: UcFederate, attr: Attribute) -> UcNetworkInterface> =
      mapOf(
          Pair(TCP_IP) { federate, attr -> UcTcpIpInterface.fromAttribute(federate, attr) },
          Pair(COAP_UDP_IP) { federate, attr ->
            UcCoapUdpIpInterface.fromAttribute(federate, attr)
          },
          Pair(CUSTOM) { federate, attr -> UcCustomInterface.fromAttribute(federate, attr) },
          Pair(UART) { federate, attr -> UcUARTInterface.fromAttribute(federate, attr) })

  fun createInterfaces(federate: UcFederate): List<UcNetworkInterface> {
    val attrs: List<Attribute> = getInterfaceAttributes(federate.inst)
    return if (attrs.isEmpty()) {
      listOf(createDefaultInterface())
    } else {
      attrs.map { createInterfaceFromAttribute(federate, it) }
    }
  }

  private fun createInterfaceFromAttribute(
      federate: UcFederate,
      attr: Attribute
  ): UcNetworkInterface {
    val protocol = attr.attrName.substringAfter("_")
    return when (protocol) {
      "tcp" -> creators.get(TCP_IP)!!.invoke(federate, attr)
      "uart" -> creators.get(UART)!!.invoke(federate, attr)
      "coap" -> creators.get(COAP_UDP_IP)!!.invoke(federate, attr)
      "custom" -> creators.get(CUSTOM)!!.invoke(federate, attr)
      else -> throw IllegalArgumentException("Unrecognized interface attribute $attr")
    }
  }

  private fun createDefaultInterface(): UcNetworkInterface =
      UcTcpIpInterface(ipAddress = IPAddress.fromString("127.0.0.1"))
}

// A NetworkEndpoint is a communication endpoint located at the UcNetworkInterface of a federate.
// A NetworkChannel is between two NetworkEndpoints.
abstract class UcNetworkEndpoint(val iface: UcNetworkInterface)

class UcTcpIpEndpoint(val ipAddress: IPAddress, val port: Int, iface: UcTcpIpInterface) :
    UcNetworkEndpoint(iface) {}

class UcUARTEndpoint(
    val uart_device: Int,
    val baud_rate: Int,
    val data_bits: UARTDataBits,
    val parity: UARTParityBits,
    val stop_bits: UARTStopBits,
    val async: Boolean,
    iface: UcUARTInterface
) : UcNetworkEndpoint(iface) {}

class UcCoapUdpIpEndpoint(val ipAddress: IPAddress, iface: UcCoapUdpIpInterface) :
    UcNetworkEndpoint(iface) {}

class UcCustomEndpoint(iface: UcCustomInterface) : UcNetworkEndpoint(iface) {}

// A federate can have several NetworkInterfaces, which are specified using attributes in the LF
// program.
// A NetworkInterface has a name and can contain a set of endpoints.
abstract class UcNetworkInterface(val type: NetworkChannelType, val name: String) {
  val endpoints = mutableListOf<UcNetworkEndpoint>()

  /**
   * A header file that should be included to support this NetworkInterface. Used by CustomInterface
   */
  abstract val includeHeaders: String

  /** A compile definition which must be defined to get support for this NetworkInterface */
  abstract val compileDefs: String
}

class UcTcpIpInterface(private val ipAddress: IPAddress, name: String? = null) :
    UcNetworkInterface(TCP_IP, name ?: "tcp") {
  private val portManager = IpAddressManager.getPortManager(ipAddress)
  override val includeHeaders: String = ""
  override val compileDefs: String = "NETWORK_CHANNEL_TCP_POSIX"

  fun createEndpoint(port: Int?): UcTcpIpEndpoint {
    val portNum =
        if (port != null) {
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
      val ip =
          if (address != null) {
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

class UcUARTInterface(
    private val uartDevice: Int,
    private val baudRate: Int,
    private val dataBits: UARTDataBits,
    private val parity: UARTParityBits,
    private val stopBits: UARTStopBits,
    private val async: Boolean,
    name: String? = null
) : UcNetworkInterface(UART, name ?: "uart") {

  override val includeHeaders: String = ""
  override val compileDefs: String = "NETWORK_CHANNEL_UART"

  fun createEndpoint(): UcUARTEndpoint {
    val ep = UcUARTEndpoint(uartDevice, baudRate, dataBits, parity, stopBits, async, this)
    endpoints.add(ep)
    return ep
  }

  companion object {
    fun fromAttribute(federate: UcFederate, attr: Attribute): UcUARTInterface {
      val uartDevice = attr.getParamInt("uart_device") ?: 0
      val baudRate = attr.getParamInt("baud_rate") ?: 9600
      val dataBits = UARTDataBits.valueOf(attr.getParamString("data_bits").toString())
      val parity = UARTParityBits.valueOf(attr.getParamString("parity").toString())
      val uartStopBits = UARTStopBits.valueOf(attr.getParamString("stop_bits").toString())
      val async = attr.getParamString("async").toBoolean() ?: true;
      val name = attr.getParamString("name")
      UARTDeviceManager.reserve(uartDevice)
      return UcUARTInterface(uartDevice, baudRate, dataBits, parity, uartStopBits, async, name)
    }
  }
}

class UcCoapUdpIpInterface(private val ipAddress: IPAddress, name: String? = null) :
    UcNetworkInterface(COAP_UDP_IP, name ?: "coap") {
  override val includeHeaders: String = ""
  override val compileDefs: String = "NETWORK_CHANNEL_COAP"

  fun createEndpoint(): UcCoapUdpIpEndpoint {
    val ep = UcCoapUdpIpEndpoint(ipAddress, this)
    endpoints.add(ep)
    return ep
  }

  companion object {
    fun fromAttribute(federate: UcFederate, attr: Attribute): UcCoapUdpIpInterface {
      val address = attr.getParamString("address")
      val name = attr.getParamString("name")
      val ip =
          if (address != null) {
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

/** A UcNetworkChannel is created by giving two endpoints and deciding which one is the server */
abstract class UcNetworkChannel(
    val type: NetworkChannelType,
    val src: UcNetworkEndpoint,
    val dest: UcNetworkEndpoint,
    val serverLhs: Boolean,
) {
  /** Generate code calling the constructor of the source endpoint */
  abstract fun generateChannelCtorSrc(): String

  /** Generate code calling the constructor of the destination endpoint */
  abstract fun generateChannelCtorDest(): String

  abstract val codeType: String

  companion object {
    /**
     * Given a FederatedConnection bundle which contains an LF connection and all the connection
     * channels. Create an endpoint at source and destination and a UcNetworkChannel connecting the,
     */
    fun createNetworkEndpointsAndChannelForBundle(
        bundle: UcFederatedConnectionBundle
    ): UcNetworkChannel {
      val attr: Attribute? = getLinkAttribute(bundle.groupedConnections.first().lfConn)
      var srcIf: UcNetworkInterface
      var destIf: UcNetworkInterface
      var channel: UcNetworkChannel
      var serverLhs = true
      var serverPort: Int? = null

      if (attr == null) {
        // If there is no @link attribute on the connection we just get the default (unless there
        //  is ambiguity)
        srcIf = bundle.src.getDefaultInterface()
        destIf = bundle.dest.getDefaultInterface()
      } else {
        // Parse the @link attribute and generate a UcNetworkChannel between the correct
        // interfaces.
        val srcIfName = attr.getParamString("left")
        val destIfName = attr.getParamString("right")
        val serverSideAttr = attr.getParamString("server_side")
        serverPort = attr.getParamInt("server_port")
        srcIf =
            if (srcIfName != null) bundle.src.getInterface(srcIfName)
            else bundle.src.getDefaultInterface()
        destIf =
            if (destIfName != null) bundle.dest.getInterface(destIfName)
            else bundle.dest.getDefaultInterface()
        serverLhs = if (serverSideAttr == null) true else !serverSideAttr!!.equals("right")
      }

      require(srcIf.type == destIf.type)
      when (srcIf.type) {
        TCP_IP -> {
          val srcEp =
              (srcIf as UcTcpIpInterface).createEndpoint(if (serverLhs) serverPort else null)
          val destEp =
              (destIf as UcTcpIpInterface).createEndpoint(if (!serverLhs) serverPort else null)
          channel = UcTcpIpChannel(srcEp, destEp, serverLhs)
        }

        UART -> {
          val srcEp = (srcIf as UcUARTInterface).createEndpoint()
          val destEp = (srcIf as UcUARTInterface).createEndpoint()
          channel = UcUARTChannel(srcEp, destEp)
        }

        COAP_UDP_IP -> {
          val srcEp = (srcIf as UcCoapUdpIpInterface).createEndpoint()
          val destEp = (destIf as UcCoapUdpIpInterface).createEndpoint()
          channel = UcCoapUdpIpChannel(srcEp, destEp)
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
      "TcpIpChannel_ctor(&self->channel, \"${if (serverLhs) srcTcp.ipAddress.address else destTcp.ipAddress.address}\", ${if (serverLhs) srcTcp.port else destTcp.port}, AF_INET, ${serverLhs});"

  override fun generateChannelCtorDest() =
      "TcpIpChannel_ctor(&self->channel, \"${if (serverLhs) srcTcp.ipAddress.address else destTcp.ipAddress.address}\", ${if (serverLhs) srcTcp.port else destTcp.port}, AF_INET, ${!serverLhs});"

  override val codeType: String
    get() = "TcpIpChannel"
}

class UcUARTChannel(private val uart_src: UcUARTEndpoint, private val uart_dest: UcUARTEndpoint) :
    UcNetworkChannel(UART, uart_src, uart_dest, false) {

  override fun generateChannelCtorSrc() =
      "UART${if (uart_src.async) "Async" else "Poll"}Channel_ctor(&self->channel, ${uart_src.uart_device}, ${uart_src.baud_rate}, UC_${uart_src.data_bits}, UC_${uart_src.parity}, UC_${uart_src.stop_bits});"

  override fun generateChannelCtorDest() =
      "UART${if (uart_src.async) "Async" else "Poll"}Channel_ctor(&self->channel, ${uart_dest.uart_device}, ${uart_dest.baud_rate}, UC_${uart_dest.data_bits}, UC_${uart_dest.parity}, UC_${uart_dest.stop_bits});"

  override val codeType: String
    get() = "UART${if (uart_src.async) "Async" else "Poll"}Channel" //TODO: this is a problem if the different sides use different implementations FIXME
}

class UcCoapUdpIpChannel(
    src: UcCoapUdpIpEndpoint,
    dest: UcCoapUdpIpEndpoint,
    // TODO: In CoAP every node is a server and a client => default server to false for now
) : UcNetworkChannel(COAP_UDP_IP, src, dest, false) {
  private val srcAddr = src
  private val destAddr = dest

  private fun getIpProtocolFamily(ip: IPAddress): String {
    return when (ip) {
      is IPAddress.IPv4 -> "AF_INET"
      is IPAddress.IPv6 -> "AF_INET6"
      else -> throw IllegalArgumentException("Unknown IP address type")
    }
  }

  override fun generateChannelCtorSrc() =
      "CoapUdpIpChannel_ctor(&self->channel, \"${destAddr.ipAddress.address}\", ${getIpProtocolFamily(destAddr.ipAddress)});"

  override fun generateChannelCtorDest() =
      "CoapUdpIpChannel_ctor(&self->channel, \"${srcAddr.ipAddress.address}\", ${getIpProtocolFamily(srcAddr.ipAddress)});"

  override val codeType: String
    get() = "CoapUdpIpChannel"
}

class UcCustomChannel(
    src: UcCustomEndpoint,
    dest: UcCustomEndpoint,
    serverLhs: Boolean = true,
) : UcNetworkChannel(CUSTOM, src, dest, serverLhs) {
  val srcIface = src.iface as UcCustomInterface
  val destIface = dest.iface as UcCustomInterface
  private val srcArgs = if (srcIface.args != null) ", ${srcIface.args}" else ""
  private val destArgs = if (destIface.args != null) ", ${destIface.args}" else ""

  override fun generateChannelCtorSrc() = "${srcIface.name}_ctor(&self->channel, ${srcArgs});"

  override fun generateChannelCtorDest() = "${destIface.name}_ctor(&self->channel, ${destArgs});"

  override val codeType: String
    get() = srcIface.name
}
