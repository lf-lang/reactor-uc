package org.lflang.generator.uc

import java.net.InetAddress
import java.net.UnknownHostException
import java.util.concurrent.atomic.AtomicInteger

/** A class representing an IPAddress, either v4 or v6. */
sealed class IPAddress {
  abstract val address: String

  override fun toString(): String = address

  override fun equals(other: Any?): Boolean {
    return address == other
  }

  data class IPv4(override val address: String) : IPAddress() {
    init {
      require(isValidIPv4(address)) { "Invalid IPv4 address: $address" }
    }

    companion object {
      fun isValidIPv4(ip: String): Boolean {
        return try {
          InetAddress.getByName(ip) is java.net.Inet4Address
        } catch (e: UnknownHostException) {
          false
        }
      }
    }
  }

  data class IPv6(override val address: String) : IPAddress() {
    init {
      require(isValidIPv6(address)) { "Invalid IPv6 address: $address" }
    }

    companion object {
      fun isValidIPv6(ip: String): Boolean {
        return try {
          InetAddress.getByName(ip) is java.net.Inet6Address
        } catch (e: UnknownHostException) {
          false
        }
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
          require(lastOctet < (255 - count)) {
            "Cannot increment the last octet of an IPv4 address beyond 255"
          }
          IPv4(octets.dropLast(1).joinToString(".") + "." + (lastOctet + count))
        }

        is IPv6 -> {
          // Parse the address instead of splitting the string, since a compressed address
          // such as "fd00::" has an empty last segment.
          val bytes = InetAddress.getByName(address.address).address
          val lastSegment = ((bytes[14].toInt() and 0xFF) shl 8) or (bytes[15].toInt() and 0xFF)
          val incremented = lastSegment + count
          require(incremented <= 0xFFFF) {
            "Cannot increment the last segment of an IPv6 address beyond FFFF"
          }
          bytes[14] = (incremented shr 8).toByte()
          bytes[15] = incremented.toByte()
          IPv6(InetAddress.getByAddress(bytes).hostAddress)
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

object IpAddressManager {
  private val usedIps = mutableSetOf<IPAddress>()
  private val portManagers = mutableMapOf<IPAddress, IpPortManager>()

  @Synchronized
  fun acquireIp(ip: IPAddress) {
    if (ip != IPAddress.fromString("127.0.0.1")) {
      require(!usedIps.contains(ip))
      usedIps.add(ip)
    }
  }

  fun getPortManager(ip: IPAddress): IpPortManager {
    if (portManagers.contains(ip)) {
      return portManagers[ip]!!
    } else {
      val newManager = IpPortManager()
      portManagers[ip] = newManager
      return newManager
    }
  }
}

class UcIpAddress {}
