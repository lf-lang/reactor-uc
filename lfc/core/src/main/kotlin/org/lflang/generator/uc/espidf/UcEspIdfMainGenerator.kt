package org.lflang.generator.uc.espidf

import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcFileConfig
import org.lflang.generator.uc.UcMainGeneratorNonFederated
import org.lflang.lf.Reactor
import org.lflang.target.TargetConfig

/**
 * ESP-IDF specific main generator for non-federated applications.
 * Generates app_main() function that directly calls lf_start().
 */
class UcEspIdfMainGenerator(
    main: Reactor,
    targetConfig: TargetConfig,
    numEvents: Int,
    numReactions: Int,
    fileConfig: UcFileConfig,
) : UcMainGeneratorNonFederated(main, targetConfig, numEvents, numReactions, fileConfig) {

  override fun generateMainFunctionName(): String = "app_main"
}

