package org.lflang.generator.uc.espidf

import org.lflang.generator.uc.UcFileConfig
import org.lflang.generator.uc.UcMainGeneratorNonFederated
import org.lflang.lf.Reactor

/**
 * ESP-IDF specific main generator for non-federated applications. Generates app_main() function
 * that directly calls lf_start().
 */
class UcEspIdfMainGenerator(
    main: Reactor,
    numEvents: Int,
    numReactions: Int,
    fileConfig: UcFileConfig,
) : UcMainGeneratorNonFederated(main, numEvents, numReactions, fileConfig) {

  override fun generateMainFunctionName(): String = "app_main"
}
