package org.lflang.generator.uc

import org.lflang.generator.uc.mics.CType
import org.lflang.generator.uc.mics.name
import org.lflang.generator.uc.mics.toCCode
import org.lflang.ir.Reactor

class UcParameterGenerator(private val reactor: Reactor, private val federate: UcFederate? = null) {

  companion object {

    val Parameter.targetType
      get(): String = this.inferredType.CType

    val Parameter.isPresentName
      get(): String = "__${this.name}"
  }

  fun generateReactorStructFields() =
      reactor.allParameters.joinToString(
          prefix = "// Reactor parameters\n", separator = "\n", postfix = "\n") {
            "${it.inferredType.CType} ${it.name};"
          }

  fun generateReactorCtorCodes() =
      reactor.allParameters.joinToString(
          prefix = "// Initialize Reactor parameters\n", separator = "\n", postfix = "\n") {
            """|
                    |self->${it.name} = ${it.name};
                """
                .trimMargin()
          }

  fun generateReactorCtorDefArguments() =
      reactor.allParameters.joinToString(separator = "") { ", ${it.inferredType.CType} ${it.name}" }

  fun generateReactorCtorDefaultArguments() =
      reactor.allParameters.joinToString(separator = "") { ", ${it.init.expr.toCCode()}" }

  fun generateReactorCtorDeclArguments(r: Instantiation) =
      r.reactor.allParameters.joinToString(separator = "") {
        if (it.name == "bank_idx" || it.name == "bank_index") {
          if (federate != null) {
            ", ${federate.bankIdx}"
          } else {
            ", i"
          }
        } else if (r.parameters.filter { p -> p.lhs.name == it.name }.isEmpty()) {
          ", ${it.init.expr.toCCode()}"
        } else {
          ", ${r.parameters.find{ p -> p.lhs.name == it.name}!!.rhs.expr.toCCode()}"
        }
      }
}
