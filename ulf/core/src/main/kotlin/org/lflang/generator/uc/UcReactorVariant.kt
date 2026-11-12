package org.lflang.generator.uc

import org.eclipse.emf.ecore.util.EcoreUtil
import org.lflang.InferredType
import org.lflang.ast.ASTUtils
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.lf.Instantiation
import org.lflang.lf.Parameter
import org.lflang.lf.Port
import org.lflang.lf.Reactor
import org.lflang.lf.Type
import org.lflang.toText

/**
 * A reactor variant is a reactor class together with concrete values for all its compile-time
 * ("const-generic") parameters and concrete types for all its type parameters.
 *
 * reactor-uc allocates all memory statically inside the self structs, so the widths of multiports
 * and banks, and the lengths of array types, must be known when the struct of a reactor is
 * generated. If these depend on parameters of the reactor, then each distinct set of values for
 * these parameters needs its own struct definition. Likewise, C has no generics, so each distinct
 * set of type arguments needs its own struct definition. The code generator therefore generates one
 * set of files (header and source) per reachable variant, i.e. it monomorphizes the program.
 *
 * Only parameters that influence the memory layout (see [UcVariantAnalysis.constParameters]) are
 * part of the variant. All other parameters remain normal runtime constructor arguments, so
 * instances that only differ in such parameters share the same code.
 *
 * A variant without any bindings is code-generated exactly as a reactor class without generics, so
 * programs that use neither const nor type generics are unaffected.
 *
 * Variants are created and populated by [UcVariantAnalysis].
 */
class UcReactorVariant
internal constructor(
    val reactor: Reactor,
    /** Values of the const-generic parameters of the reactor, in declaration order. */
    val constArgs: Map<Parameter, Int>,
    /** Concrete types bound to the type parameters of the reactor, in declaration order. */
    val typeArgs: Map<String, Type>,
) {
  /** The name of this variant, used for its C types and generated files. */
  lateinit var codeName: String
    internal set

  val codeType
    get(): String = "Reactor_$codeName"

  val includeGuard
    get(): String = "LFC_GEN_${codeName.uppercase()}_H"

  val hasBindings
    get(): Boolean = constArgs.isNotEmpty() || typeArgs.isNotEmpty()

  internal val children = LinkedHashMap<Instantiation, UcReactorVariant>()
  internal val portWidths = HashMap<Port, Int>()
  internal val bankWidths = HashMap<Instantiation, Int>()
  internal val resolvedTypes = HashMap<Type, Type>()

  /** The variant of the reactor instantiated by [inst], which must be a child of this variant. */
  fun child(inst: Instantiation): UcReactorVariant =
      children[inst]
          ?: throw IllegalStateException(
              "No variant of ${inst.name} in $codeName. The variant analysis did not visit it.")

  /** The width of a port of this reactor. */
  fun width(port: Port): Int = portWidths[port] ?: 1

  /** The width of the bank created by [inst], a child instantiation of this reactor. */
  fun width(inst: Instantiation): Int = bankWidths[inst] ?: 1

  /**
   * The bank width used in the generated code. A federate is generated as a single instance, even
   * if it is part of a bank of federates.
   */
  fun codeWidth(inst: Instantiation): Int = if (inst.isAFederate) 1 else width(inst)

  /** The width of [port] of the child reactor instantiated by [inst]. */
  fun childPortWidth(inst: Instantiation, port: Port): Int = child(inst).width(port)

  /**
   * Resolve the type parameters and const-generic array lengths in [type], which must appear in
   * this reactor. Returns [type] itself if it does not depend on the variant.
   */
  fun resolveType(type: Type): Type = resolvedTypes[type] ?: type

  fun resolveType(type: InferredType): InferredType =
      if (type.astType == null || resolveType(type.astType) === type.astType) type
      else InferredType.fromAST(resolveType(type.astType))

  /** The C type of [type] (without any array specification). */
  fun cType(type: Type): String = resolveType(type).toText()

  fun cType(type: InferredType): String = resolveType(type).CType

  /** Whether [type] is a C-style array type, e.g. `int[4]` or `int[N]`. */
  fun isArray(type: Type?): Boolean = type != null && resolveType(type).cStyleArraySpec != null

  /** The element type of the array type [type]. */
  fun arrayElementType(type: Type): String = ASTUtils.baseType(resolveType(type))

  /** The length of the array type [type]. */
  fun arrayLength(type: Type): Int = resolveType(type).cStyleArraySpec.length

  /** Whether [type] is `void` (or absent). */
  fun isVoid(type: Type?): Boolean = type == null || resolveType(type).id == "void"

  override fun toString(): String = codeName

  companion object {
    /** Turn the textual representation of a type into a valid part of a C identifier. */
    internal fun mangle(text: String): String {
      val sb = StringBuilder()
      for (c in text.trim()) {
        when {
          c.isLetterOrDigit() || c == '_' -> sb.append(c)
          c == '*' -> sb.append("ptr")
          c.isWhitespace() -> if (sb.isNotEmpty() && sb.last() != '_') sb.append('_')
          c == '[' -> sb.append("_arr")
          c == ']' -> {}
          else -> sb.append("_x${c.code.toString(16)}")
        }
      }
      return sb.toString()
    }

    internal fun copy(type: Type): Type = EcoreUtil.copy(type)

    /** The C code for a (resolved) type argument, as used in the `#define` of a type parameter. */
    fun typeArgCode(type: Type): String =
        if (type.cStyleArraySpec != null) ASTUtils.toOriginalText(type)
        else UcTypes.getTargetType(InferredType.fromAST(type))
  }
}
