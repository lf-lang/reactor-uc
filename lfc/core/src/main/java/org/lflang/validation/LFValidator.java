/* Validation checks for Lingua Franca code. */

/*************
 * Copyright (c) 2019-2020, The University of California at Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************/

package org.lflang.validation;

import com.google.inject.Inject;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;
import org.eclipse.emf.common.util.EList;
import org.eclipse.emf.common.util.TreeIterator;
import org.eclipse.emf.ecore.EAttribute;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.emf.ecore.EStructuralFeature;
import org.eclipse.xtext.nodemodel.util.NodeModelUtils;
import org.eclipse.xtext.validation.Check;
import org.eclipse.xtext.validation.CheckType;
import org.eclipse.xtext.validation.ValidationMessageAcceptor;
import org.lflang.AttributeUtils;
import org.lflang.InferredType;
import org.lflang.ModelInfo;
import org.lflang.ast.ASTUtils;
import org.lflang.lf.Attribute;
import org.lflang.lf.BracedListExpression;
import org.lflang.lf.BuiltinTrigger;
import org.lflang.lf.BuiltinTriggerRef;
import org.lflang.lf.CodeExpr;
import org.lflang.lf.Connection;
import org.lflang.lf.Expression;
import org.lflang.lf.ImportedReactor;
import org.lflang.lf.Initializer;
import org.lflang.lf.Instantiation;
import org.lflang.lf.LfPackage.Literals;
import org.lflang.lf.Literal;
import org.lflang.lf.Mode;
import org.lflang.lf.ModeTransition;
import org.lflang.lf.ParameterReference;
import org.lflang.lf.Reaction;
import org.lflang.lf.Reactor;
import org.lflang.lf.ReactorDecl;
import org.lflang.lf.StateVar;
import org.lflang.lf.Time;
import org.lflang.lf.Timer;
import org.lflang.lf.Type;
import org.lflang.lf.TypedVariable;
import org.lflang.lf.Variable;
import org.lflang.target.Target;
import org.lflang.util.FileUtil;

/**
 * Custom validation checks for Lingua Franca programs.
 *
 * <p>Also see: <a
 * href="https://www.eclipse.org/Xtext/documentation/303_runtime_concepts.html#validation">...</a>
 *
 * @author Edward A. Lee
 * @author Marten Lohstroh
 * @author Matt Weber
 * @author Christian Menard
 * @author Hou Seng Wong
 * @author Clément Fournier
 */
public class LFValidator extends BaseLFValidator {

  // The methods annotated with @Check are automatically invoked on AST nodes matching the types of
  // their arguments. CheckType.FAST ensures that these checks run whenever a file is modified;
  // when CheckType.NORMAL is used, the check is run upon saving.
  // NOTE: please list methods in alphabetical order, and follow a naming convention checkClass,
  // where Class is the AST class.

  public void checkReactorName(String name) throws IOException {
    // Check for illegal names.
    checkName(name, Literals.REACTOR_DECL__NAME);

    // C++ reactors may not be called 'preamble'
  }

  @Check(CheckType.FAST)
  public void checkReactor(Reactor reactor) throws IOException {
    String fileName = FileUtil.nameWithoutExtension(reactor.eResource());

    if (reactor.isFederated() || reactor.isMain()) {
      // Do not allow multiple main/federated reactors.
      TreeIterator<EObject> iter = reactor.eResource().getAllContents();
      int nMain = countMainOrFederated(iter);
      if (nMain > 1) {
        EAttribute attribute = Literals.REACTOR__MAIN;
        if (reactor.isFederated()) {
          attribute = Literals.REACTOR__FEDERATED;
        }
        error("Multiple definitions of main or federated reactor.", attribute);
      }

      if (reactor.getName() != null && !reactor.getName().equals(fileName)) {
        // Make sure that if the name is given, it matches the expected name.
        error(
            "Name of main reactor must match the file name (or be omitted).",
            Literals.REACTOR_DECL__NAME);
      }

      // check the reactor name indicated by the file name
      // Skip this check if the file is named __synthetic0. This Name is used during testing,
      // and we would get an unexpected error due to the '__' prefix otherwise.
      if (!fileName.equals("__synthetic0")) {
        checkReactorName(fileName);
      }
    } else {
      // Not federated or main.
      if (reactor.getName() == null) {
        error("Reactor must be named.", Literals.REACTOR_DECL__NAME);
      } else {
        checkReactorName(reactor.getName());

        TreeIterator<EObject> iter = reactor.eResource().getAllContents();
        int nMain = countMainOrFederated(iter);
        if (nMain > 0 && reactor.getName().equals(fileName)) {
          error("Name conflict with main reactor.", Literals.REACTOR_DECL__NAME);
        }
      }
    }

    Set<Reactor> superClasses = ASTUtils.superClasses(reactor);
    if (superClasses == null) {
      error(
          "Problem with superclasses: Either they form a cycle or are not defined",
          Literals.REACTOR_DECL__NAME);
      // Continue checks, but without any superclasses.
      superClasses = new LinkedHashSet<>();
    }

    if (reactor.getHost() != null) {
      if (!reactor.isFederated()) {
        error(
            "Cannot assign a host to reactor '"
                + reactor.getName()
                + "' because it is not federated.",
            Literals.REACTOR__HOST);
      }
    }

    List<Variable> variables = new ArrayList<>();
    variables.addAll(reactor.getInputs());
    variables.addAll(reactor.getOutputs());
    variables.addAll(reactor.getActions());
    variables.addAll(reactor.getTimers());

    error(
        "The reactor-uc target does not support reactor inheritance.",
        Literals.REACTOR__SUPER_CLASSES);
    // if (!reactor.getSuperClasses().isEmpty() && !target.supportsInheritance()) {
    // }

    // Perform checks on super classes.
    for (Reactor superClass : superClasses) {
      HashSet<Variable> conflicts = new HashSet<>();

      // Detect input conflicts
      checkConflict(superClass.getInputs(), reactor.getInputs(), variables, conflicts);
      // Detect output conflicts
      checkConflict(superClass.getOutputs(), reactor.getOutputs(), variables, conflicts);
      // Detect output conflicts
      checkConflict(superClass.getActions(), reactor.getActions(), variables, conflicts);
      // Detect conflicts
      for (Timer timer : superClass.getTimers()) {
        List<Variable> filteredVariables = new ArrayList<>(variables);
        filteredVariables.removeIf(it -> reactor.getTimers().contains(it));
        if (hasNameConflict(timer, filteredVariables)) {
          conflicts.add(timer);
        } else {
          variables.add(timer);
        }
      }

      // Report conflicts.
      if (conflicts.size() > 0) {
        List<String> names = new ArrayList<>();
        for (Variable it : conflicts) {
          names.add(it.getName());
        }
        error(
            String.format(
                "Cannot extend %s due to the following conflicts: %s.",
                superClass.getName(), String.join(",", names)),
            Literals.REACTOR__SUPER_CLASSES);
      }
    }
  }

  private void checkMaxWaitAttribute(Attribute attr) {
    // Check that the attribute is at the top level.
    var container = attr.eContainer();
    if (!(container instanceof Instantiation) && !(container instanceof Connection)) {
      warning(
          "maxwait attribute can only be used in an instantiation or connection.",
          attr,
          Literals.ATTRIBUTE__ATTR_NAME);
    }
    var top = container.eContainer();
    if (!(top instanceof Reactor) || !((Reactor) top).isFederated()) {
      warning(
          "maxwait attribute can only be used at the top level in a federated reactor.",
          attr,
          Literals.ATTRIBUTE__ATTR_NAME);
      return;
    }
  }

  @Check(CheckType.FAST)
  public void checkReactorIconAttribute(Reactor reactor) {
    var path = AttributeUtils.getIconPath(reactor);
    if (path != null) {
      var param = AttributeUtils.findAttributeByName(reactor, "icon").getAttrParms().get(0);
      // Check file extension
      var validExtensions = Set.of("bmp", "png", "gif", "ico", "jpeg");
      var extensionStart = path.lastIndexOf(".");
      var extension = extensionStart != -1 ? path.substring(extensionStart + 1) : "";
      if (!validExtensions.contains(extension.toLowerCase())) {
        warning(
            "File extension '"
                + extension
                + "' is not supported. Provide any of: "
                + String.join(", ", validExtensions),
            param,
            Literals.ATTR_PARM__VALUE);
        return;
      }

      // Check file location
      var iconLocation = FileUtil.locateFile(path, reactor.eResource());
      if (iconLocation == null) {
        warning("Cannot locate icon file.", param, Literals.ATTR_PARM__VALUE);
      }
      if (("file".equals(iconLocation.getScheme()) || iconLocation.getScheme() == null)
          && !(new File(iconLocation.getPath()).exists())) {
        warning("Icon does not exist.", param, Literals.ATTR_PARM__VALUE);
      }
    }
  }

  @Check(CheckType.FAST)
  public void checkInitialMode(Reactor reactor) {
    if (!reactor.getModes().isEmpty()) {
      long initialModesCount = reactor.getModes().stream().filter(m -> m.isInitial()).count();
      if (initialModesCount == 0) {
        error("Every modal reactor requires one initial mode.", Literals.REACTOR__MODES, 0);
      } else if (initialModesCount > 1) {
        reactor.getModes().stream()
            .filter(m -> m.isInitial())
            .skip(1)
            .forEach(
                m -> {
                  error(
                      "A modal reactor can only have one initial mode.",
                      Literals.REACTOR__MODES,
                      reactor.getModes().indexOf(m));
                });
      }
    }
  }

  @Check(CheckType.FAST)
  public void checkModeStateNamespace(Reactor reactor) {
    if (!reactor.getModes().isEmpty()) {
      var names = new ArrayList<String>();
      reactor.getStateVars().stream().map(it -> it.getName()).forEach(it -> names.add(it));
      for (var mode : reactor.getModes()) {
        for (var stateVar : mode.getStateVars()) {
          if (names.contains(stateVar.getName())) {
            error(
                String.format(
                    "Duplicate state variable '%s'. (State variables are currently scoped on"
                        + " reactor level not modes)",
                    stateVar.getName()),
                stateVar,
                Literals.STATE_VAR__NAME);
          }
          names.add(stateVar.getName());
        }
      }
    }
  }

  @Check(CheckType.FAST)
  public void checkModeTimerNamespace(Reactor reactor) {
    if (!reactor.getModes().isEmpty()) {
      var names = new ArrayList<String>();
      reactor.getTimers().stream().map(it -> it.getName()).forEach(it -> names.add(it));
      for (var mode : reactor.getModes()) {
        for (var timer : mode.getTimers()) {
          if (names.contains(timer.getName())) {
            error(
                String.format(
                    "Duplicate Timer '%s'. (Timers are currently scoped on reactor level not"
                        + " modes)",
                    timer.getName()),
                timer,
                Literals.VARIABLE__NAME);
          }
          names.add(timer.getName());
        }
      }
    }
  }

  @Check(CheckType.FAST)
  public void checkModeActionNamespace(Reactor reactor) {
    if (!reactor.getModes().isEmpty()) {
      var names = new ArrayList<String>();
      reactor.getActions().stream().map(it -> it.getName()).forEach(it -> names.add(it));
      for (var mode : reactor.getModes()) {
        for (var action : mode.getActions()) {
          if (names.contains(action.getName())) {
            error(
                String.format(
                    "Duplicate Action '%s'. (Actions are currently scoped on reactor level not"
                        + " modes)",
                    action.getName()),
                action,
                Literals.VARIABLE__NAME);
          }
          names.add(action.getName());
        }
      }
    }
  }

  @Check(CheckType.FAST)
  public void checkModeInstanceNamespace(Reactor reactor) {
    if (!reactor.getModes().isEmpty()) {
      var names = new ArrayList<String>();
      reactor.getActions().stream().map(it -> it.getName()).forEach(it -> names.add(it));
      for (var mode : reactor.getModes()) {
        for (var instantiation : mode.getInstantiations()) {
          if (names.contains(instantiation.getName())) {
            error(
                String.format(
                    "Duplicate Instantiation '%s'. (Instantiations are currently scoped on reactor"
                        + " level not modes)",
                    instantiation.getName()),
                instantiation,
                Literals.INSTANTIATION__NAME);
          }
          names.add(instantiation.getName());
        }
      }
    }
  }

  @Check(CheckType.FAST)
  public void checkMissingStateResetInMode(Reactor reactor) {
    if (!reactor.getModes().isEmpty()) {
      var resetModes = new HashSet<Mode>();
      // Collect all modes that may be reset
      for (var m : reactor.getModes()) {
        for (var r : m.getReactions()) {
          for (var e : r.getEffects()) {
            if (e.getVariable() instanceof Mode && e.getTransition() != ModeTransition.HISTORY) {
              resetModes.add((Mode) e.getVariable());
            }
          }
        }
      }
      for (var m : resetModes) {
        // Check state variables in this mode
        if (!m.getStateVars().isEmpty()) {
          var hasResetReaction =
              m.getReactions().stream()
                  .anyMatch(
                      r ->
                          r.getTriggers().stream()
                              .anyMatch(
                                  t ->
                                      (t instanceof BuiltinTriggerRef
                                          && ((BuiltinTriggerRef) t).getType()
                                              == BuiltinTrigger.RESET)));
          if (!hasResetReaction) {
            for (var s : m.getStateVars()) {
              if (!s.isReset()) {
                error(
                    "State variable is not reset upon mode entry. It is neither marked for"
                        + " automatic reset nor is there a reset reaction.",
                    m,
                    Literals.MODE__STATE_VARS,
                    m.getStateVars().indexOf(s));
              }
            }
          }
        }
        // Check state variables in instantiated reactors
        if (!m.getInstantiations().isEmpty()) {
          for (var i : m.getInstantiations()) {
            var error = new LinkedHashSet<StateVar>();
            var checked = new HashSet<Reactor>();
            var toCheck = new LinkedList<Reactor>();
            toCheck.add((Reactor) i.getReactorClass());
            while (!toCheck.isEmpty()) {
              var check = toCheck.pop();
              checked.add(check);
              if (!check.getStateVars().isEmpty()) {
                var hasResetReaction =
                    check.getReactions().stream()
                        .anyMatch(
                            r ->
                                r.getTriggers().stream()
                                    .anyMatch(
                                        t ->
                                            (t instanceof BuiltinTriggerRef
                                                && ((BuiltinTriggerRef) t).getType()
                                                    == BuiltinTrigger.RESET)));
                if (!hasResetReaction) {
                  // Add state vars that are not self-resetting to the error
                  check.getStateVars().stream()
                      .filter(s -> !s.isReset())
                      .forEachOrdered(error::add);
                }
              }
              // continue with inner
              for (var innerInstance : check.getInstantiations()) {
                var next = (Reactor) innerInstance.getReactorClass();
                if (!checked.contains(next)) {
                  toCheck.push(next);
                }
              }
            }
            if (!error.isEmpty()) {
              error(
                  "This reactor contains state variables that are not reset upon mode entry: "
                      + error.stream()
                          .map(
                              e -> e.getName() + " in " + ASTUtils.getEnclosingReactor(e).getName())
                          .collect(Collectors.joining(", "))
                      + ".\n"
                      + "The state variables are neither marked for automatic reset nor have a"
                      + " dedicated reset reaction. It is unsafe to instantiate this reactor inside"
                      + " a mode entered with reset.",
                  m,
                  Literals.MODE__INSTANTIATIONS,
                  m.getInstantiations().indexOf(i));
            }
          }
        }
      }
    }
  }

  @Check(CheckType.FAST)
  public void checkStateResetWithoutInitialValue(StateVar state) {
    if (state.isReset() && (state.getInit() == null)) {
      error(
          "The state variable can not be automatically reset without an initial value.",
          state,
          Literals.STATE_VAR__RESET);
    }
  }

  @Check(CheckType.FAST)
  public void checkUnspecifiedTransitionType(Reaction reaction) {
    for (var effect : reaction.getEffects()) {
      var variable = effect.getVariable();
      if (variable instanceof Mode) {
        // The transition type is always set to default by Xtext.
        // Hence, check if there is an explicit node for the transition type in the AST.
        var transitionAssignment =
            NodeModelUtils.findNodesForFeature((EObject) effect, Literals.VAR_REF__TRANSITION);
        if (transitionAssignment.isEmpty()) { // Transition type not explicitly specified.
          var mode = (Mode) variable;
          // Check if reset or history transition would make a difference.
          var makesDifference =
              !mode.getStateVars().isEmpty()
                  || !mode.getTimers().isEmpty()
                  || !mode.getActions().isEmpty()
                  || mode.getConnections().stream().anyMatch(c -> c.getDelay() != null);
          if (!makesDifference && !mode.getInstantiations().isEmpty()) {
            // Also check instantiated reactors
            for (var i : mode.getInstantiations()) {
              var checked = new HashSet<Reactor>();
              var toCheck = new LinkedList<Reactor>();
              toCheck.add((Reactor) i.getReactorClass());
              while (!toCheck.isEmpty() && !makesDifference) {
                var check = toCheck.pop();
                checked.add(check);

                makesDifference |=
                    !check.getModes().isEmpty()
                        || !ASTUtils.allStateVars(check).isEmpty()
                        || !ASTUtils.allTimers(check).isEmpty()
                        || !ASTUtils.allActions(check).isEmpty()
                        || ASTUtils.allConnections(check).stream()
                            .anyMatch(c -> c.getDelay() != null);

                // continue with inner
                for (var innerInstance : check.getInstantiations()) {
                  var next = (Reactor) innerInstance.getReactorClass();
                  if (!checked.contains(next)) {
                    toCheck.push(next);
                  }
                }
              }
            }
          }
          if (makesDifference) {
            warning(
                "You should specify a transition type! "
                    + "Reset and history transitions have different effects on this target mode. "
                    + "Currently, a reset type is implicitly assumed.",
                reaction,
                Literals.REACTION__EFFECTS,
                reaction.getEffects().indexOf(effect));
          }
        }
      }
    }
  }

  //////////////////////////////////////////////////////////////
  //// Public methods.

  /** Return the error reporter for this validator. */
  public ValidatorMessageReporter getErrorReporter() {
    return this.errorReporter;
  }

  /** Implementation required by xtext to report validation errors. */
  @Override
  public ValidationMessageAcceptor getMessageAcceptor() {
    return messageAcceptor == null ? this : messageAcceptor;
  }

  //////////////////////////////////////////////////////////////
  //// Protected methods.

  /** Generate an error message for an AST node. */
  @Override
  protected void error(java.lang.String message, org.eclipse.emf.ecore.EStructuralFeature feature) {
    super.error(message, feature);
  }

  //////////////////////////////////////////////////////////////
  //// Private methods.

  /**
   * For each input, report a conflict if: 1) the input exists and the type doesn't match; or 2) the
   * input has a name clash with variable that is not an input.
   *
   * @param superVars List of typed variables of a particular kind (i.e., inputs, outputs, or
   *     actions), found in a super class.
   * @param sameKind Typed variables of the same kind, found in the subclass.
   * @param allOwn Accumulator of non-conflicting variables incorporated in the subclass.
   * @param conflicts Set of variables that are in conflict, to be used by this function to report
   *     conflicts.
   */
  private <T extends TypedVariable> void checkConflict(
      EList<T> superVars, EList<T> sameKind, List<Variable> allOwn, HashSet<Variable> conflicts) {
    for (T superVar : superVars) {
      T match = null;
      for (T it : sameKind) {
        if (it.getName().equals(superVar.getName())) {
          match = it;
          break;
        }
      }
      List<Variable> rest = new ArrayList<>(allOwn);
      rest.removeIf(it -> sameKind.contains(it));

      if ((match != null && superVar.getType() != match.getType())
          || hasNameConflict(superVar, rest)) {
        conflicts.add(superVar);
      } else {
        allOwn.add(superVar);
      }
    }
  }

  /**
   * Check the name of a feature for illegal substrings such as reserved identifiers and names with
   * double leading underscores.
   *
   * @param name The name.
   * @param feature The feature containing the name (for error reporting).
   */
  private void checkName(String name, EStructuralFeature feature) {

    // Raises an error if the string starts with two underscores.
    if (name.length() >= 2 && name.substring(0, 2).equals("__")) {
      error(UNDERSCORE_MESSAGE + name, feature);
    }

    if (this.target.isReservedIdent(name)) {
      error(RESERVED_MESSAGE + name, feature);
    }
  }

  /**
   * Check that the initializer is compatible with the type. Note that if the type is inferred it
   * will necessarily be compatible so this method is not harmful.
   */
  public void typeCheck(Initializer init, InferredType type, EStructuralFeature feature) {
    if (init == null) {
      return;
    }

    if (type.isTime) {
      checkExpressionIsTime(init.getExpr(), feature);
    }
  }

  private void checkExpressionIsTime(Expression value, EStructuralFeature feature) {
    if (value == null || value instanceof Time) {
      return;
    }

    if (value instanceof ParameterReference) {
      if (!ASTUtils.isOfTimeType(((ParameterReference) value).getParameter())
          && target.requiresTypes) {
        error("Referenced parameter is not of time type.", feature);
      }
      return;
    } else if (value instanceof Literal) {
      if (ASTUtils.isZero(((Literal) value).getLiteral())) {
        return;
      }

      if (ASTUtils.isInteger(((Literal) value).getLiteral())) {
        error("Missing time unit.", feature);
        return;
      }

      if (value instanceof BracedListExpression) {
        final var exprs = ((BracedListExpression) value).getItems();
        if (exprs.size() == 1) {
          checkExpressionIsTime(exprs.get(0), feature);
          return;
        }
      } else if (value instanceof CodeExpr) {
        // We leave checking of target code expressions to the target compiler
        return;
      }
    }

    // fallthrough
    error("Invalid time value.", feature);
  }

  /**
   * Return the number of main or federated reactors declared.
   *
   * @param iter An iterator over all objects in the resource.
   */
  private int countMainOrFederated(TreeIterator<EObject> iter) {
    int nMain = 0;
    while (iter.hasNext()) {
      EObject obj = iter.next();
      if (!(obj instanceof Reactor)) {
        continue;
      }
      Reactor r = (Reactor) obj;
      if (r.isMain() || r.isFederated()) {
        nMain++;
      }
    }
    return nMain;
  }

  /**
   * Report whether a given reactor has dependencies on a cyclic instantiation pattern. This means
   * the reactor has an instantiation in it -- directly or in one of its contained reactors -- that
   * is self-referential.
   *
   * @param reactor The reactor definition to find out whether it has any dependencies on cyclic
   *     instantiations.
   * @param cycleSet The set of all reactors that are part of an instantiation cycle.
   * @param visited The set of nodes already visited in this graph traversal.
   */
  private boolean dependsOnCycle(Reactor reactor, Set<Reactor> cycleSet, Set<Reactor> visited) {
    Set<Reactor> origins = info.instantiationGraph.getUpstreamAdjacentNodes(reactor);
    if (visited.contains(reactor)) {
      return false;
    } else {
      visited.add(reactor);
      for (Reactor it : origins) {
        if (cycleSet.contains(it) || dependsOnCycle(it, cycleSet, visited)) {
          // Reached a cycle.
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Report whether the name of the given element matches any variable in the ones to check against.
   *
   * @param element The element to compare against all variables in the given iterable.
   * @param toCheckAgainst Iterable variables to compare the given element against.
   */
  private boolean hasNameConflict(Variable element, Iterable<Variable> toCheckAgainst) {
    int numNameConflicts = 0;
    for (Variable it : toCheckAgainst) {
      if (it.getName().equals(element.getName())) {
        numNameConflicts++;
      }
    }
    return numNameConflicts > 0;
  }

  /** Return true if target is C or a C-based target like CCpp. */
  private boolean isCBasedTarget() {
    return true;
  }

  /**
   * Report whether a given imported reactor is used in this resource or not.
   *
   * @param reactor The imported reactor to check whether it is used.
   */
  private boolean isUnused(ImportedReactor reactor) {
    TreeIterator<EObject> instantiations = reactor.eResource().getAllContents();
    TreeIterator<EObject> subclasses = reactor.eResource().getAllContents();

    boolean instantiationsCheck = true;
    while (instantiations.hasNext() && instantiationsCheck) {
      EObject obj = instantiations.next();
      if (!(obj instanceof Instantiation)) {
        continue;
      }
      Instantiation inst = (Instantiation) obj;
      instantiationsCheck &=
          (inst.getReactorClass() != reactor
              && inst.getReactorClass() != reactor.getReactorClass());
    }

    boolean subclassesCheck = true;
    while (subclasses.hasNext() && subclassesCheck) {
      EObject obj = subclasses.next();
      if (!(obj instanceof Reactor)) {
        continue;
      }
      Reactor subclass = (Reactor) obj;
      for (ReactorDecl decl : subclass.getSuperClasses()) {
        subclassesCheck &= (decl != reactor && decl != reactor.getReactorClass());
      }
    }
    return instantiationsCheck && subclassesCheck;
  }

  /**
   * Return true if the two types match. Unfortunately, xtext does not seem to create a suitable
   * equals() method for Type, so we have to do this manually.
   */
  private boolean sameType(Type type1, Type type2) {
    if (type1 == null) {
      return type2 == null;
    }
    if (type2 == null) {
      return type1 == null;
    }
    // Most common case first.
    if (type1.getId() != null) {
      if (type1.getStars() != null) {
        if (type2.getStars() == null) {
          return false;
        }
        if (type1.getStars().size() != type2.getStars().size()) {
          return false;
        }
      }
      return (type1.getId().equals(type2.getId()));
    }

    // Type specification in the grammar is:
    // (time?='time' (arraySpec=ArraySpec)?) | ((id=(DottedName) (stars+='*')* ('<'
    // typeParms+=TypeParm (',' typeParms+=TypeParm)* '>')? (arraySpec=ArraySpec)?) | code=Code);
    if (type1.isTime()) {
      if (!type2.isTime()) {
        return false;
      }
      // Ignore the arraySpec because that is checked when connection
      // is checked for balance.
      return true;
    }
    // Type must be given in a code body
    return type1.getCode().getBody().equals(type2.getCode().getBody());
  }

  //////////////////////////////////////////////////////////////
  //// Private fields.

  /** The error reporter. */
  private ValidatorMessageReporter errorReporter =
      new ValidatorMessageReporter(getMessageAcceptor(), new ValidatorStateAccess());

  /** Helper class containing information about the model. */
  private ModelInfo info = new ModelInfo();

  @Inject(optional = true)
  private ValidationMessageAcceptor messageAcceptor;

  /** The declared target. */
  private Target target;

  //////////////////////////////////////////////////////////////
  //// Private static constants.

  private static String ACTIONS_MESSAGE =
      "\"actions\" is a reserved word for the TypeScript target for objects "
          + "(inputs, outputs, actions, timers, parameters, state, reactor definitions, "
          + "and reactor instantiation): ";

  private static String HOST_OR_FQN_REGEX =
      "^([a-z0-9]+(-[a-z0-9]+)*)|(([a-z0-9]+(-[a-z0-9]+)*\\.)+[a-z]{2,})$";

  /** Regular expression to check the validity of IPV4 addresses (due to David M. Syzdek). */
  private static String IPV4_REGEX =
      "((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}"
          + "(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])";

  /**
   * Regular expression to check the validity of IPV6 addresses (due to David M. Syzdek), with minor
   * adjustment to allow up to six IPV6 segments (without truncation) in front of an embedded
   * IPv4-address.
   */
  private static String IPV6_REGEX =
      "(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|"
          + "([0-9a-fA-F]{1,4}:){1,7}:|"
          + "([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|"
          + "([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|"
          + "([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|"
          + "([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|"
          + "([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|"
          + "[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|"
          + ":((:[0-9a-fA-F]{1,4}){1,7}|:)|"
          + "fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|"
          + "::(ffff(:0{1,4}){0,1}:){0,1}"
          + IPV4_REGEX
          + "|"
          + "([0-9a-fA-F]{1,4}:){1,4}:"
          + IPV4_REGEX
          + "|"
          + "([0-9a-fA-F]{1,4}:){1,6}"
          + IPV4_REGEX
          + ")";

  private static String RESERVED_MESSAGE =
      "Reserved words in the target language are not allowed for objects (inputs, outputs, actions,"
          + " timers, parameters, state, reactor definitions, and reactor instantiation): ";

  private static List<String> SPACING_VIOLATION_POLICIES = List.of("defer", "drop", "replace");

  private static String UNDERSCORE_MESSAGE =
      "Names of objects (inputs, outputs, actions, timers, parameters, "
          + "state, reactor definitions, and reactor instantiation) may not start with \"__\": ";

  private static String USERNAME_REGEX = "^[a-z_]([a-z0-9_-]{0,31}|[a-z0-9_-]{0,30}\\$)$";
}
