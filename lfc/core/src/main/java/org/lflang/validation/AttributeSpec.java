/*************
 * Copyright (c) 2019-2022, The University of California at Berkeley.
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

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;
import org.lflang.ast.ASTUtils;
import org.lflang.lf.AttrParm;
import org.lflang.lf.Attribute;
import org.lflang.lf.LfPackage.Literals;
import org.lflang.util.StringUtil;

/**
 * Specification of the structure of an attribute annotation.
 *
 * @author Clément Fournier
 * @author Shaokai Lin
 */
public class AttributeSpec {

  private final Map<String, AttrParamSpec> paramSpecByName;

  public static final String VALUE_ATTR = "value";
  public static final String EACH_ATTR = "each";
  public static final String OPTION_ATTR = "option";

  /** A map from a string to a supported AttributeSpec */
  public static final Map<String, AttributeSpec> ATTRIBUTE_SPECS_BY_NAME = new HashMap<>();

  public AttributeSpec(List<AttrParamSpec> params) {
    if (params != null) {
      paramSpecByName = params.stream().collect(Collectors.toMap(it -> it.name, it -> it));
    } else {
      paramSpecByName = null;
    }
  }

  /** Check that the attribute conforms to this spec and whether attr has the correct name. */
  public void check(LFValidator validator, Attribute attr) {
    Set<String> seen;
    // If there is just one parameter, it is required to be named "value".
    if (attr.getAttrParms() != null
        && attr.getAttrParms().size() == 1
        && attr.getAttrParms().get(0).getName() == null) {
      // If we are in this branch,
      // then the user has provided @attr("value"),
      // which is a shorthand for @attr(value="value").
      if (paramSpecByName == null) {
        validator.error("Attribute doesn't take a parameter.", Literals.ATTRIBUTE__ATTR_NAME);
        return;
      }
      AttrParamSpec valueSpec = paramSpecByName.get(VALUE_ATTR);
      if (valueSpec == null) {
        validator.error("Attribute doesn't have a 'value' parameter.", Literals.ATTR_PARM__NAME);
        return;
      }

      valueSpec.check(validator, attr.getAttrParms().get(0));
      seen = Set.of(VALUE_ATTR);
    } else {
      // Process multiple parameters, each of which has to be named.
      seen = processNamedAttrParms(validator, attr);
    }

    // Check if there are any missing parameters required by this attribute.
    if (paramSpecByName != null) {
      Map<String, AttrParamSpec> missingParams = new HashMap<>(paramSpecByName);
      missingParams.keySet().removeAll(seen);
      missingParams.forEach(
          (name, paramSpec) -> {
            if (!paramSpec.isOptional) {
              validator.error(
                  "Missing required attribute parameter '" + name + "'.",
                  Literals.ATTRIBUTE__ATTR_PARMS);
            }
          });
    }
  }

  /**
   * Check whether the attribute parameters are named, whether these names are known, and whether
   * the named parameters conform to the param spec (whether the param has the right type, etc.).
   *
   * @param validator The current validator in use.
   * @param attr The attribute being checked.
   * @return A set of named attribute parameters the user provides.
   */
  private Set<String> processNamedAttrParms(LFValidator validator, Attribute attr) {
    Set<String> seen = new HashSet<>();
    if (attr.getAttrParms() != null) {
      for (AttrParm parm : attr.getAttrParms()) {
        if (paramSpecByName == null) {
          validator.error("Attribute does not take parameters.", Literals.ATTRIBUTE__ATTR_NAME);
          break;
        }
        if (parm.getName() == null) {
          validator.error("Missing name for attribute parameter.", Literals.ATTRIBUTE__ATTR_NAME);
          continue;
        }

        AttrParamSpec parmSpec = paramSpecByName.get(parm.getName());
        if (parmSpec == null) {
          validator.error(
              "\"" + parm.getName() + "\"" + " is an unknown attribute parameter.",
              Literals.ATTRIBUTE__ATTR_NAME);
          continue;
        }
        // Check whether a parameter conforms to its spec.
        parmSpec.check(validator, parm);
        seen.add(parm.getName());
      }
    }
    return seen;
  }

  /**
   * The specification of the attribute parameter.
   *
   * @param name The name of the attribute parameter
   * @param type The type of the parameter
   * @param isOptional True if the parameter is optional.
   */
  record AttrParamSpec(String name, AttrParamType type, boolean isOptional) {

    // Check if a parameter has the right type.
    // Currently, only String, Int, Boolean, Float, and target language are supported.
    public void check(LFValidator validator, AttrParm parm) {
      switch (type) {
        case STRING -> {
          if (!StringUtil.hasQuotes(parm.getValue())) {
            validator.error(
                "Incorrect type: \"" + parm.getName() + "\"" + " should have type String.",
                Literals.ATTRIBUTE__ATTR_NAME);
          }
        }
        case INT -> {
          if (!ASTUtils.isInteger(parm.getValue())) {
            validator.error(
                "Incorrect type: \"" + parm.getName() + "\"" + " should have type Int.",
                Literals.ATTRIBUTE__ATTR_NAME);
          }
        }
        case BIGINT -> {
          if (!ASTUtils.isBigInteger(parm.getValue())) {
            validator.error(
                "Incorrect type: \"" + parm.getName() + "\"" + " should have type Int.",
                Literals.ATTRIBUTE__ATTR_NAME);
          }
        }
        case BOOLEAN -> {
          if (!ASTUtils.isBoolean(parm.getValue())) {
            validator.error(
                "Incorrect type: \"" + parm.getName() + "\"" + " should have type Boolean.",
                Literals.ATTRIBUTE__ATTR_NAME);
          }
        }
        case FLOAT -> {
          if (!ASTUtils.isFloat(parm.getValue())) {
            validator.error(
                "Incorrect type: \"" + parm.getName() + "\"" + " should have type Float.",
                Literals.ATTRIBUTE__ATTR_NAME);
          }
        }
        case TIME -> {
          // TIME attributes use parm.getTime(), not parm.getValue(), unless the value is "0"
          if (parm.getTime() == null && !parm.getValue().equals("0")) {
            validator.error(
                "Incorrect time specification: \"" + parm.getName() + "\"",
                Literals.ATTRIBUTE__ATTR_NAME);
          }
        }
        default -> throw new IllegalArgumentException("unexpected type");
      }
    }
  }

  /** The type of attribute parameters currently supported. */
  enum AttrParamType {
    STRING,
    INT,
    BIGINT,
    BOOLEAN,
    FLOAT,
    TIME,
  }

  /*
   * The specs of the known annotations are declared here.
   * Note: If an attribute only has one parameter, the parameter name should be "value."
   */
  static {
    // @label("value")
    ATTRIBUTE_SPECS_BY_NAME.put(
        "label",
        new AttributeSpec(List.of(new AttrParamSpec(VALUE_ATTR, AttrParamType.STRING, false))));
    // @maxwait(time)
    ATTRIBUTE_SPECS_BY_NAME.put(
        "maxwait",
        new AttributeSpec(List.of(new AttrParamSpec(VALUE_ATTR, AttrParamType.TIME, false))));
    // @sparse
    ATTRIBUTE_SPECS_BY_NAME.put("sparse", new AttributeSpec(null));
    // @icon("value")
    ATTRIBUTE_SPECS_BY_NAME.put(
        "icon",
        new AttributeSpec(List.of(new AttrParamSpec(VALUE_ATTR, AttrParamType.STRING, false))));
    // @side("value")
    ATTRIBUTE_SPECS_BY_NAME.put(
        "side",
        new AttributeSpec(List.of(new AttrParamSpec(VALUE_ATTR, AttrParamType.STRING, false))));
    // @layout(option="string", value="any") e.g. @layout(option="port.side", value="WEST")
    ATTRIBUTE_SPECS_BY_NAME.put(
        "layout",
        new AttributeSpec(
            List.of(
                new AttrParamSpec(OPTION_ATTR, AttrParamType.STRING, false),
                new AttrParamSpec(VALUE_ATTR, AttrParamType.STRING, false))));
    // @enclave(each=boolean)
    ATTRIBUTE_SPECS_BY_NAME.put(
        "enclave",
        new AttributeSpec(List.of(new AttrParamSpec(EACH_ATTR, AttrParamType.BOOLEAN, true))));
    ATTRIBUTE_SPECS_BY_NAME.put("_fed_config", new AttributeSpec(List.of()));
    // @property(name="<property_name>", tactic="<induction|bmc>", spec="<SMTL_spec>")
    // SMTL is the safety fragment of Metric Temporal Logic (MTL).
    ATTRIBUTE_SPECS_BY_NAME.put(
        "property",
        new AttributeSpec(
            List.of(
                new AttrParamSpec("name", AttrParamType.STRING, false),
                new AttrParamSpec("tactic", AttrParamType.STRING, false),
                new AttrParamSpec("spec", AttrParamType.STRING, false),
                new AttrParamSpec("CT", AttrParamType.INT, true),
                new AttrParamSpec("expect", AttrParamType.BOOLEAN, true))));
    ATTRIBUTE_SPECS_BY_NAME.put("_c_body", new AttributeSpec(null));
    ATTRIBUTE_SPECS_BY_NAME.put(
        "_tpoLevel",
        new AttributeSpec(List.of(new AttrParamSpec(VALUE_ATTR, AttrParamType.INT, false))));
    ATTRIBUTE_SPECS_BY_NAME.put(
        "_networkReactor",
        new AttributeSpec(List.of(new AttrParamSpec(VALUE_ATTR, AttrParamType.STRING, false))));
    // @max_pending_event(<value>)
    ATTRIBUTE_SPECS_BY_NAME.put(
        "max_pending_events",
        new AttributeSpec(List.of(new AttrParamSpec(VALUE_ATTR, AttrParamType.INT, false))));
    // @buffer(<value>)
    ATTRIBUTE_SPECS_BY_NAME.put(
        "buffer",
        new AttributeSpec(List.of(new AttrParamSpec(VALUE_ATTR, AttrParamType.INT, false))));
    // @interface_tcp(name="string", address="string") e.g. @interface:tcp(name="if1",
    // address="127.0.0.1")

    ATTRIBUTE_SPECS_BY_NAME.put(
        "interface_uart",
        new AttributeSpec(
            List.of(
                new AttrParamSpec("name", AttrParamType.STRING, true),
                new AttrParamSpec("uart_device", AttrParamType.INT, true),
                new AttrParamSpec("baud_rate", AttrParamType.INT, true),
                new AttrParamSpec("data_bits", AttrParamType.INT, true),
                new AttrParamSpec("parity", AttrParamType.STRING, true),
                new AttrParamSpec("stop_bits", AttrParamType.INT, true),
                new AttrParamSpec("async", AttrParamType.BOOLEAN, true))));

    ATTRIBUTE_SPECS_BY_NAME.put(
        "interface_tcp",
        new AttributeSpec(
            List.of(
                new AttrParamSpec("name", AttrParamType.STRING, true),
                new AttrParamSpec("address", AttrParamType.STRING, true))));
    ATTRIBUTE_SPECS_BY_NAME.put(
        "interface_coap",
        new AttributeSpec(
            List.of(
                new AttrParamSpec("name", AttrParamType.STRING, true),
                new AttrParamSpec("address", AttrParamType.STRING, true))));
    ATTRIBUTE_SPECS_BY_NAME.put(
        "interface_custom",
        new AttributeSpec(
            List.of(
                new AttrParamSpec("name", AttrParamType.STRING, false),
                new AttrParamSpec("args", AttrParamType.STRING, true),
                new AttrParamSpec("include", AttrParamType.STRING, false))));
    ATTRIBUTE_SPECS_BY_NAME.put(
        "joining_policy",
        new AttributeSpec(List.of(new AttrParamSpec("policy", AttrParamType.STRING, false))));
    // @link(type="string", server_port=int, server_side="string", args="string") e.g.
    // @link(type="TcpIp", server_port=1042)
    ATTRIBUTE_SPECS_BY_NAME.put(
        "link",
        new AttributeSpec(
            List.of(
                new AttrParamSpec("left", AttrParamType.STRING, true),
                new AttrParamSpec("right", AttrParamType.STRING, true),
                new AttrParamSpec("server_port", AttrParamType.INT, true),
                new AttrParamSpec("server_side", AttrParamType.STRING, true))));
    // @platform_riot
    ATTRIBUTE_SPECS_BY_NAME.put("platform_riot", new AttributeSpec(null));
    // @platform_zephyr
    ATTRIBUTE_SPECS_BY_NAME.put("platform_zephyr", new AttributeSpec(null));
    // @platform_native
    ATTRIBUTE_SPECS_BY_NAME.put("platform_native", new AttributeSpec(null));
    // @clock_sync(grandmaster=true, server_port=1042)
    ATTRIBUTE_SPECS_BY_NAME.put(
        "clock_sync",
        new AttributeSpec(
            List.of(
                new AttrParamSpec("disabled", AttrParamType.BOOLEAN, true),
                new AttrParamSpec("grandmaster", AttrParamType.BOOLEAN, true),
                new AttrParamSpec("period", AttrParamType.BIGINT, true),
                new AttrParamSpec("max_adj", AttrParamType.INT, true),
                new AttrParamSpec("kp", AttrParamType.FLOAT, true),
                new AttrParamSpec("ki", AttrParamType.FLOAT, true))));
  }
}
