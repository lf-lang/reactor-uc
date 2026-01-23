package org.lflang.target.property;

import org.lflang.MessageReporter;
import org.lflang.ast.ASTUtils;
import org.lflang.lf.Element;
import org.lflang.target.property.type.PrimitiveType;

/** The number of cores to utilize. The default is to use all available cores. */
public final class CoresProperty extends TargetProperty<Integer, PrimitiveType> {

  /** Singleton target property instance. */
  public static final CoresProperty INSTANCE = new CoresProperty();

  private CoresProperty() {
    super(PrimitiveType.NON_NEGATIVE_INTEGER);
  }

  @Override
  public Integer initialValue() {
    return 0;
  }

  @Override
  protected Integer fromString(String string, MessageReporter reporter) {
    try {
      return Integer.parseInt(string);
    } catch (NumberFormatException e) {
      reporter.nowhere().error("Invalid number of cores: " + string);
      return 0;
    }
  }

  @Override
  protected Integer fromAst(Element node, MessageReporter reporter) {
    return ASTUtils.toInteger(node);
  }

  @Override
  public Element toAstElement(Integer value) {
    return ASTUtils.toElement(value);
  }

  @Override
  public String name() {
    return "cores";
  }
}