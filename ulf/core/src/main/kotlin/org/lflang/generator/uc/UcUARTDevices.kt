package org.lflang.generator.uc

enum class UARTParityBits {
  UART_PARITY_NONE,
  UART_PARITY_EVEN,
  UART_PARITY_ODD,
  UART_PARITY_MARK,
  UART_PARITY_SPACE
}

enum class UARTDataBits {
  UART_DATA_BITS_5,
  UART_DATA_BITS_6,
  UART_DATA_BITS_7,
  UART_DATA_BITS_8
}

enum class UARTStopBits {
  UART_STOP_BITS_1,
  UART_STOP_BITS_2
}

fun UARTDataBitsFromInteger(int: Int): UARTDataBits {
  when (int) {
    5 -> return UARTDataBits.UART_DATA_BITS_5
    6 -> return UARTDataBits.UART_DATA_BITS_6
    7 -> return UARTDataBits.UART_DATA_BITS_7
    8 -> return UARTDataBits.UART_DATA_BITS_8
  }

  throw UnsupportedOperationException("Invalid Integer Value for UARTDataBits")
}

fun UARTStopBitsFromInteger(int: Int): UARTStopBits {
  when (int) {
    1 -> return UARTStopBits.UART_STOP_BITS_1
    2 -> return UARTStopBits.UART_STOP_BITS_2
  }

  throw UnsupportedOperationException("Invalid Integer Value for UARTStopBits")
}

object UARTDeviceManager {
  /**
   * Devices are scoped per federate. Each federate is its own binary on its own board, so
   * `uart_device = 0` on two federates names two different physical devices. Only a collision
   * *within* one federate is an error.
   */
  private val usedPorts = mutableMapOf<String, MutableSet<Int>>()

  @Synchronized
  fun reserve(federate: UcFederate, port: Int) {
    require(port in 0..255) { "UART device $port is out of range, expected 0..255" }
    val used = usedPorts.getOrPut(federate.name) { mutableSetOf() }
    require(used.add(port)) {
      "Federate ${federate.name} declares UART device $port more than once"
    }
  }
}
