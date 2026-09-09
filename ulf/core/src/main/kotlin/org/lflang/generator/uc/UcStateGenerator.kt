package org.lflang.generator.uc

import org.lflang.allModes
import org.lflang.allStateVars
import org.lflang.generator.uc.UcPortGenerator.Companion.arrayLength
import org.lflang.generator.uc.UcPortGenerator.Companion.isArray
import org.lflang.inferredType
import org.lflang.isInitialized
import org.lflang.lf.Mode
import org.lflang.lf.Reactor
import org.lflang.lf.StateVar

class UcStateGenerator(private val reactor: Reactor) {

  companion object {

    val StateVar.resetSourceName: String
      get() = "_${name}_reset_source"

    val Mode.resetStateVars: List<StateVar>
      get() = stateVars.filter { it.isReset }

    val Reactor.modeResetStateVars: List<StateVar>
      get() = allModes.flatMap { it.resetStateVars }

    val Reactor.allResetStateVars: List<StateVar>
      get() = allStateVars.filter { it.isReset }
  }

  private fun StateVar.declare(name: String): String =
      if (type.isArray) "${type.id} $name[${type.arrayLength}];"
      else "${inferredType.CType} $name;"

  fun generateReactorStructFields(): String {
    val fields =
        reactor.allStateVars.joinToString(prefix = "// State variables \n", separator = "\n") {
          it.declare(it.name)
        }
    val shadows =
        reactor.allResetStateVars.joinToString(separator = "\n") { it.declare(it.resetSourceName) }
    return if (shadows.isEmpty()) fields else "$fields\n$shadows"
  }

  fun generateInitializeStateVars(): String {
    val init =
        reactor.allStateVars
            .filter { it.isInitialized }
            .joinToString(prefix = "// Initialize State variables \n", separator = "\n") {
              if (it.type.isArray) {
                """|${it.type.id} _${it.name}_init[${it.type.arrayLength}] = ${it.init.expr.toCCode()};
                   |memcpy(&self->${it.name}, &_${it.name}_init, sizeof(_${it.name}_init));
                """
                    .trimMargin()
              } else {
                "self->${it.name} = ${it.init.expr.toCCode()};"
              }
            }

    val snapshot =
        reactor.allResetStateVars.joinToString(separator = "\n") {
          if (it.type.isArray)
              "memcpy(&self->${it.resetSourceName}, &self->${it.name}, sizeof(self->${it.name}));"
          else "self->${it.resetSourceName} = self->${it.name};"
        }
    return if (snapshot.isEmpty()) init else "$init\n$snapshot"
  }
}
