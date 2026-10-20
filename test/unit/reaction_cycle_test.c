#include "level_graph.h"
#include "reactor-uc/environment.h"
#include "unity.h"

Environment* _lf_environment = NULL;

static TestReactor main_reactor, x, y;
static TestPort x_in, x_out, y_in, y_out;
static TestConnection x_to_y, y_to_x;

/**
 * x.reaction_0 reads x.in, x.reaction_1 writes x.out, y.reaction_0 forwards y.in to
 * y.out, and x.out -> y.in, y.out -> x.in. Declaration order inside x closes the loop.
 */
static void build_cycle(void) {
  TestReactor_ctor(&main_reactor, "main", NULL, 0);
  TestReactor_ctor(&x, "x", &main_reactor, 2);
  TestReactor_ctor(&y, "y", &main_reactor, 1);
  TestReactor_add_port(&x, &x_in, TRIG_INPUT);
  TestReactor_add_port(&x, &x_out, TRIG_OUTPUT);
  TestReactor_add_port(&y, &y_in, TRIG_INPUT);
  TestReactor_add_port(&y, &y_out, TRIG_OUTPUT);
  TestPort_add_effect(&x_in, &x.reactions[0]);
  TestPort_add_source(&x_out, &x.reactions[1]);
  TestPort_add_effect(&y_in, &y.reactions[0]);
  TestPort_add_source(&y_out, &y.reactions[0]);
  TestConnection_connect(&x_to_y, &main_reactor, &x_out, &y_in);
  TestConnection_connect(&y_to_x, &main_reactor, &y_out, &x_in);
}

void test_cycle_is_reported(void) {
  build_cycle();
  TEST_ASSERT_EQUAL(LF_ERR, main_reactor.super.calculate_levels(&main_reactor.super));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_cycle_is_reported);
  return UNITY_END();
}
