package org.lflang.generator.uc

import org.lflang.AttributeUtils
import org.lflang.TimeValue
import org.lflang.allInstantiations
import org.lflang.allModes
import org.lflang.ast.ASTUtils
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeWidth
import org.lflang.generator.uc.UcModeGenerator.Companion.MODE_STATE_FIELD
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.generator.uc.UcReactorGenerator.Companion.hasPhysicalActions
import org.lflang.lf.Attribute
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.toUnixString

abstract class UcMainGenerator(
    val reactor: Reactor,
    val numEvents: Int,
    val numReactions: Int,
) {
  abstract fun generateStartSource(): String

  /**
   * The link-time guard that a program and the runtime archive it links agree about
   * `LF_RUNTIME_EXTENSIONS`, which changes `sizeof(Environment)` and `sizeof(Reaction)`. The symbol
   * it references encodes the option state, so a mismatch fails at link rather than by corrupting
   * adjacent static storage.
   */
  fun generateAbiGuard(): String = "LF_RUNTIME_EXTENSIONS_ABI_GUARD();"

  /**
   * Appends every `lf_mode_state_t` reachable from [reactor] to [states], in preorder of the
   * instantiation tree. `lf_micromode_validate_all` asserts that order.
   */
  private fun collectModeStates(reactor: Reactor, path: String, states: MutableList<String>) {
    if (reactor.allModes.isNotEmpty()) states += "&$path.$MODE_STATE_FIELD"
    for (inst in reactor.allInstantiations) {
      for (i in 0 until inst.codeWidth) {
        collectModeStates(inst.reactor, "$path.${inst.name}[$i]", states)
      }
    }
  }

  /** The whole-program mode registry, or empty when nothing in the program declares a mode. */
  protected fun generateModeProgram(root: Reactor, rootPath: String = "main_reactor"): String {
    val states = mutableListOf<String>()
    collectModeStates(root, rootPath, states)
    if (states.isEmpty()) return ""
    val rows = states.joinToString(",\n") { "    $it" }
    return "#include \"micromode/micromode.h\"\n" +
        "static lf_mode_state_t* _lf_mode_states[${states.size}] = {\n$rows,\n};\n" +
        "static lf_micromode_program_t _lf_program = {\n" +
        "    _lf_mode_states, ${states.size},\n" +
        "    LF_EXTENSION_INSTANCE_INIT(&lf_micromode_extension_descriptor, &_lf_program)};"
  }

  /** Runs before `assemble`, so a mistake in the wiring fails at startup rather than silently. */
  protected fun generateModeValidateCall(modeProgram: String): String =
      if (modeProgram.isEmpty()) "" else "lf_micromode_validate_all(&_lf_program, _lf_environment);"

  val eventQueueName = "Main_EventQueue"
  val systemEventQueueName = "Main_SystemEventQueue"
  val reactionQueueName = "Main_ReactionQueue"

  abstract fun getNumSystemEvents(): Int

  abstract fun keepAlive(): Boolean

  fun generateDefineScheduler() =
      """
        |static DynamicScheduler _scheduler;
        |static Scheduler* scheduler = &_scheduler.super;
        """
          .trimMargin()

  fun generateIncludeScheduler() = """#include "reactor-uc/schedulers/dynamic/scheduler.h" """

  open fun generateInitializeScheduler() =
      "DynamicScheduler_ctor(&_scheduler, _lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getTimeout()}, ${keepAlive()});"

  fun getTimeout(): String {
    val attr = AttributeUtils.findAttributeByName(reactor, "timeout")
    if (attr != null) {
      val time = attr.getAttrParms().get(0).getTime()
      if (time == null) {
        if (attr.getAttrParms().get(0).getValue().equals("forever")) {
          return "FOREVER"
        } else if (attr.getAttrParms().get(0).getValue().equals("never")) {
          return TimeValue.ZERO.toCCode()
        }
      } else {
        return ASTUtils.toTimeValue(time).toCCode()
      }
    }
    return "FOREVER"
  }

  fun fast() = if (AttributeUtils.getFastAttrValue(reactor)) "true" else "false"

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
            |"""
            .trimMargin()
      }

  open fun generateMainFunctionName(): String = "main"

  open fun generateMainBody(): String =
      with(PrependOperator) {
        """
            |  lf_start();
            |  return 0;
            """
            .trimMargin()
      }

  open fun generateMainSourceInclude(): String =
      with(PrependOperator) {
        """
            |#include "lf_start.h"
            """
            .trimMargin()
      }

  fun generateMainSource(): String =
      with(PrependOperator) {
        """
          |${generateMainSourceInclude()}
          |
          |int ${generateMainFunctionName()}(void) {
          |${generateMainBody()}
          |}
        """
            .trimMargin()
      }
}

open class UcMainGeneratorNonFederated(
    protected val main: Reactor,
    numEvents: Int,
    numReactions: Int,
    protected val fileConfig: UcFileConfig,
) : UcMainGenerator(main, numEvents, numReactions) {

  protected val ucParameterGenerator = UcParameterGenerator(main)

  override fun getNumSystemEvents(): Int = 0

  override fun keepAlive(): Boolean {
    val attr: Attribute? = AttributeUtils.findAttributeByName(main, "keepalive")
    if (attr != null) {
      return attr.getAttrParms().get(0).getValue().equals("true", ignoreCase = true)
    } else {
      return main.hasPhysicalActions()
    }
  }

  override fun generateStartSource(): String {
    val modeProgram = generateModeProgram(main)
    return with(PrependOperator) {
      """
            |#include "reactor-uc/reactor-uc.h"
        ${" |"..generateIncludeScheduler()}
            |#include "${fileConfig.getReactorHeaderPath(main).toUnixString()}"
            |static ${main.codeType} main_reactor;
        ${fuseNonEmpty(" |"..modeProgram)}
            |static Environment lf_environment;
            |Environment *_lf_environment = &lf_environment;
        ${" |"..generateDefineQueues()}
        ${" |"..generateDefineScheduler()}
            |void lf_exit(void) {
            |   Environment_free(&lf_environment);
            |}
            |void lf_start(void) {
        ${" |  "..generateAbiGuard()}
        ${" |  "..generateInitializeQueues()}
        ${" |  "..generateInitializeScheduler()}
            |    Environment_ctor(&lf_environment, (Reactor *)&main_reactor, scheduler, ${fast()});
            |    ${main.codeType}_ctor(&main_reactor, NULL, _lf_environment ${ucParameterGenerator.generateReactorCtorDefaultArguments()});
        ${fuseNonEmpty(" |    "..generateModeValidateCall(modeProgram))}
            |    _lf_environment->assemble(_lf_environment);
            |    _lf_environment->start(_lf_environment);
            |    lf_exit();
            |}
        """
          .trimMargin()
    }
  }
}

class UcMainGeneratorFederated(
    private val currentFederate: UcFederate,
    private val otherFederates: List<UcFederate>,
    numEvents: Int,
    numReactions: Int,
    private val fileConfig: UcFileConfig,
) :
    UcMainGenerator(
        currentFederate.inst.eContainer() as Reactor,
        numEvents,
        numReactions,
    ) {
  private val top = currentFederate.inst.eContainer() as Reactor
  private val main = currentFederate.inst.reactor
  private val clockSyncMainReactor = UcClockSyncMainAttribute(top)
  private val ucConnectionGenerator = UcConnectionGenerator(top, currentFederate, otherFederates)
  private val netBundlesSize = ucConnectionGenerator.getNumFederatedConnectionBundles()
  private val clockSyncGenerator =
      UcClockSyncGenerator(currentFederate, ucConnectionGenerator, clockSyncMainReactor)
  private val longestPath = 0

  override fun getNumSystemEvents(): Int {
    val clockSyncSystemEvents = UcClockSyncGenerator.getNumSystemEvents(netBundlesSize)
    val startupCoordinatorEvents = UcStartupCoordinatorGenerator.getNumSystemEvents(netBundlesSize)
    val shutdownCoordinatorEvents =
        UcShutdownCoordinatorGenerator.getNumSystemEvents(netBundlesSize)
    return clockSyncSystemEvents + startupCoordinatorEvents + shutdownCoordinatorEvents
  }

  override fun keepAlive(): Boolean {
    val attr: Attribute? = AttributeUtils.findAttributeByName(top, "keepalive")
    if (attr != null) {
      return attr.getAttrParms().get(0).getValue().equals("true", ignoreCase = true)
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
      "DynamicScheduler_ctor(&_scheduler, _lf_environment, &${eventQueueName}.super, &${systemEventQueueName}.super, &${reactionQueueName}.super, ${getTimeout()}, ${keepAlive()});"

  override fun generateStartSource(): String {
    // The federate, not the whole program: each federate is its own binary with its own registry.
    val modeProgram = generateModeProgram(main, "main_reactor.${currentFederate.inst.name}[0]")
    return with(PrependOperator) {
      """
            |#include "reactor-uc/reactor-uc.h"
        ${" |"..generateIncludeScheduler()}
            |#include "lf_federate.h"
            |static ${currentFederate.codeType} main_reactor;
        ${fuseNonEmpty(" |"..modeProgram)}
            |static FederatedEnvironment lf_environment;
            |Environment *_lf_environment = &lf_environment.super;
        ${" |"..generateDefineQueues()}
        ${" |"..generateDefineScheduler()}
            |void lf_exit(void) {
            |   FederatedEnvironment_free(&lf_environment);
            |}
            |void lf_start(void) {
        ${" |    "..generateAbiGuard()}
        ${" |    "..generateInitializeQueues()}
        ${" |    "..generateInitializeScheduler()}
            |    FederatedEnvironment_ctor(&lf_environment, (Reactor *)&main_reactor, scheduler, ${fast()},
            |                     (FederatedConnectionBundle **) &main_reactor._bundles, ${netBundlesSize}, &main_reactor.${UcStartupCoordinatorGenerator.instName}.super,
            |                     &main_reactor.${UcShutdownCoordinatorGenerator.instName}.super, ${if (clockSyncGenerator.enabled()) "&main_reactor.${UcClockSyncGenerator.instName}.super" else "NULL"});
            |    ${currentFederate.codeType}_ctor(&main_reactor, NULL, _lf_environment);
        ${fuseNonEmpty(" |    "..generateModeValidateCall(modeProgram))}
            |    _lf_environment->assemble(_lf_environment);
            |    _lf_environment->start(_lf_environment);
            |    lf_exit();
            |}
        """
          .trimMargin()
    }
  }
}
