package org.lflang.generator.uc

import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.lf.Reactor
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

  abstract fun generateDefineScheduler(): String

  abstract fun generateIncludeScheduler(): String

  abstract fun generateInitializeScheduler(): String

  fun getDuration() =
      if (targetConfig.isSet(TimeOutProperty.INSTANCE))
          targetConfig.get(TimeOutProperty.INSTANCE).toCCode()
      else "FOREVER"

  fun keepAlive() = if (targetConfig.isSet(KeepaliveProperty.INSTANCE)) "true" else "false"

  fun fast() = if (targetConfig.isSet(FastProperty.INSTANCE)) "true" else "false"

  fun generateDefineQueues() =
      with(PrependOperator) {
        """
      |// Define queues used by scheduler
      |LF_DEFINE_EVENT_QUEUE(${eventQueueName}, ${numEvents})
      |LF_DEFINE_EVENT_QUEUE(${systemEventQueueName}, ${getNumSystemEvents()})
      |LF_DEFINE_REACTION_QUEUE(${reactionQueueName}, ${numReactions})
    """
            .trimMargin()
      }

  fun generateInitializeQueues() =
      with(PrependOperator) {
        """
      |// Define queues used by scheduler
      |LF_INITIALIZE_EVENT_QUEUE(${eventQueueName}, ${numEvents})
      |LF_INITIALIZE_EVENT_QUEUE(${systemEventQueueName}, ${getNumSystemEvents()})
      |LF_INITIALIZE_REACTION_QUEUE(${reactionQueueName}, ${numReactions})
    """
            .trimMargin()
      }

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

  override fun generateDefineScheduler() =
      """
    |static DynamicScheduler _scheduler;
    |static Scheduler* scheduler = &_scheduler.super;
  """
          .trimMargin()

  override fun generateIncludeScheduler() =
      """#include "reactor-uc/schedulers/dynamic/scheduler.h" """

  override fun generateInitializeScheduler() =
      "DynamicScheduler_ctor(&_scheduler, &lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getDuration()}, ${keepAlive()});"

  override fun getNumSystemEvents(): Int = 0

  override fun generateStartSource() =
      with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
        ${" |"..generateIncludeScheduler()}
            |#include "${fileConfig.getReactorHeaderPath(main).toUnixString()}"
            |static ${main.codeType} main_reactor;
            |static Environment lf_environment;
            |Environment *_lf_environment = &lf_environment;
        ${" |"..generateDefineQueues()}
        ${" |"..generateDefineScheduler()}
            |void lf_exit(void) {
            |   Environment_free(&lf_environment);
            |}
            |void lf_start(void) {
        ${" |  "..generateInitializeQueues()}
        ${" |  "..generateInitializeScheduler()}
            |    Environment_ctor(&lf_environment, (Reactor *)&main_reactor, scheduler, ${fast()});
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
  private val ucConnectionGenerator = UcConnectionGenerator(top, currentFederate, otherFederates)
  private val netBundlesSize = ucConnectionGenerator.getNumFederatedConnectionBundles()
  private val clockSyncGenerator =
      UcClockSyncGenerator(currentFederate, ucConnectionGenerator, targetConfig)
  private val longestPath = 0

  override fun getNumSystemEvents(): Int {
    val clockSyncSystemEvents = UcClockSyncGenerator.getNumSystemEvents(netBundlesSize)
    val startupCoordinatorEvents = UcStartupCoordinatorGenerator.getNumSystemEvents(netBundlesSize)
    return clockSyncSystemEvents + startupCoordinatorEvents
  }

  override fun generateDefineScheduler() =
      """
    |static DynamicSchedulerFederated _scheduler;
    |static Scheduler* scheduler = &_scheduler.super.super;
  """
          .trimMargin()

  override fun generateIncludeScheduler() =
      """#include "reactor-uc/schedulers/dynamic/scheduler_federated.h" """

  override fun generateInitializeScheduler() =
      "DynamicSchedulerFederated_ctor(&_scheduler, &lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getDuration()}, ${keepAlive()});"

  override fun generateStartSource() =
      with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
        ${" |"..generateIncludeScheduler()}
            |#include "lf_federate.h"
            |static ${currentFederate.codeType} main_reactor;
            |static EnvironmentFederated lf_environment;
            |Environment *_lf_environment = &lf_environment.super;
        ${" |"..generateDefineQueues()}
        ${" |"..generateDefineScheduler()}
            |void lf_exit(void) {
            |   EnvironmentFederated_free(&lf_environment);
            |}
            |void lf_start(void) {
        ${" |    "..generateInitializeQueues()}
        ${" |    "..generateInitializeScheduler()}
            |    EnvironmentFederated_ctor(&lf_environment, (Reactor *)&main_reactor, scheduler, ${fast()},  
            |                     (FederatedConnectionBundle **) &main_reactor._bundles, ${netBundlesSize}, &main_reactor.${UcStartupCoordinatorGenerator.instName}.super, 
            |                     ${if (clockSyncGenerator.enabled()) "&main_reactor.${UcClockSyncGenerator.instName}.super" else "NULL"});
            |    ${currentFederate.codeType}_ctor(&main_reactor, NULL, _lf_environment);
            |    _lf_environment->assemble(_lf_environment);
            |    _lf_environment->start(_lf_environment);
            |    lf_exit();
            |}
        """
            .trimMargin()
      }
}
