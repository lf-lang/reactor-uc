package org.lflang.generator.uc

import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasPhysicalActions
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.target.TargetConfig
import org.lflang.target.property.FastProperty
import org.lflang.target.property.KeepaliveProperty
import org.lflang.target.property.TimeOutProperty
import org.lflang.toUnixString

abstract class UcMainGenerator(
    val targetConfig: TargetConfig,
    val numEvents: Int,
    val numReactions: Int
) {
  abstract fun generateStartSource(): String

  val eventQueueName = "Main_EventQueue"
  val systemEventQueueName = "Main_SystemEventQueue"
  val reactionQueueName = "Main_ReactionQueue"

  abstract fun getNumSystemEvents(): Int

  abstract fun keepAlive(): Boolean

  fun generateIncludeScheduler() = """#include "reactor-uc/schedulers/dynamic/scheduler.h" """

  open fun generateInitializeScheduler() =
      "DynamicScheduler_ctor(&_scheduler, _lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getDuration()}, ${keepAlive()});"

  fun getDuration() =
      if (targetConfig.isSet(TimeOutProperty.INSTANCE))
          targetConfig.get(TimeOutProperty.INSTANCE).toCCode()
      else "FOREVER"

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
    numEvents: Int,
    numReactions: Int,
    private val fileConfig: UcFileConfig,
) : UcMainGenerator(targetConfig, numEvents, numReactions) {

  private val ucParameterGenerator = UcParameterGenerator(main)

  override fun getNumSystemEvents(): Int = 0

  override fun keepAlive(): Boolean {
    if (targetConfig.isSet(KeepaliveProperty.INSTANCE)) {
      return targetConfig.get(KeepaliveProperty.INSTANCE)
    } else {
      return main.hasPhysicalActions()
    }
  }

  override fun generateStartSource() =
      with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
        ${" |"..generateIncludeScheduler()}
            |#include "${fileConfig.getReactorHeaderPath(main).toUnixString()}"
            |LF_DEFINE_ENVIRONMENT_STRUCT(${main.codeType}, ${numEvents}, ${numReactions})
            |LF_DEFINE_ENVIRONMENT_CTOR(${main.codeType})
            |static ${main.codeType} main_reactor;
            |static Environment_${main.codeType} environment;
            |Environment *_lf_environment = (Environment *) &environment;
            |void lf_exit(void) {
            |   Environment_free(_lf_environment);
            |}
            |void lf_start(void) {
            |    Environment_${main.codeType}_ctor(&environment, &main_reactor, ${getDuration()}, ${keepAlive()}, ${fast()});
            |    ${main.codeType}_ctor(&main_reactor, NULL, _lf_environment ${ucParameterGenerator.generateReactorCtorDefaultArguments()});
            |    _lf_environment->assemble(_lf_environment);
            |    _lf_environment->start(_lf_environment);
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
    numEvents: Int,
    numReactions: Int,
    private val fileConfig: UcFileConfig,
) : UcMainGenerator(targetConfig, numEvents, numReactions) {

  private val top = currentFederate.inst.eContainer() as Reactor
  private val main = currentFederate.inst.reactor
  private val ucConnectionGenerator = UcConnectionGenerator(top, currentFederate, otherFederates)
  private val netBundlesSize = ucConnectionGenerator.getNumFederatedConnectionBundles()
  private val clockSync = UcClockSyncGenerator(currentFederate, ucConnectionGenerator, targetConfig)
  private val startupCoordinator =
      UcStartupCoordinatorGenerator(currentFederate, ucConnectionGenerator)

  override fun getNumSystemEvents(): Int {
    val clockSyncSystemEvents = clockSync.numSystemEvents
    val startupCoordinatorEvents = startupCoordinator.numSystemEvents
    return clockSyncSystemEvents + startupCoordinatorEvents
  }

  override fun keepAlive(): Boolean {
    return if (targetConfig.isSet(KeepaliveProperty.INSTANCE)) {
      targetConfig.get(KeepaliveProperty.INSTANCE)
    } else {
      if (main.inputs.isNotEmpty()) {
        true
      } else if (top.hasPhysicalActions()) {
        true
      } else {
        false
      }
    }
  }

  override fun generateInitializeScheduler() =
      "DynamicScheduler_ctor(&_scheduler, _lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getDuration()}, ${keepAlive()});"

  override fun generateStartSource() =
      with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
        ${" |"..generateIncludeScheduler()}
            |#include "lf_federate.h"
            |LF_DEFINE_FEDERATE_ENVIRONMENT_STRUCT(${currentFederate.codeType}, ${numEvents}, ${numReactions}, ${netBundlesSize}, ${startupCoordinator.numSystemEvents}, ${clockSync.numSystemEvents})
            |LF_DEFINE_FEDERATE_ENVIRONMENT_CTOR(${currentFederate.codeType}, ${netBundlesSize}, ${ucConnectionGenerator.getLongestFederatePath()}, ${clockSync.enabled}, ${currentFederate.clockSyncParams.grandmaster}, ${currentFederate.clockSyncParams.period}, ${currentFederate.clockSyncParams.maxAdj}, ${currentFederate.clockSyncParams.Kp}, ${currentFederate.clockSyncParams.Ki})
            |static ${currentFederate.codeType} main_reactor;
            |static Environment_${currentFederate.codeType} environment;
            |Environment *_lf_environment = (Environment *) &environment;
            |void lf_exit(void) {
            |   FederateEnvironment_free(&environment.super);
            |}
            |void lf_start(void) {
            |    Environment_${currentFederate.codeType}_ctor(&environment, &main_reactor, ${getDuration()}, ${fast()});
            |    ${currentFederate.codeType}_ctor(&main_reactor, NULL, _lf_environment);
            |    _lf_environment->assemble(_lf_environment);
            |    _lf_environment->start(_lf_environment);
            |    lf_exit();
            |}
        """
            .trimMargin()
      }
}
