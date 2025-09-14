package org.lflang;

import static org.lflang.ast.ASTUtils.factory;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import org.eclipse.emf.ecore.EObject;
import org.lflang.ast.ASTUtils;
import org.lflang.lf.*;
import org.lflang.target.property.type.PlatformType;
import org.lflang.util.StringUtil;

/**
 * A helper class for processing attributes in the AST.
 *
 * @author Shaokai Lin
 * @author Clément Fournier
 * @author Alexander Schulz-Rosengarten
 * @author Edward A. Lee
 */
public class AttributeUtils {

  /**
   * Return a list of attributes declared on the given node. An empty list is returned if the node
   * does not have any attributes.
   *
   * @param node The node to get the attributes from.
   * @throws IllegalArgumentException If the node cannot have attributes.
   */
  public static List<Attribute> getAttributes(EObject node) throws IllegalArgumentException {
    if (node instanceof Reactor) {
      return ((Reactor) node).getAttributes();
    } else if (node instanceof Reaction) {
      return ((Reaction) node).getAttributes();
    } else if (node instanceof Action) {
      return ((Action) node).getAttributes();
    } else if (node instanceof Timer) {
      return ((Timer) node).getAttributes();
    } else if (node instanceof StateVar) {
      return ((StateVar) node).getAttributes();
    } else if (node instanceof Parameter) {
      return ((Parameter) node).getAttributes();
    } else if (node instanceof Input) {
      return ((Input) node).getAttributes();
    } else if (node instanceof Output) {
      return ((Output) node).getAttributes();
    } else if (node instanceof Instantiation) {
      return ((Instantiation) node).getAttributes();
    } else if (node instanceof Watchdog) {
      return ((Watchdog) node).getAttributes();
    } else if (node instanceof Connection) {
      return ((Connection) node).getAttributes();
    }
    throw new IllegalArgumentException("Not annotatable: " + node);
  }

  /**
   * Return the first attribute with the given name if present, otherwise return null. If there are
   * multiple attributes with the same name, only the first one is returned.
   *
   * @param node The node to get the attribute from.
   * @param name The name of the attribute to get.
   * @throws IllegalArgumentException If the node cannot have attributes
   */
  public static Attribute findAttributeByName(EObject node, String name)
      throws IllegalArgumentException {
    List<Attribute> attrs = getAttributes(node);
    return attrs.stream()
        .filter(
            it ->
                it.getAttrName()
                    .equalsIgnoreCase(name)) // case-insensitive search (more user-friendly)
        .findFirst()
        .orElse(null);
  }

  /**
   * Return a list of attributes with the given name. An empty list is returned if the node does not
   * have any attributes with the given name.
   *
   * @param node The node to get the attributes from.
   * @param name The name of the attributes to get.
   * @throws IllegalArgumentException If the node cannot have attributes
   */
  public static List<Attribute> findAttributesByName(EObject node, String name)
      throws IllegalArgumentException {
    List<Attribute> attrs = getAttributes(node);
    return attrs.stream()
        .filter(
            it ->
                it.getAttrName()
                    .equalsIgnoreCase(name)) // case-insensitive search (more user-friendly)
        .toList();
  }

  /**
   * Return the first argument specified for the first attribute with the given name. If there is no
   * attribute with the given name, return null. If the attribute has no arguments, or if the
   * arguments are not of a suitable form, return null. This ignores any argument name, if one is
   * given, and only returns arguments of the form of strings ("foo"), numbers (123), boolean values
   * (true, false), or the time values `forever` or `never`. Time values with units are not returned
   * (the return value will be null).
   *
   * <p>This is a convenience method for common use cases where an attribute is specified with a
   * single argument of a suitable form. The validator should check that the arguments are of a
   * suitable form.
   *
   * @param node The node to get the attribute value from.
   * @param attrName The name of the attribute to get the value from.
   */
  public static String getAttributeValue(EObject node, String attrName) {
    final var attr = findAttributeByName(node, attrName);
    if (attr == null || attr.getAttrParms().isEmpty()) {
      return null;
    }
    return StringUtil.removeQuotes(attr.getAttrParms().get(0).getValue());
  }

  /**
   * Return the parameter with the given name for the specified attribute or null if no such
   * parameter is found.
   *
   * @param attribute The attribute to get the parameter from.
   * @param parameterName The name of the parameter to get.
   */
  public static AttrParm getAttributeParameter(Attribute attribute, String parameterName) {
    return (attribute == null)
        ? null
        : attribute.getAttrParms().stream()
            .filter(param -> Objects.equals(param.getName(), parameterName))
            .findFirst()
            .orElse(null);
  }

  /**
   * Return true if there is a parameter of the given attribute with the given name whose value is
   * "true" (case-insensitive) and return false otherwise.
   *
   * @param attribute The attribute to get the parameter from.
   * @param parameterName The name of the parameter to get.
   */
  public static boolean getBooleanAttributeParameter(Attribute attribute, String parameterName) {
    if (attribute == null || parameterName == null) {
      return false;
    }
    final var param = getAttributeParameter(attribute, parameterName);
    if (param == null || param.getValue() == null) {
      return false;
    }
    return param.getValue().equalsIgnoreCase("true");
  }

  /**
   * Return true if the specified node is an Input and has an {@code @sparse} attribute.
   *
   * @param node An AST node.
   */
  public static boolean isSparse(EObject node) {
    return findAttributeByName(node, "sparse") != null;
  }

  /** Return true if the reactor is marked to be a federate. */
  public static boolean isFederate(Reactor reactor) {
    return findAttributeByName(reactor, "_fed_config") != null;
  }

  /**
   * Return true if the reaction is marked to have a C code body.
   *
   * <p>Currently, this is only used for synthesized reactions in the context of federated execution
   * in Python.
   */
  public static boolean hasCBody(Reaction reaction) {
    return findAttributeByName(reaction, "_c_body") != null;
  }

  /** Return the declared label of the node, as given by the @label annotation. */
  public static String getLabel(EObject node) {
    return getAttributeValue(node, "label");
  }

  /** Return the declared icon of the node, as given by the @icon annotation. */
  public static String getIconPath(EObject node) {
    return getAttributeValue(node, "icon");
  }

  /**
   * Return the {@code @side} annotation for the given node (presumably a port) or null if there is
   * no such annotation.
   */
  public static String getPortSide(EObject node) {
    return getAttributeValue(node, "side");
  }

  /**
   * Return the `@layout` annotations for the given element or null if there is no such annotation.
   * Layout annotations have the form: ```
   *
   * @layout(option="string", value="any") ``` For example, ```
   * @layout(option="port.side", value="WEST") ``` This will return all such annotations for the
   *     specified node in the form of a map from the option name to the value.
   */
  public static Map<String, String> getLayoutOption(EObject node) {
    final List<Attribute> attrs = findAttributesByName(node, "layout");
    HashMap<String, String> result = new HashMap<>();
    for (Attribute attribute : attrs) {
      result.put(
          // FIXME: This assumes the parameters are in the correct order.
          StringUtil.removeQuotes(attribute.getAttrParms().get(0).getValue()),
          StringUtil.removeQuotes(attribute.getAttrParms().get(1).getValue()));
    }
    return result;
  }

  /**
   * Return a list of attributes with names containing the text "interface". An empty list is
   * returned if the node does not have any attributes with names starting with "interface".
   *
   * @param node The instantiation node to get the attributes from.
   */
  public static List<Attribute> getInterfaceAttributes(Instantiation node) {
    List<Attribute> attrs = getAttributes(node);
    return attrs.stream()
        .filter(
            it ->
                it.getAttrName()
                    .contains("interface")) // case-insensitive search (more user-friendly)
        .toList();
  }

  public static Attribute getJoiningPolicy(Instantiation node) {
    return findAttributeByName(node, "joining_policy");
  }

  public static int getMaxNumberOfPendingEvents(Action node) {
    Attribute attr = findAttributeByName(node, "max_pending_events");
    if (attr != null) {
      return Integer.valueOf(attr.getAttrParms().get(0).getValue());
    } else {
      return -1;
    }
  }

  public static int getConnectionBufferSize(Connection node) {
    Attribute attr = findAttributeByName(node, "buffer");
    if (attr != null) {
      return Integer.valueOf(attr.getAttrParms().get(0).getValue());
    } else {
      return -1;
    }
  }

  public static Attribute getLinkAttribute(Connection node) {
    return findAttributeByName(node, "link");
  }

  /** Return true if the specified instance has an {@code @enclave} attribute. */
  public static boolean isEnclave(Instantiation node) {
    return findAttributeByName(node, "enclave") != null;
  }

  /**
   * Annotate @{code node} with enclave @attribute
   *
   * @param node
   */
  public static void setEnclaveAttribute(Instantiation node) {
    if (!isEnclave(node)) {
      Attribute enclaveAttr = factory.createAttribute();
      enclaveAttr.setAttrName("enclave");
      node.getAttributes().add(enclaveAttr);
    }
  }

  public static PlatformType.Platform getFederatePlatform(Instantiation node) {
    if (findAttributeByName(node, "platform_native") != null) {
      return PlatformType.Platform.NATIVE;
    } else if (findAttributeByName(node, "platform_riot") != null) {
      return PlatformType.Platform.RIOT;
    } else if (findAttributeByName(node, "platform_zephyr") != null) {
      return PlatformType.Platform.ZEPHYR;
    } else {
      return PlatformType.Platform.AUTO;
    }
  }

  public static boolean isGrandmaster(Instantiation node) {
    return findAttributeByName(node, "grandmaster") != null;
  }

  public static Attribute getClockSyncAttr(Instantiation inst) {
    return findAttributeByName(inst, "clock_sync");
  }

  /**
   * Return the value of the `@maxwait` attribute of the given node or TimeValue.ZERO if does not
   * have one.
   *
   * @param The instantiation.
   */
  public static TimeValue getMaxWait(Instantiation node) {
    final var attr = findAttributeByName(node, "maxwait");
    if (attr != null) {
      // The attribute is expected to have a single argument of type Time
      // or one of the literals "forever", "never", or "0".
      // The validator checks this.
      final var time = attr.getAttrParms().get(0).getTime();
      if (time == null) {
        if (attr.getAttrParms().get(0).getValue().equals("forever")) {
          return TimeValue.MAX_VALUE;
        } else if (attr.getAttrParms().get(0).getValue().equals("never")) {
          // Interpret "never" as 0.
          return TimeValue.ZERO;
        }
      } else {
        return ASTUtils.toTimeValue(time);
      }
    }
    return TimeValue.ZERO;
  }
}
