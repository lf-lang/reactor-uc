package org.lflang.generator.uc

import org.lflang.allStateVars
import org.lflang.inferredType
import org.lflang.isInitialized
import org.lflang.lf.Reactor

class UcStateGenerator(private val variant: UcReactorVariant) {
  private val reactor = variant.reactor

  fun generateReactorStructFields() =
      reactor.allStateVars.joinToString(prefix = "// State variables \n", separator = "\n") {
        if (variant.isArray(it.type)) {
          "${variant.arrayElementType(it.type)} ${it.name}[${variant.arrayLength(it.type)}];"
        } else {
          "${variant.cType(it.inferredType)} ${it.name};"
        }
      }

  fun generateInitializeStateVars() =
      reactor.allStateVars
          .filter { it.isInitialized }
          .joinToString(prefix = "// Initialize State variables \n", separator = "\n") {
            if (variant.isArray(it.type)) {
              """|${variant.arrayElementType(it.type)} _${it.name}_init[${variant.arrayLength(it.type)}] = ${it.init.expr.toCCode()};
                   |memcpy(&self->${it.name}, &_${it.name}_init, sizeof(_${it.name}_init));
                """
                  .trimMargin()
            } else {
              "self->${it.name} = ${it.init.expr.toCCode()};"
            }
          }
}
