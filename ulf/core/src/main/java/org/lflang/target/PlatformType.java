package org.lflang.target;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;

/** Enumeration of supported platforms */
public class PlatformType {
  /**
   * A target platform.
   *
   * <p>Each constant carries the CMake name it maps to and the spellings that select it from a
   * {@code @platform} attribute. Those spellings are the single source of truth: {@link
   * Platform#fromAttribute} parses them and {@link Platform#attributeValues} reports them, so
   * parsing, validation and this declaration cannot drift apart.
   */
  public enum Platform {
    AUTO("auto"), // Not selectable via @platform
    ARDUINO("arduino", "ARDUINO"), // FIXME: not multithreaded
    NATIVE("Native", "NATIVE"),
    NRF52("nRF52", "NRF52"),
    // "RP2040" is the legacy spelling and still resolves here.
    PICO("Pico", "PICO", "RP2040"),
    LINUX("Linux", "LINUX"),
    MAC("Darwin", "DARWIN"),
    ZEPHYR("Zephyr", "ZEPHYR"),
    RIOT("RIOT", "RIOT"),
    FLEXPRET("FlexPRET", "FLEXPRET"),
    WINDOWS("Windows", "WINDOWS"),
    PATMOS("Patmos", "PATMOS"),
    ESPIDF("ESP-IDF", "ESP-IDF"),
    FREERTOS("FreeRTOS", "FREERTOS");

    final String cMakeName;
    private final List<String> attributeNames;

    Platform(String cMakeName, String... attributeNames) {
      this.cMakeName = cMakeName;
      this.attributeNames = List.of(attributeNames);
    }

    private static final Map<String, Platform> BY_ATTRIBUTE_NAME = new HashMap<>();

    static {
      for (Platform platform : values()) {
        for (String name : platform.attributeNames) {
          BY_ATTRIBUTE_NAME.put(name.toUpperCase(Locale.ROOT), platform);
        }
      }
    }

    /** Resolve a {@code @platform} value, ignoring case. Empty if the value names no platform. */
    public static Optional<Platform> fromAttribute(String value) {
      return Optional.ofNullable(BY_ATTRIBUTE_NAME.get(value.toUpperCase(Locale.ROOT)));
    }

    /** Every legal {@code @platform} value, for validation and for error messages. */
    public static List<String> attributeValues() {
      return Arrays.stream(values()).flatMap(platform -> platform.attributeNames.stream()).toList();
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
