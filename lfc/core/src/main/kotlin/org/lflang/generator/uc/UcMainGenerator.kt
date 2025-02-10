package org.lflang.generator.uc

import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.Reactor
import org.lflang.target.TargetConfig
import org.lflang.target.property.FastProperty
import org.lflang.target.property.KeepaliveProperty
import org.lflang.target.property.TimeOutProperty
import org.lflang.toUnixString

abstract class UcMainGenerator(val targetConfig: TargetConfig) {
  abstract fun generateStartSource(): String

  fun getDuration() =
      if (targetConfig.isSet(TimeOutProperty.INSTANCE))
          targetConfig.get(TimeOutProperty.INSTANCE).toCCode()
      else "FOREVER"

  fun keepAlive() = if (targetConfig.isSet(KeepaliveProperty.INSTANCE)) "true" else "false"

  fun fast() = if (targetConfig.isSet(FastProperty.INSTANCE)) "true" else "false"

  fun generateStartHeader() =
      with(PrependOperator) {
        """
            |#ifndef REACTOR_UC_LF_MAIN_H
            |#define REACTOR_UC_LF_MAIN_H
            |
            |void lf_start(void);
            |
            |#endif
            |
        """
            .trimMargin()
      }

  fun generateMainSource() =
      with(PrependOperator) {
        """
            |#include "lf_start.h"
            |int main(void) {
            |  lf_start();
            |  return 0;
            |}
        """
            .trimMargin()
      }
}

class UcMainGeneratorNonFederated(
    private val main: Reactor,
    targetConfig: TargetConfig,
    private val fileConfig: UcFileConfig,
) : UcMainGenerator(targetConfig) {

  private val ucParameterGenerator = UcParameterGenerator(main)

  override fun generateStartSource() =
      with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(main).toUnixString()}"
            |static ${main.codeType} main_reactor;
            |static Environment lf_environment;
            |Environment *_lf_environment = &lf_environment;
            |void lf_exit(void) {
            |   Environment_free(&lf_environment);
            |}
            |void lf_start(void) {
            |    Environment_ctor(&lf_environment, (Reactor *)&main_reactor);
            |    ${main.codeType}_ctor(&main_reactor, NULL, &lf_environment ${ucParameterGenerator.generateReactorCtorDefaultArguments()});
            |    lf_environment.scheduler->duration = ${getDuration()};
            |    lf_environment.scheduler->keep_alive = ${keepAlive()};
            |    lf_environment.fast_mode = ${fast()};
            |    lf_environment.assemble(&lf_environment);
            |    lf_environment.start(&lf_environment);
            |    lf_exit();
            |}
        """
            .trimMargin()
      }
}

class UcMainGeneratorFederated(
    private val currentFederate: UcFederate,
    private val otherFederates: List<UcFederate>,
    targetConfig: TargetConfig,
    private val fileConfig: UcFileConfig,
) : UcMainGenerator(targetConfig) {

  private val top = currentFederate.inst.eContainer() as Reactor
  private val ucConnectionGenerator = UcConnectionGenerator(top, currentFederate, otherFederates)

  override fun generateStartSource() =
      with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
            |#include "lf_federate.h"
            |static ${currentFederate.codeType} main_reactor;
            |static Environment lf_environment;
            |Environment *_lf_environment = &lf_environment;
            |void lf_exit(void) {
            |   Environment_free(&lf_environment);
            |}
            |void lf_start(void) {
            |    Environment_ctor(&lf_environment, (Reactor *)&main_reactor);
            |    lf_environment.scheduler->duration = ${getDuration()};
            |    lf_environment.scheduler->keep_alive = ${keepAlive()};
            |    lf_environment.scheduler->leader = ${top.instantiations.first() == currentFederate.inst && currentFederate.bankIdx == 0};
            |    lf_environment.fast_mode = ${fast()};
            |    lf_environment.has_async_events  = true; // Due to coordination messages. All federates have async inputs.
            |    ${currentFederate.codeType}_ctor(&main_reactor, NULL, &lf_environment);
            |    lf_environment.net_bundles_size = ${ucConnectionGenerator.getNumFederatedConnectionBundles()};
            |    lf_environment.net_bundles = (FederatedConnectionBundle **) &main_reactor._bundles;
            |    lf_environment.assemble(&lf_environment);
            |    lf_environment.start(&lf_environment);
            |    lf_exit();
            |}
        """
            .trimMargin()
      }
}
