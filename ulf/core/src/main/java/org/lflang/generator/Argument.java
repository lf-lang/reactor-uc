package org.lflang.generator;

/**
 * A record that ties a target property to a value obtained as a parameter to a compilation run.
 *
 * @param property A target property.
 * @param value The value to assign to it.
 * @param <T> The type of the value.
 * @author Marten Lohstroh
 */
public record Argument<T>(T value) {}
