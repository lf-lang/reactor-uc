package org.lflang.generator.uc

import java.util.concurrent.atomic.AtomicInteger

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

object UARTDeviceManager {
  private val currentPort = AtomicInteger(0) // Starting port number
  private val usedPorts = mutableSetOf<Int>()

  @Synchronized
  fun acquireUARTDevice(): Int {
    while (true) {
      val port = currentPort.getAndIncrement()
      if (port in 0..255 && usedPorts.add(port)) {
        return port
      }
    }
  }

  @Synchronized
  fun reserve(port: Int) {
    assert(port < 65535)
    assert(!usedPorts.contains(port))
    usedPorts.add(port)
  }
}
