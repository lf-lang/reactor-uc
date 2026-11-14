package org.lflang.generator.uc

import org.eclipse.emf.ecore.EObject
import org.lflang.MessageReporter
import org.lflang.allActions
import org.lflang.allConnections
import org.lflang.allInputs
import org.lflang.allInstantiations
import org.lflang.allOutputs
import org.lflang.allParameters
import org.lflang.allStateVars
import org.lflang.ast.ASTUtils
import org.lflang.lf.CodeExpr
import org.lflang.lf.Expression
import org.lflang.lf.Instantiation
import org.lflang.lf.Literal
import org.lflang.lf.Parameter
import org.lflang.lf.ParameterReference
import org.lflang.lf.Port
import org.lflang.lf.Reactor
import org.lflang.lf.Type
import org.lflang.lf.WidthSpec
import org.lflang.reactor

/**
 * Discovers all reactor variants (see [UcReactorVariant]) reachable from a top-level reactor by
 * walking the instantiation tree, resolving const-generic parameters (parameters that determine
 * multiport widths, bank widths or array lengths) and type parameters along the way.
 *
 * All widths of ports and banks, and all types that depend on type parameters or const-generic
 * parameters, are resolved and cached in the variants, so the code generators can look them up
 * without needing to know about the instantiation hierarchy. Problems (e.g. a width depending on a
 * value that is not known at compile time) are reported to [messageReporter].
 */
class UcVariantAnalysis(top: Reactor, private val messageReporter: MessageReporter) {

  private val variantsByKey = LinkedHashMap<List<Any>, UcReactorVariant>()
  private val usedCodeNames = HashSet<Pair<Reactor, String>>()
  private val constParamCache = HashMap<Reactor, Set<Parameter>>()

  /** The variant of the top-level reactor. */
  val top: UcReactorVariant

  /** All variants, in the order in which they were discovered. */
  val allVariants: Collection<UcReactorVariant>
    get() = variantsByKey.values

  init {
    if (top.typeParms.isNotEmpty()) {
      error(top, "The top-level reactor ${top.name} cannot have type parameters.")
    }
    val constArgs = LinkedHashMap<Parameter, Int>()
    for (param in constParameters(top)) {
      constArgs[param] = evaluate(param.init?.expr, null, param, top) ?: 1
    }
    this.top = getOrCreate(top, constArgs, emptyMap(), isTop = true)
  }

  /** Returns all variants reachable from (and including) [variant]. */
  fun reachableFrom(variant: UcReactorVariant): List<UcReactorVariant> {
    val res = LinkedHashSet<UcReactorVariant>()
    fun visit(v: UcReactorVariant) {
      if (res.add(v)) v.children.values.forEach { visit(it) }
    }
    visit(variant)
    return res.toList()
  }

  /** Returns all variants instantiated below (excluding) [variant]. */
  fun instantiatedBelow(variant: UcReactorVariant): List<UcReactorVariant> {
    val res = LinkedHashSet<UcReactorVariant>()
    fun visit(v: UcReactorVariant) {
      v.children.values.forEach { if (res.add(it)) visit(it) }
    }
    visit(variant)
    return res.toList()
  }

  private fun error(obj: EObject, msg: String) {
    messageReporter.at(obj).error(msg)
  }

  /**
   * Returns the const-generic parameters of [reactor]: the parameters that are used (directly or
   * through parameters of child reactors) as multiport widths, bank widths or array lengths.
   */
  fun constParameters(reactor: Reactor): Set<Parameter> =
      constParamCache.getOrPut(reactor) {
        val params = reactor.allParameters
        val res = LinkedHashSet<Parameter>()
        fun addRefsIn(spec: WidthSpec?) {
          spec?.terms?.forEach { term -> term.parameter?.let { if (it in params) res.add(it) } }
        }
        fun addRefsIn(type: Type?) {
          if (type == null) return
          type.cStyleArraySpec?.lengthParam?.let { if (it in params) res.add(it) }
          type.typeArgs?.forEach { addRefsIn(it) }
        }
        reactor.allInputs.forEach { addRefsIn(it.widthSpec); addRefsIn(it.type) }
        reactor.allOutputs.forEach { addRefsIn(it.widthSpec); addRefsIn(it.type) }
        reactor.allActions.forEach { addRefsIn(it.type) }
        reactor.allStateVars.forEach { addRefsIn(it.type) }
        for (inst in reactor.allInstantiations) {
          addRefsIn(inst.widthSpec)
          inst.typeArgs.forEach { addRefsIn(it) }
          val childConst = constParameters(inst.reactor)
          for (assignment in inst.parameters) {
            if (assignment.lhs in childConst) {
              val expr = assignment.rhs?.expr
              if (expr is ParameterReference && expr.parameter in params) res.add(expr.parameter)
            }
          }
        }
        // Keep the declaration order to get deterministic names.
        params.filter { it in res }.toSet()
      }

  /**
   * Evaluate [expr], which is assigned to the const-generic parameter [param], to an integer.
   * Parameter references are resolved in [context], the variant in which the expression appears.
   */
  private fun evaluate(
      expr: Expression?,
      context: UcReactorVariant?,
      param: Parameter,
      errorObj: EObject
  ): Int? {
    val value: Int? =
        when (expr) {
          null -> null
          is Literal -> parseInt(expr.literal)
          is CodeExpr -> parseInt(expr.code.body)
          is ParameterReference -> {
            val v = context?.constArgs?.get(expr.parameter)
            if (v == null) {
              error(
                  errorObj,
                  "The parameter ${param.name} determines a multiport width, bank width or array" +
                      " length and must be known at compile time, but it is assigned the parameter" +
                      " ${expr.parameter.name}, whose value is not known at compile time.")
              return null
            }
            v
          }
          else -> null
        }
    if (value == null) {
      error(
          errorObj,
          "The parameter ${param.name} determines a multiport width, bank width or array length" +
              " and must be assigned an integer literal (or a parameter that is one), but got" +
              " '${expr?.let { ASTUtils.toOriginalText(it) } ?: "nothing"}'.")
    }
    return value
  }

  private fun parseInt(text: String?): Int? {
    val t = text?.trim() ?: return null
    return try {
      Integer.decode(t)
    } catch (e: NumberFormatException) {
      null
    }
  }

  private fun getOrCreate(
      reactor: Reactor,
      constArgs: Map<Parameter, Int>,
      typeArgs: Map<String, Type>,
      isTop: Boolean = false,
  ): UcReactorVariant {
    val key: List<Any> =
        listOf(
            reactor,
            constArgs.map { Pair(it.key.name, it.value) },
            typeArgs.map { Pair(it.key, ASTUtils.toOriginalText(it.value)) })
    variantsByKey[key]?.let {
      return it
    }

    val variant = UcReactorVariant(reactor, constArgs, typeArgs)
    // There is only one variant of the top-level reactor, so it keeps the plain reactor name.
    variant.codeName = if (isTop) reactor.name.also { usedCodeNames.add(Pair(reactor, it)) } else uniqueCodeName(variant)
    variantsByKey[key] = variant

    resolveTypes(variant)
    for (inst in reactor.allInstantiations) {
      variant.children[inst] = createChild(variant, inst)
    }
    resolveWidths(variant)
    checkConnectionTypes(variant)
    return variant
  }

  /**
   * Check that ports connected within [variant] have the same type after resolving type parameters
   * and const-generic array lengths, e.g. that `int[N]` is not connected to `int[M]` with N != M.
   */
  private fun checkConnectionTypes(variant: UcReactorVariant) {
    for (conn in variant.reactor.allConnections) {
      val refs = conn.leftPorts + conn.rightPorts
      val types =
          refs.mapNotNull { ref ->
            val port = ref.variable as? Port ?: return@mapNotNull null
            val type = port.type ?: return@mapNotNull null
            val owner = if (ref.container != null) variant.children[ref.container] else variant
            val resolved = owner?.resolveType(type) ?: type
            Triple(ref, type, resolved)
          }
      // Only check connections whose types depend on the variant. Other type mismatches are left
      // to the C compiler, as before.
      if (types.none { it.second !== it.third }) continue
      val texts = types.map { ASTUtils.toOriginalText(it.third) }
      if (texts.distinct().size > 1) {
        error(
            conn,
            "Connected ports must have the same type, but in ${variant.codeName} the types are " +
                types.zip(texts).joinToString(", ") { (t, text) ->
                  "${ASTUtils.toOriginalText(t.first)}: $text"
                } +
                ".")
      }
    }
  }

  /**
   * Whether [variant] is the default variant of its reactor class: it has no type parameters and
   * all its const-generic parameters have their default values. The default variant keeps the
   * plain reactor name, so programs that don't override any width parameters generate the same
   * code as without const generics.
   */
  private fun isDefaultVariant(variant: UcReactorVariant): Boolean =
      variant.typeArgs.isEmpty() &&
          variant.constArgs.all { (param, value) ->
            val default =
                when (val expr = param.init?.expr) {
                  is Literal -> parseInt(expr.literal)
                  is CodeExpr -> parseInt(expr.code.body)
                  else -> null
                }
            default == value
          }

  private fun uniqueCodeName(variant: UcReactorVariant): String {
    val base =
        if (isDefaultVariant(variant)) variant.reactor.name
        else
            (listOf(variant.reactor.name) +
                    variant.constArgs.map { "${it.key.name}_${it.value}" } +
                    variant.typeArgs.map {
                      "${it.key}_${UcReactorVariant.mangle(ASTUtils.toOriginalText(it.value))}"
                    })
                .joinToString("__")
    var name = base
    var idx = 1
    // Different variants could be mangled to the same name, so make the names unique.
    while (Pair(variant.reactor, name) in usedCodeNames) {
      name = "${base}_${idx++}"
    }
    usedCodeNames.add(Pair(variant.reactor, name))
    return name
  }

  /** Compute the variant of the child reactor instantiated by [inst] within [parent]. */
  private fun createChild(parent: UcReactorVariant, inst: Instantiation): UcReactorVariant {
    val child = inst.reactor
    val constArgs = LinkedHashMap<Parameter, Int>()
    for (param in constParameters(child)) {
      if (param.name == "bank_index" || param.name == "bank_idx") {
        error(
            inst,
            "The parameter ${param.name} of ${child.name} cannot be used as a multiport width, bank" +
                " width or array length, since it differs between the reactors of a bank.")
        constArgs[param] = 1
        continue
      }
      val assignment = inst.parameters.lastOrNull { it.lhs == param }
      constArgs[param] =
          if (assignment != null) evaluate(assignment.rhs?.expr, parent, param, assignment) ?: 1
          else evaluate(param.init?.expr, null, param, inst) ?: 1
    }

    val typeArgs = LinkedHashMap<String, Type>()
    val typeParams = child.typeParms
    if (typeParams.isNotEmpty() || inst.typeArgs.isNotEmpty()) {
      if (typeParams.size != inst.typeArgs.size) {
        error(
            inst,
            "${child.name} has ${typeParams.size} type parameter(s), but ${inst.typeArgs.size}" +
                " type argument(s) were given.")
      }
      for ((idx, typeParam) in typeParams.withIndex()) {
        val name = typeParam.literal ?: typeParam.code?.body?.trim()
        if (name == null) {
          error(typeParam, "Unsupported type parameter.")
          continue
        }
        val arg = inst.typeArgs.getOrNull(idx) ?: continue
        val resolved = parent.resolveType(arg)
        if (resolved.typeArgs.isNotEmpty()) {
          error(arg, "Type arguments of types (e.g. `Foo<int>`) are not supported by the uC target.")
        }
        typeArgs[name.trim()] = resolved
      }
    }
    return getOrCreate(child, constArgs, typeArgs)
  }

  /** Resolve all types appearing in the reactor of [variant]. */
  private fun resolveTypes(variant: UcReactorVariant) {
    if (!variant.hasBindings) return
    val reactor = variant.reactor
    val types = mutableListOf<Type>()
    reactor.allInputs.mapNotNullTo(types) { it.type }
    reactor.allOutputs.mapNotNullTo(types) { it.type }
    reactor.allActions.mapNotNullTo(types) { it.type }
    reactor.allStateVars.mapNotNullTo(types) { it.type }
    reactor.allParameters.mapNotNullTo(types) { it.type }
    reactor.allInstantiations.forEach { types.addAll(it.typeArgs) }
    for (type in types) {
      val resolved = resolveType(variant, type)
      if (resolved !== type) variant.resolvedTypes[type] = resolved
    }
  }

  private fun resolveType(variant: UcReactorVariant, type: Type): Type {
    var res = type
    val typeParamName = type.id ?: type.code?.body?.trim()
    val arg = typeParamName?.let { variant.typeArgs[it] }
    if (arg != null) {
      res = UcReactorVariant.copy(arg)
      res.stars.addAll(type.stars)
      if (type.cStyleArraySpec != null) {
        if (res.cStyleArraySpec != null) {
          error(
              type,
              "The type parameter $typeParamName is bound to the array type" +
                  " ${ASTUtils.toOriginalText(arg)} and cannot be used as an array element type.")
        } else {
          res.cStyleArraySpec = UcReactorVariant.copy(type).cStyleArraySpec
        }
      }
    }
    val lengthParam = res.cStyleArraySpec?.lengthParam
    if (lengthParam != null) {
      if (res === type) res = UcReactorVariant.copy(type)
      val length = variant.constArgs[lengthParam]
      if (length == null || length <= 0) {
        error(
            type,
            "The array length ${lengthParam.name} must be a positive integer known at compile time.")
      }
      res.cStyleArraySpec.lengthParam = null
      res.cStyleArraySpec.length = length ?: 1
    }
    return res
  }

  /** Compute the widths of all ports and banks in the reactor of [variant]. */
  private fun resolveWidths(variant: UcReactorVariant) {
    val reactor = variant.reactor
    for (port in reactor.allInputs + reactor.allOutputs) {
      variant.portWidths[port as Port] = evaluateWidth(port.widthSpec, variant, port)
    }
    for (inst in reactor.allInstantiations) {
      variant.bankWidths[inst] = evaluateWidth(inst.widthSpec, variant, inst)
    }
  }

  private fun evaluateWidth(spec: WidthSpec?, variant: UcReactorVariant, obj: EObject): Int {
    if (spec == null) return 1
    if (spec.isOfVariableLength) {
      error(obj, "Variable-length multiports and banks (`[]`) are not supported by the uC target.")
      return 1
    }
    var width = 0
    for (term in spec.terms) {
      width +=
          when {
            term.parameter != null -> {
              val v = variant.constArgs[term.parameter]
              if (v == null) {
                error(
                    obj,
                    "The width depends on the parameter ${term.parameter.name}, whose value is not" +
                        " known at compile time.")
                1
              } else v
            }
            term.port != null -> {
              val ref = term.port
              val port = ref.variable as? Port
              if (port == null) {
                error(obj, "widthof() must refer to a port.")
                1
              } else if (ref.container != null) {
                val child = variant.children[ref.container]
                if (child == null) {
                  error(obj, "Cannot resolve the width of ${ref.container.name}.${port.name}.")
                  1
                } else {
                  child.width(port) * (variant.bankWidths[ref.container] ?: 1)
                }
              } else {
                variant.portWidths[port]
                    ?: evaluateWidth(port.widthSpec, variant, port).also {
                      variant.portWidths[port] = it
                    }
              }
            }
            term.code != null -> {
              val v = parseInt(term.code.body)
              if (v == null) {
                error(
                    obj,
                    "Widths given as target code are not supported by the uC target, because they" +
                        " must be known at compile time.")
                1
              } else v
            }
            else -> term.width
          }
    }
    if (width < 0) {
      error(obj, "Widths must not be negative, but got $width.")
      return 1
    }
    return width
  }
}
