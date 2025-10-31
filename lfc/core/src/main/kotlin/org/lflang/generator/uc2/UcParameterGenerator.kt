package org.lflang.generator.uc2

import org.lflang.ir.Parameter
import org.lflang.ir.Reactor
import org.lflang.ir.ReactorInstantiation

class UcParameterGenerator(private val reactor: Reactor) {

  companion object {

    val Parameter.targetType
      get(): String = this.type.targetCode.code

    val Parameter.isPresentName
      get(): String = "__${this.name}"
  }

  fun generateReactorStructFields() =
      reactor.instantiation.allParameters.joinToString(
          prefix = "// Reactor parameters\n", separator = "\n", postfix = "\n") {
            "${it.targetType} ${it.name};"
          }

  fun generateReactorCtorCodes() =
      reactor.instantiation.allParameters.joinToString(
          prefix = "// Initialize Reactor parameters\n", separator = "\n", postfix = "\n") {
            """|
                    |self->${it.name} = ${it.name};
                """
                .trimMargin()
          }

  fun generateReactorCtorDefArguments() =
      reactor.instantiation.allParameters.joinToString(separator = "") { ", ${it.targetType} ${it.name}" }

  fun generateReactorCtorDefaultArguments() =
      reactor.instantiation.allParameters.joinToString(separator = "") { ", ${it.init.code}" }

  fun generateReactorCtorDeclArguments(r: ReactorInstantiation) =
      r.reactor.instantiation.allParameters.joinToString(separator = "") {
        if (it.name == "bank_idx" || it.name == "bank_index") {
            ", i"
        } else if (r.reactor.instantiation.allParameters.none { p -> p.name == it.name }) {
          ", ${it.init.code}"
        } else {
          ", ${r.allParameters.find{ p -> p.name == it.name}!!.value.code}"
        }
      }
}
