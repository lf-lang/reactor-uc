package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.Expression
import org.lflang.lf.Instantiation
import org.lflang.lf.Parameter
import org.lflang.lf.ParameterReference
import org.lflang.lf.Reactor

class UcParameterGenerator(
    private val variant: UcReactorVariant,
    private val federate: UcFederate? = null
) {
  private val reactor = variant.reactor

  companion object {

    val Parameter.targetType
      get(): String = this.inferredType.CType

    val Parameter.isPresentName
      get(): String = "__${this.name}"
  }

  fun generateReactorStructFields() =
      reactor.allParameters.joinToString(
          prefix = "// Reactor parameters\n", separator = "\n", postfix = "\n") {
            "${variant.cType(it.inferredType)} ${it.name};"
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
      reactor.allParameters.joinToString(separator = "") {
        ", ${variant.cType(it.inferredType)} ${it.name}"
      }

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
          ", ${generateArgument(r.parameters.findLast { p -> p.lhs.name == it.name }!!.rhs.expr)}"
        }
      }

  /**
   * Generate the C code for an argument passed to the constructor of a child reactor. Parameter
   * references refer to parameters of this reactor, which are arguments of the generated
   * constructor. The top-level reactor of a federated program, however, is not generated as a
   * reactor. Each federate is generated as a stand-alone program, so references to parameters of
   * the top-level reactor are replaced by their values.
   */
  private fun generateArgument(expr: Expression): String =
      if (federate != null && expr is ParameterReference && expr.parameter.init?.expr != null) {
        generateArgument(expr.parameter.init.expr)
      } else {
        expr.toCCode()
      }
}
