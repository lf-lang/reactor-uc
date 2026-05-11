package org.lflang.target;

/** Enumeration of supported platforms */
public class PlatformType {
  public enum Platform {
    AUTO,
    ARDUINO, // FIXME: not multithreaded
    NATIVE("Native"),
    NRF52("nRF52"),
    RP2040("Rp2040"),
    LINUX("Linux"),
    MAC("Darwin"),
    ZEPHYR("Zephyr"),
    RIOT("RIOT"),
    FLEXPRET("FlexPRET"),
    WINDOWS("Windows"),
    PATMOS("Patmos"),
    ESPIDF("ESP-IDF"),
    FREERTOS("FreeRTOS");

    final String cMakeName;

    Platform() {
      this.cMakeName = this.toString();
    }

    Platform(String cMakeName) {
      this.cMakeName = cMakeName;
    }

    /** Return the name in lower case. */
    @Override
    public String toString() {
      return this.name().toLowerCase();
    }

    public Platform getDefault() {
      return Platform.AUTO;
    }
  }
}
