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
import org.lflang.target.property.type.PlatformType
import org.lflang.target.property.ThreadPolicyProperty
import org.lflang.target.property.type.ThreadPolicyType
import org.lflang.target.property.type.ThreadPolicyType.ThreadPolicy
import org.lflang.toUnixString
import org.lflang.allReactions
import org.lflang.allInstantiations
import org.lflang.lf.Deadline
import org.lflang.lf.Reaction
import org.lflang.lf.ParameterReference
import org.lflang.lf.Expression
import org.lflang.lf.Instantiation
import org.lflang.generator.orNever
import org.lflang.toText
import org.lflang.ast.ASTUtils
import org.lflang.generator.getActualValue
import java.nio.file.Path
import org.lflang.util.FileUtil

abstract class UcMainGenerator(
    val targetConfig: TargetConfig,
    val numEvents: Int,
    val numReactions: Int
) {
  abstract fun generateStartSource(platform: PlatformType.Platform): String

  val eventQueueName = "Main_EventQueue"
  val systemEventQueueName = "Main_SystemEventQueue"
  val reactionQueueName = "Main_ReactionQueue"

  abstract fun getNumSystemEvents(): Int

  abstract fun keepAlive(): Boolean

  /**
   * Collect all deadlines from federates, including nested reactors.
   * This method traverses through all federates and their nested reactor instantiations
   * to find all reactions with deadlines.
   * 
   * @param federates List of federates to collect deadlines from
   * @return List of all deadlines found in the federates and their nested reactors
   */
  fun collectAllDeadlinesFromFederates(federates: List<UcFederate>): List<Long> {
    val allDeadlines = mutableListOf<Long>()
    
    for (federate in federates) {
      // Collect deadlines from the main reactor of this federate
      // Start the instantiation chain with the federate's instantiation
      val mainReactor = federate.inst.reactor
      val initialChain = listOf(federate.inst)
      allDeadlines.addAll(collectDeadlinesFromReactor(mainReactor, initialChain))
      
      // Collect deadlines from all nested reactors in this federate
      allDeadlines.addAll(collectDeadlinesFromNestedReactors(mainReactor, initialChain))
    }
    
    return allDeadlines
  }
  
  /**
   * Resolves a delay expression to a time value in nanoseconds.
   * 
   * @param delay The delay expression (can be a Time literal or ParameterReference)
   * @param instantiationChain The chain of instantiations from innermost to outermost,
   *        used to resolve parameter references through the hierarchy
   * @return The time value in nanoseconds, or 0 if it cannot be determined
   */
  fun getTimeValue(delay: Expression, instantiationChain: List<Instantiation>): Long {
    return when {
      delay is org.lflang.lf.Time -> {
        // Direct time expression
        ASTUtils.toTimeValue(delay).toNanoSeconds()
      }
      delay is ParameterReference -> {
        // Use ASTUtils to resolve the parameter reference, which handles nested
        // parameter references through the instantiation chain
        val result = ASTUtils.getTimeValueFromParameterReference(delay, instantiationChain)
        result ?: 0L
      } 
      else -> {
        0
      }
    }
  }

  /**
   * Collects all deadlines from a single reactor.
   * 
   * @param reactor The reactor to collect deadlines from
   * @param instantiationChain The chain of instantiations leading to this reactor,
   *        from innermost to outermost. Used to resolve parametric deadlines.
   * @return List of deadlines found in this reactor
   */
  fun collectDeadlinesFromReactor(reactor: Reactor, instantiationChain: List<Instantiation>): List<Long> {
    val deadlines = mutableListOf<Long>()
    for (reaction in reactor.allReactions) {
      if (reaction.deadline != null) {
        deadlines.add(getTimeValue(reaction.deadline.delay, instantiationChain))
      }
    }

    return deadlines
  }
  
  /**
   * Recursively collects deadlines from all nested reactors within a given reactor.
   * 
   * The instantiation chain is built by prepending each instantiation as we descend
   * into nested reactors. This follows the pattern used by ASTUtils.initialValue():
   * - instantiationChain[0] is the current instantiation
   * - instantiationChain[1] is the instantiation whose reactor contains [0]
   * - And so on up the hierarchy
   * 
   * @param reactor The reactor to search for nested reactors
   * @param instantiationChain The chain of instantiations leading to this reactor
   * @return List of deadlines found in all nested reactors
   */
  fun collectDeadlinesFromNestedReactors(reactor: Reactor, instantiationChain: List<Instantiation>): List<Long> {
    val nestedDeadlines = mutableListOf<Long>()
    
    // Get all instantiations (nested reactors) in this reactor
    val instantiations = reactor.allInstantiations
    
    for (instantiation in instantiations) {
      val nestedReactor = instantiation.reactor
      
      // Build extended chain by prepending the current instantiation
      // This matches the pattern in ASTUtils.inferPortWidth()
      val extendedChain = listOf(instantiation) + instantiationChain
      
      // Collect deadlines from this nested reactor
      nestedDeadlines.addAll(collectDeadlinesFromReactor(nestedReactor, extendedChain))
      
      // Recursively collect deadlines from nested reactors within this nested reactor
      nestedDeadlines.addAll(collectDeadlinesFromNestedReactors(nestedReactor, extendedChain))
    }
    
    return nestedDeadlines
  }

  /**
   * Returns stats on the deadlines of all federates (or the main reactor only if non federated)
   * in the form of a list of three double values provided in this order:
   * 1. the minimum deadline
   * 2. the median deadline
   * 3. the maximum deadline
   * The stats are returned in milliseconds.
   */
  fun getDeadlineStats(federates: List<UcFederate>, mainReactor: Reactor? = null) : List<Double> {
    val stats = mutableListOf<Double>()

    val allDeadlines = mutableListOf<Long>()
    if (federates.isNotEmpty()) {
      allDeadlines.addAll(collectAllDeadlinesFromFederates(federates))
    } else {
      // Non-federated case: collect from main reactor
      val reactor = mainReactor ?: throw IllegalArgumentException("mainReactor must be provided for non-federated case")
      // For the main reactor, there's no parent instantiation, so we start with an empty chain.
      // Parametric deadlines in the main reactor will use the parameter's default value.
      allDeadlines.addAll(collectDeadlinesFromReactor(reactor, emptyList()))
      allDeadlines.addAll(collectDeadlinesFromNestedReactors(reactor, emptyList()))
    }
    
    if (allDeadlines.isEmpty()) {
      return stats
    }

    allDeadlines.sort()

    // Minimum deadline
    stats.add(allDeadlines.first() / 1000000.0)
    
    // Median deadline
    var median: Double
    if (allDeadlines.size % 2 == 1) {
      median = allDeadlines[allDeadlines.size / 2] / 1000000.0
    } else {
      median = (allDeadlines[allDeadlines.size / 2 - 1] + allDeadlines[allDeadlines.size / 2]) / 2.0 / 1000000.0
    }
    stats.add(median)

    // Maximum deadline
    stats.add(allDeadlines.last() / 1000000.0)

    return stats
  }

  fun generatePriorityFunction(federates: List<UcFederate>, mainReactor: Reactor? = null) : String {
    val stats = getDeadlineStats(federates, mainReactor)
    
    if (stats.isEmpty()) {
      return with(PrependOperator) {
        """
        |// Priority assignment function
        |int get_priority_value(interval_t rel_deadline) {
        |  return 97;
        |}
        """.trimMargin()
      }
    }

    val minDl = stats[0]
    val medDl = stats[1]
    val maxDl = stats[2]
    
    val ret = with(PrependOperator) {
          """
          |// Priority assignment function
          |int get_priority_value(interval_t rel_deadline) {
          |  double d_max = ${maxDl};
          |  double d_min = ${minDl};
          |  double median = ${medDl};
          |  double rel_deadline_ms = rel_deadline / 1000000.0;
          |  // Constants that shape the priority function
          |  double alpha_max = 0.025;
          |  double alpha_min = 0.005;
          |
          |  int prio = 1;
          |  if (d_min == d_max) {
          |    prio = 97;
          |  } else {
          |    double alpha = alpha_max - (median - d_min) / (d_max - d_min) * (alpha_max - alpha_min);
          |    double K = 95 / (exp(-alpha * d_min) - exp(-alpha * d_max));
          |    double P = 97 - 95 * exp(-alpha * d_min) / (exp(-alpha * d_min) - exp(-alpha * d_max));
          |    double continuous_fun_value = K * exp(-alpha * rel_deadline_ms) + P;
          |    prio = (int)round(continuous_fun_value);
          |  }
          |
          |  validate(prio >= 2 && prio <= 97);

          |  return prio;
          |}
          """.trimMargin()
    }

    return ret
  }

  fun generateDefineScheduler() =
      if (isPriorityScheduler()) {
        """
            |static PriorityScheduler _scheduler;
            |static Scheduler* scheduler = &_scheduler.super.super;
          """
                  .trimMargin()      
      } else {
        """
            |static DynamicScheduler _scheduler;
            |static Scheduler* scheduler = &_scheduler.super;
          """
                  .trimMargin()
      }

  fun generateIncludeScheduler() = 
      if (isPriorityScheduler()) {
        """#include "reactor-uc/schedulers/priority/scheduler.h" """
      } else {
        """#include "reactor-uc/schedulers/dynamic/scheduler.h" """
      }

  open fun generateInitializeScheduler() =
      if (isPriorityScheduler()) {
        "PriorityScheduler_ctor(&_scheduler, _lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getDuration()}, ${keepAlive()});"
      } else {
        "DynamicScheduler_ctor(&_scheduler, _lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getDuration()}, ${keepAlive()});"
      }

  fun isPriorityScheduler(): Boolean {
      val threadPolicy = targetConfig.getOrDefault(ThreadPolicyProperty.INSTANCE)
      return threadPolicy == ThreadPolicyType.ThreadPolicy.LF_SCHED_TIMESLICE || 
            threadPolicy == ThreadPolicyType.ThreadPolicy.LF_SCHED_PRIORITY
  }

  fun getDuration() =
      if (targetConfig.isSet(TimeOutProperty.INSTANCE))
          targetConfig.get(TimeOutProperty.INSTANCE).toCCode()
      else "FOREVER"

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
            |#include "reactor-uc/tag.h"
            |
            |void lf_start(void);
            |int get_priority_value(interval_t rel_deadline);
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

  override fun generateStartSource(platform: PlatformType.Platform) =
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
        ${if (platform == PlatformType.Platform.NATIVE) " |"..generatePriorityFunction(emptyList(), main) else ""}
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
  private val clockSyncGenerator =
      UcClockSyncGenerator(currentFederate, ucConnectionGenerator, targetConfig)
  private val longestPath = 0

  override fun getNumSystemEvents(): Int {
    val clockSyncSystemEvents = UcClockSyncGenerator.getNumSystemEvents(netBundlesSize)
    val startupCoordinatorEvents = UcStartupCoordinatorGenerator.getNumSystemEvents(netBundlesSize)
    return clockSyncSystemEvents + startupCoordinatorEvents
  }

  override fun keepAlive(): Boolean {
    if (targetConfig.isSet(KeepaliveProperty.INSTANCE)) {
      return targetConfig.get(KeepaliveProperty.INSTANCE)
    } else {
      if (main.inputs.isNotEmpty()) {
        return true
      } else if (top.hasPhysicalActions()) {
        return true
      } else {
        return false
      }
    }
  }

  override fun generateInitializeScheduler() =
    if (isPriorityScheduler()) {
      "PriorityScheduler_ctor(&_scheduler, _lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getDuration()}, ${keepAlive()});"
    } else {
      "DynamicScheduler_ctor(&_scheduler, _lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getDuration()}, ${keepAlive()});"
    }

  override fun generateStartSource(platform: PlatformType.Platform) =
      with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
        ${" |"..generateIncludeScheduler()}
            |#include "lf_federate.h"
            |#include <math.h>
            |static ${currentFederate.codeType} main_reactor;
            |static FederatedEnvironment lf_environment;
            |Environment *_lf_environment = &lf_environment.super;
        ${" |"..generateDefineQueues()}
        ${" |"..generateDefineScheduler()}
            |void lf_exit(void) {
            |   FederatedEnvironment_free(&lf_environment);
            |}
            |void lf_start(void) {
        ${" |    "..generateInitializeQueues()}
        ${" |    "..generateInitializeScheduler()}
            |    FederatedEnvironment_ctor(&lf_environment, (Reactor *)&main_reactor, scheduler, ${fast()},  
            |                     (FederatedConnectionBundle **) &main_reactor._bundles, ${netBundlesSize}, &main_reactor.${UcStartupCoordinatorGenerator.instName}.super, 
            |                     ${if (clockSyncGenerator.enabled()) "&main_reactor.${UcClockSyncGenerator.instName}.super" else "NULL"});
            |    ${currentFederate.codeType}_ctor(&main_reactor, NULL, _lf_environment);
            |    _lf_environment->assemble(_lf_environment);
            |    _lf_environment->start(_lf_environment);
            |    lf_exit();
            |}
        ${if (platform == PlatformType.Platform.NATIVE) " |"..generatePriorityFunction(otherFederates) else ""}
        """
            .trimMargin()
      }
}
