package org.lflang.generator.uc

import org.lflang.allStateVars
import org.lflang.generator.uc.UcPortGenerator.Companion.arrayLength
import org.lflang.generator.uc.UcPortGenerator.Companion.isArray
import org.lflang.inferredType
import org.lflang.isInitialized
import org.lflang.lf.Mode
import org.lflang.lf.Reactor
import org.lflang.lf.StateVar

/**
 * Emits a reactor's state variables. A `reset state` variable gets a second field beside it holding
 * the value it was initialized with, which is what a mode's reset entry restores from.
 */
class UcStateGenerator(private val reactor: Reactor) {

  companion object {
    /** Field holding the value a `reset state` variable is restored to. */
    val StateVar.resetSourceName: String
      get() = "_${name}_reset_source"

    val Mode.resetStateVars: List<StateVar>
      get() = stateVars.filter { it.isReset }

    val Reactor.allResetStateVars: List<StateVar>
      get() = allStateVars.filter { it.isReset }
  }

  /** A declaration of this variable's type under [fieldName]. */
  private fun StateVar.declaration(fieldName: String): String =
      if (type.isArray) "${type.id} $fieldName[${type.arrayLength}];"
      else "${inferredType.CType} $fieldName;"

  fun generateReactorStructFields(): String {
    val vars =
        reactor.allStateVars.joinToString(prefix = "// State variables \n", separator = "\n") {
          it.declaration(it.name)
        }
    val resetSources =
        reactor.allResetStateVars.joinToString(separator = "\n") {
          it.declaration(it.resetSourceName)
        }
    return fuseNonEmpty(vars, resetSources)
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
    // Taken after initialization, so the reset source holds the declared initial value.
    val snapshot =
        reactor.allResetStateVars.joinToString(separator = "\n") {
          if (it.type.isArray)
              "memcpy(&self->${it.resetSourceName}, &self->${it.name}, sizeof(self->${it.name}));"
          else "self->${it.resetSourceName} = self->${it.name};"
        }
    return fuseNonEmpty(init, snapshot)
  }
}
