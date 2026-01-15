
#include "reactor-uc/trace/otel/otel.h"
#include "reactor-uc/reaction.h"

#include "reactor-uc/environment.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/logging.h"

#include <opentelemetry_c/opentelemetry_c.h>

#include <string.h>

void Reaction_fqn(Reaction* reaction, char* array, size_t length) {
  (void)length;
  Reactor* parent = reaction->parent;

  size_t i = 0;
  while (parent->name[i] != '\0') {
    i++;
  }

  memcpy(array, parent->name, i);
  array[i] = '_';
  sprintf(array + i + 1, "%d", (int)reaction->index);
  array[i + 2] = '\0';
  //while (parent != NULL) {
    //memcpy(array + i, parent->name, parent);
  //}
}

void OTELTrace_open_span(Tracer* tracer, Reaction* reaction) {
  struct OTELTrace* otel_trace = (struct OTELTrace*)tracer;
  void *span = otelc_start_span(tracer, "test-reactor-uc", OTELC_SPAN_KIND_INTERNAL, "");
  void *map = otelc_create_attr_map();

  Reactor* container = reaction->parent;

  // xronos.element_type - For reaction spans, element_type is always "reaction"
  const char* element_type_value = "reaction";

  // xronos.fqn - For the fully qualified name of the reaction
  char reaction_fqn[128];
  Reaction_fqn(reaction, reaction_fqn, sizeof(reaction_fqn));

  // xronos.name
  char reaction_name_str[32];
  snprintf(reaction_name_str, sizeof(reaction_name_str), "%d", (int)reaction->index);

  // Use bytes instead of string to avoid UTF-8 validation issues
  otelc_set_str_attr(map, "xronos.element_type",  element_type_value);

  // xronos.fqn - Fully Qualified Name (e.g., "MyReactor.0", "Parent.Child.1")
  otelc_set_str_attr(map, "xronos.fqn",  reaction_fqn);

  // xronos.name - The reaction number as a string (e.g., "0" from "MyReactor.0")
  otelc_set_str_attr(map, "xronos.name", reaction_name_str);

  // xronos.container_fqn - The reactor name (e.g., "MyReactor" from "MyReactor.0")
  otelc_set_str_attr(map, "xronos.container_fqn", container->name);

  tag_t current_tag = container->env->scheduler->current_tag(container->env->scheduler);

  // Extract high cardinality data from trace record
  // xronos.timestamp (logical timestamp)
  int64_t timestamp = current_tag.time;
  otelc_set_int64_t_attr(map, "xronos.timestamp", timestamp);

  // xronos.microstep
  int64_t microstep = current_tag.microstep;
  otelc_set_int64_t_attr(map, "xronos.microstep", microstep);

  // xronos.lag (system clock - logical time)
  int64_t lag = container->env->get_elapsed_physical_time(container->env) - container->env->get_elapsed_logical_time(container->env);
  otelc_set_int64_t_attr(map, "xronos.lag", lag);

  otelc_set_span_attrs(span, map);
  otelc_destroy_attr_map(map);

  otel_trace->current_span = span;
}
void OTELTrace_close_span(Tracer* tracer) {
  OTELTrace* otel_trace = (struct OTELTrace*)tracer;

  otelc_end_span(otel_trace->current_span);
}

void OTELTrace_ctor(struct OTELTrace* tracer, const char* endpoint, const char* application_name, const char* hostname) {
  // Set the traces endpoint
  if (setenv("OTEL_EXPORTER_OTLP_TRACES_ENDPOINT", endpoint, 1) != 0) {
    LF_ERR(FED, "Cannot set env variable for OTEL endpoint");
  }

  int use_ssl = (strncmp(endpoint, "https://", 8) == 0);
  const char* insecure_value = use_ssl ? "false" : "true";

  if (setenv("OTEL_EXPORTER_OTLP_TRACES_INSECURE", insecure_value, 1) != 0) {
    LF_ERR(FED, "Cannot set env variable for OTEL endpoint");
  }

  otelc_init_tracer_provider(
        application_name,
        "1.0.0",
        "reactor-uc",
        hostname);

  tracer->tracer = otelc_get_tracer();

  tracer->super.open_span = OTELTrace_open_span;
  tracer->super.close_span = OTELTrace_close_span;
}