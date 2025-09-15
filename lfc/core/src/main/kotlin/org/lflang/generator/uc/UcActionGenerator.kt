package org.lflang.generator.uc

import org.lflang.generator.PrependOperator
import org.lflang.ir.Reactor
import org.lflang.ir.Action
import org.lflang.ir.Shutdown
import org.lflang.ir.Startup

class UcActionGenerator(private val reactor: Reactor) {
  private fun generateSelfStruct(action: Action): String {
      return if (action.type.isVoid) {
          "LF_DEFINE_ACTION_STRUCT_VOID(${reactor.codeType}, ${action.lfName}, ${action.actionType}, ${action.getEffects().size}, ${action.getSources().size}, ${action.getObservers().size}, ${action.maxNumPendingEvents});"
      } else if (action.type.isArray) {
          "LF_DEFINE_ACTION_STRUCT_ARRAY(${reactor.codeType}, ${action.lfName}, ${action.actionType}, ${action.getEffects().size}, ${action.getSources().size}, ${action.getObservers().size}, ${action.maxNumPendingEvents}, ${action.type.targetCode}, ${action.type.arrayLength});"
      } else {
          "LF_DEFINE_ACTION_STRUCT(${reactor.codeType}, ${action.lfName}, ${action.actionType}, ${action.getEffects().size}, ${action.getSources().size}, ${action.getObservers().size}, ${action.maxNumPendingEvents}, ${action.type.targetCode});"
      }
  }

  private fun generateCtor(action: Action) =
      with(PrependOperator) {
        """
            |LF_DEFINE_ACTION_CTOR${if (action.type.isVoid) "_VOID" else ""}(${reactor.codeType}, ${action.lfName}, ${action.actionType}, ${action.getEffects().size}, ${action.getSources().size}, ${action.getObservers().size}, ${action.maxNumPendingEvents} ${if (action.type.isVoid) ", ${action.type.targetCode}" else ""});
            |
        """
            .trimMargin()
      }


  fun generateCtors(): String =
      reactor.actions.joinToString(separator = "\n") { generateCtor(it) }

  fun generateSelfStructs(): String =
      reactor.actions.joinToString(separator = "\n") { generateSelfStruct(it) }


  fun generateReactorStructFields(): String =
      reactor.actions.joinToString(
            prefix = "// Actions and builtin triggers\n", separator = "\n", postfix = "\n") {
              "LF_ACTION_INSTANCE(${reactor.codeType}, ${it.lfName});"
            }

  private fun generateReactorCtorCode(action: Action) =
      "LF_INITIALIZE_ACTION(${reactor.codeType}, ${action.lfName}, ${action.minDelay.toCCode()}, ${action.minSpacing.toCCode()});"


  fun generateReactorCtorCodes(): String =
      reactor.actions.joinToString(
            prefix = "// Initialize actions and builtin triggers\n",
            separator = "\n",
            postfix = "\n") {
              generateReactorCtorCode(it)
            }
}

class BuiltInGenerator(private val reactor: Reactor) {
    private fun generateShutdownCtor() =
        "LF_DEFINE_SHUTDOWN_CTOR(${reactor.codeType});\n"

    private fun generateStartUpCtor() =
        "LF_DEFINE_STARTUP_CTOR(${reactor.codeType});\n"

    private fun generateShutdownSelfStruct(trigger: Shutdown) =
        "LF_DEFINE_SHUTDOWN_STRUCT(${reactor.codeType}, ${trigger.getEffects().size}, ${trigger.getObservers().size});\n"

    private fun generateStartupSelfStruct(trigger: Startup) =
        "LF_DEFINE_STARTUP_STRUCT(${reactor.codeType}, ${trigger.getEffects().size}, ${trigger.getObservers().size});\n"


    private fun generateReactorCtorCodeStartup() = "LF_INITIALIZE_STARTUP(${reactor.codeType});\n"
    private fun generateReactorCtorCodeShutdown() = "LF_INITIALIZE_SHUTDOWN(${reactor.codeType});\n"

    fun generateCtors() : String {
        var code = String()
        if (reactor.hasStartup) code += generateStartUpCtor()
        if (reactor.hasShutdown) code += generateShutdownCtor()
        return code
    }

    fun generateSelfStructs(): String {
        var code = String()
        if (reactor.hasStartup) code += generateStartupSelfStruct(reactor.startup!!)
        if (reactor.hasShutdown) code += generateShutdownSelfStruct(reactor.shutdown!!)
        return code
    }

    fun generateReactorCtorCodes(): String {
        var code = String()
        if (reactor.hasStartup) code += generateReactorCtorCodeStartup()
        if (reactor.hasShutdown) code += generateReactorCtorCodeShutdown()
        return code
    }
}
