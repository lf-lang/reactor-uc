package org.lflang.generator.uc.freertos

import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcFileConfig
import org.lflang.generator.uc.UcMainGeneratorNonFederated
import org.lflang.lf.Reactor
import org.lflang.target.TargetConfig

/**
 * FreeRTOS specific main generator.
 * Generates a main function that creates a FreeRTOS task to run the Lingua Franca application.
 */
class UcFreeRtosMainGenerator(
    main: Reactor,
    targetConfig: TargetConfig,
    numEvents: Int,
    numReactions: Int,
    fileConfig: UcFileConfig,
) : UcMainGeneratorNonFederated(main, targetConfig, numEvents, numReactions, fileConfig) {

  override fun generateMainFunctionName(): String = "main"

  override fun generateMainBody(): String =
    with(PrependOperator) {
      """
        |  xTaskCreate(lf_start, "lf_start", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 2, NULL);
        |  vTaskStartScheduler();
        |
        |  /* If all is well, the scheduler will now be running, and the following
        |  line will never be reached. If the following line does execute, then
        |  there was insufficient FreeRTOS heap memory available for the Idle and/or
        |  timer tasks to be created. See the memory management section on the
        |  FreeRTOS web site for more details on the FreeRTOS heap
        |  http://www.freertos.org/a00111.html. */
        |  for (;;);
        |
        |  return 0;
      """
        .trimMargin()
    }

  override fun generateMainSourceInclude(): String =
    with(PrependOperator) {
      """
        |#include "FreeRTOS.h"
        |#include "task.h"
        |#include "lf_start.h"
      """
        .trimMargin()
    }
}
