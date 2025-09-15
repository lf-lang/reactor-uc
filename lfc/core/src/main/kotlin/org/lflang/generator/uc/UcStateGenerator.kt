package org.lflang.generator.uc

import org.lflang.ir.Reactor

class UcStateGenerator(private val reactor: Reactor) {
  fun generateReactorStructFields() =
      reactor.stateVars.joinToString(prefix = "// State variables \n", separator = "\n") {
        if (it.type.isArray) {
          "${it.type.targetCode} ${it.lfName}[${it.type.arrayLength}];"
        } else {
          "${it.type.targetCode} ${it.lfName};"
        }
      }

  fun generateInitializeStateVars() =
      reactor.stateVars
          .filter { it.isInitialized }
          .joinToString(prefix = "// Initialize State variables \n", separator = "\n") {
            if (it.type.isArray) {
              """|${it.type.targetCode} _${it.lfName}_init[${it.type.arrayLength}] = ${it.init};
                   |memcpy(&self->${it.lfName}, &_${it.lfName}_init, sizeof(_${it.lfName}_init));
                """
                  .trimMargin()
            } else {
              "self->${it.lfName} = ${it.init};"
            }
          }
}
