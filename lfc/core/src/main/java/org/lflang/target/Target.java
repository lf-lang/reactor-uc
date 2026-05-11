/* Static information about targets. */
/**
 * Copyright (c) 2019, The University of California at Berkeley. Redistribution and use in source
 * and binary forms, with or without modification, are permitted provided that the following
 * conditions are met: 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer. 2. Redistributions in binary form must
 * reproduce the above copyright notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution. THIS SOFTWARE IS PROVIDED BY
 * THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.lflang.target;

import java.util.*;
import net.jcip.annotations.Immutable;
import org.lflang.lf.TargetDecl;

/**
 * Enumeration of targets and their associated properties.
 *
 * @author Marten Lohstroh
 */
@Immutable
public enum Target {
  UC(
      "uC",
      true,
      Arrays.asList(
          // List via: https://en.cppreference.com/w/c/keyword
          "auto",
          "break",
          "case",
          "char",
          "const",
          "continue",
          "default",
          "do",
          "double",
          "else",
          "enum",
          "extern",
          "float",
          "for",
          "goto",
          "if",
          "inline", // (since C99)
          "int",
          "long",
          "register",
          "restrict", // (since C99)
          "return",
          "short",
          "signed",
          "sizeof",
          "static",
          "struct",
          "switch",
          "typedef",
          "union",
          "unsigned",
          "void",
          "volatile",
          "while",
          "_Alignas", // (since C11)
          "_Alignof", // (since C11)
          "_Atomic", // (since C11)
          "_Bool", // (since C99)
          "_Complex", // (since C99)
          "_Generic", // (since C11)
          "_Imaginary", // (since C99)
          "_Noreturn", // (since C11)
          "_Static_assert", // (since C11)
          "_Thread_local" // (since C11)
          ));

  /** String representation of this target. */
  private final String displayName;

  /** Whether or not this target requires types. */
  public final boolean requiresTypes;

  /** Reserved words in the target language. */
  private final Set<String> keywords;

  /** An unmodifiable list of all known targets. */
  public static final List<Target> ALL = List.of(Target.values());

  /**
   * Private constructor for targets.
   *
   * @param displayName String representation of this target.
   * @param requiresTypes Types Whether this target requires type annotations or not.
   * @param keywords List of reserved strings in the target language.
   */
  Target(String displayName, boolean requiresTypes, Collection<String> keywords) {
    this.displayName = displayName;
    this.requiresTypes = requiresTypes;
    this.keywords = Collections.unmodifiableSet(new LinkedHashSet<>(keywords));
  }

  /**
   * Return the display name of the target, as it should be written in LF code. This is hence a
   * single identifier. Eg for returns {@code "Cpp"}, for returns {@code "Python"}. Avoid using
   * either {@link #name()} or {@link #toString()}, which have unrelated contracts.
   */
  public String getDisplayName() {
    return displayName;
  }

  /**
   * Return the description. Avoid depending on this, toString is supposed to be debug information.
   * Prefer {@link #getDisplayName()}.
   */
  @Override
  public String toString() {
    return displayName;
  }

  /**
   * Given a string and a list of candidate objects, return the first candidate that matches, or
   * null if no candidate matches.
   *
   * <p>todo move to CollectionUtil (introduced in #442)
   *
   * @param string The string to match against candidates.
   * @param candidates The candidates to match the string against.
   */
  public static <T> T match(
      final String string, final Iterable<T> candidates) { // FIXME: use Optional
    // kotlin: candidates.firstOrNull { it.toString().equalsIgnoreCase(string) }
    for (T candidate : candidates) {
      if (candidate.toString().equalsIgnoreCase(string)) {
        return candidate;
      }
    }
    return null;
  }

  /**
   * Given a string and a list of candidate objects, return the first candidate that matches, or
   * null if no candidate matches.
   *
   * <p>todo move to CollectionUtil (introduced in #442)
   *
   * @param string The string to match against candidates.
   * @param candidates The candidates to match the string against.
   */
  public static <T> T match(final String string, final T[] candidates) {
    return match(string, Arrays.asList(candidates));
  }

  /**
   * Returns whether the given identifier is invalid as the name of an LF construct. This usually
   * means that the identifier is a keyword in the target language. In Rust, many keywords may be
   * escaped with the syntax {@code r#keyword}, and they are considered valid identifiers.
   */
  public boolean isReservedIdent(String ident) {
    return this.keywords.contains(ident);
  }

  /**
   * Return the target constant corresponding to given target declaration among. Return a non-null
   * result, will throw if invalid.
   *
   * @throws RuntimeException If no {@link TargetDecl} is present or if it is invalid.
   */
  public static Target fromDecl(TargetDecl targetDecl) {
    String name = targetDecl.getName();
    if (!Objects.equals(name, "uC")) {
      throw new RuntimeException("Invalid target name '" + name + "'");
    } else {
      return Target.UC;
    }
  }

  /** Return a string that demarcates the beginning of a single-line comment. */
  public String getSingleLineCommentPrefix() {
    return "//";
  }
}
