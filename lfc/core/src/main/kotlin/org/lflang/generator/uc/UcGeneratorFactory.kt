package org.lflang.generator.uc

import org.lflang.generator.uc.espidf.UcEspIdfCmakeGenerator
import org.lflang.generator.uc.espidf.UcEspIdfMainGenerator
import org.lflang.generator.uc.freertos.UcFreeRtosMainGenerator
import org.lflang.lf.Instantiation
import org.lflang.lf.Reactor
import org.lflang.target.TargetConfig
import org.lflang.target.property.type.PlatformType

/**
 * Factory for creating platform-specific generators (main, cmake, and make generators). This
 * centralizes the platform selection logic and makes it easy to add new platforms.
 *
 * Usage:
 * ```
 * val mainGen = UcGeneratorFactory.createMainGenerator(...)
 * val cmakeGen = UcGeneratorFactory.createCmakeGenerator(...)
 * ```
 */
object UcGeneratorFactory {

  /**
   * Creates the appropriate non-federated main generator based on the target platform
   * configuration.
   *
   * @param main The main reactor
   * @param targetConfig The target configuration containing platform information
   * @param numEvents Number of events in the system
   * @param numReactions Number of reactions in the system
   * @param fileConfig File configuration for path resolution
   * @return A platform-specific UcMainGeneratorNonFederated instance
   */
  fun createMainGenerator(
      main: Reactor,
      targetConfig: TargetConfig,
      numEvents: Int,
      numReactions: Int,
      fileConfig: UcFileConfig
  ): UcMainGeneratorNonFederated {

      // Default case for POSIX, RP2040, ZEPHYR, RIOT, etc.
      return UcMainGeneratorNonFederated(main, targetConfig, numEvents, numReactions, fileConfig)
  }

  /**
   * Creates the appropriate CMake generator based on the target platform configuration.
   *
   * @param mainDef The main instantiation
   * @param targetConfig The target configuration containing platform information
   * @param fileConfig File configuration for path resolution
   * @return A platform-specific UcCmakeGeneratorNonFederated instance
   */
  fun createCmakeGenerator(
      mainDef: Instantiation,
      targetConfig: TargetConfig,
      fileConfig: UcFileConfig
  ): UcCmakeGeneratorNonFederated {

      // Default CMake generator for all other platforms
      return UcCmakeGeneratorNonFederated(mainDef, targetConfig, fileConfig)
  }
}
