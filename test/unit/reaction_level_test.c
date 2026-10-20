#include "level_graph.h"
#include "reactor-uc/environment.h"
#include "unity.h"

Environment* _lf_environment = NULL;

static TestReactor main_reactor, c, g;
static TestPort c_in, g_in;
static TestConnection c_to_g;

/**
 * main.reaction_1 (level 1) writes c.in, and c forwards it inward with c.in -> g.in.
 * g.reaction_0 reads g.in, so it must come after main.reaction_1 although the writer
 * sits on an intermediate port of the chain, not on its final upstream port.
 */
void test_writer_of_intermediate_port_is_counted(void) {
  TestReactor_ctor(&main_reactor, "main", NULL, 2);
  TestReactor_ctor(&c, "c", &main_reactor, 0);
  TestReactor_ctor(&g, "g", &c, 1);
  TestReactor_add_port(&c, &c_in, TRIG_INPUT);
  TestReactor_add_port(&g, &g_in, TRIG_INPUT);
  TestPort_add_source(&c_in, &main_reactor.reactions[1]);
  TestPort_add_effect(&g_in, &g.reactions[0]);
  TestConnection_connect(&c_to_g, &c, &c_in, &g_in);

  TEST_ASSERT_EQUAL(LF_OK, main_reactor.super.calculate_levels(&main_reactor.super));
  TEST_ASSERT_EQUAL(1, main_reactor.reactions[1].level);
  TEST_ASSERT_EQUAL(2, g.reactions[0].level);
}

static TestReactor modal, m1, m2;
static TestPort modal_in, modal_out, m1_out, m2_in;
static TestConnection loop, m1_to_m2;

/**
 * The shape of LF's ModalCycleBreaker: modal.reaction_0 reads modal.in and modal.reaction_1
 * writes modal.out, with modal.out -> modal.in. Declaration order plus the port loop is a
 * cycle unless the writer-to-reader edge is dropped.
 */
static void build_port_loop(uint8_t reader_group, uint8_t writer_group) {
  TestReactor_ctor(&main_reactor, "main", NULL, 0);
  TestReactor_ctor(&modal, "modal", &main_reactor, 2);
  TestReactor_add_port(&modal, &modal_in, TRIG_INPUT);
  TestReactor_add_port(&modal, &modal_out, TRIG_OUTPUT);
  TestPort_add_effect(&modal_in, &modal.reactions[0]);
  TestPort_add_source(&modal_out, &modal.reactions[1]);
  TestConnection_connect(&loop, &main_reactor, &modal_out, &modal_in);
  Reaction_set_exclusion_group(&modal.reactions[0], reader_group);
  Reaction_set_exclusion_group(&modal.reactions[1], writer_group);
}

void test_exclusive_writer_is_skipped(void) {
  build_port_loop(2, 1);
  TEST_ASSERT_EQUAL(LF_OK, main_reactor.super.calculate_levels(&main_reactor.super));
  TEST_ASSERT_EQUAL(0, modal.reactions[0].level);
  TEST_ASSERT_EQUAL(1, modal.reactions[1].level);
}

// Without the skip, the port loop is a cycle and level computation fails.
void test_writer_without_group_is_not_skipped(void) {
  build_port_loop(2, 0);
  TEST_ASSERT_EQUAL(LF_ERR, main_reactor.super.calculate_levels(&main_reactor.super));
  build_port_loop(0, 1);
  TEST_ASSERT_EQUAL(LF_ERR, main_reactor.super.calculate_levels(&main_reactor.super));
}

void test_writer_in_same_group_is_not_skipped(void) {
  build_port_loop(1, 1);
  TEST_ASSERT_EQUAL(LF_ERR, main_reactor.super.calculate_levels(&main_reactor.super));
}

/** m1.reaction_1 (level 1) writes m1.out -> m2.in, read by m2.reaction_0: different parents. */
void test_writer_with_other_parent_is_not_skipped(void) {
  TestReactor_ctor(&main_reactor, "main", NULL, 0);
  TestReactor_ctor(&m1, "m1", &main_reactor, 2);
  TestReactor_ctor(&m2, "m2", &main_reactor, 1);
  TestReactor_add_port(&m1, &m1_out, TRIG_OUTPUT);
  TestReactor_add_port(&m2, &m2_in, TRIG_INPUT);
  TestPort_add_source(&m1_out, &m1.reactions[1]);
  TestPort_add_effect(&m2_in, &m2.reactions[0]);
  TestConnection_connect(&m1_to_m2, &main_reactor, &m1_out, &m2_in);
  Reaction_set_exclusion_group(&m1.reactions[1], 1);
  Reaction_set_exclusion_group(&m2.reactions[0], 2);

  TEST_ASSERT_EQUAL(LF_OK, main_reactor.super.calculate_levels(&main_reactor.super));
  TEST_ASSERT_EQUAL(2, m2.reactions[0].level);
}

void test_constructor_clears_exclusion_group(void) {
  main_reactor.reactions[0].exclusion_group = 5;
  TestReactor_ctor(&main_reactor, "main", NULL, 1);
  TEST_ASSERT_EQUAL(0, main_reactor.reactions[0].exclusion_group);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_writer_of_intermediate_port_is_counted);
  RUN_TEST(test_exclusive_writer_is_skipped);
  RUN_TEST(test_writer_without_group_is_not_skipped);
  RUN_TEST(test_writer_in_same_group_is_not_skipped);
  RUN_TEST(test_writer_with_other_parent_is_not_skipped);
  RUN_TEST(test_constructor_clears_exclusion_group);
  return UNITY_END();
}
