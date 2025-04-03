package org.lflang.generator.uc

import org.lflang.allStateVars
import org.lflang.generator.uc.UcPortGenerator.Companion.arrayLength
import org.lflang.generator.uc.UcPortGenerator.Companion.isArray
import org.lflang.isInitialized
import org.lflang.lf.Reactor
import org.lflang.toText

class UcStateGenerator(private val reactor: Reactor) {
  fun generateReactorStructFields() =
      reactor.allStateVars.joinToString(prefix = "// State variables \n", separator = "\n") {
        if (it.type.isArray) {
          "${it.type.id} ${it.name}[${it.type.arrayLength}];"
        } else {
          "${it.type.toText()} ${it.name};"
        }
      }

  fun generateInitializeStateVars() =
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
}
