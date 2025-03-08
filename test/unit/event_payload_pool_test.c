
#include "unity.h"

#include "reactor-uc/event.h"
#include <string.h>

// Verify that we can push a bunch of values and then pop them off
void test_allocate_free(void) {
  lf_ret_t ret;
  EventPayloadPool t;
  int buffer[10];
  bool used[10];
  int *payloads[10];
  EventPayloadPool_ctor(&t, (void *)buffer, used, sizeof(int), 10, 0);

  for (int j = 0; j < 3; j++) {
    int val = 1;
    for (int i = 0; i < 10; i++) {
      int val = j * 10 + i;
      ret = t.allocate(&t, (void *)&payloads[i]);
      TEST_ASSERT_EQUAL(LF_OK, ret);
      memcpy(payloads[i], &val, sizeof(int));
    }

    for (int i = 0; i < 10; i++) {
      int exp = j * 10 + i;
      TEST_ASSERT_EQUAL(exp, *payloads[i]);
      ret = t.free(&t, payloads[i]);
      TEST_ASSERT_EQUAL(LF_OK, ret);
    }
  }
}
void test_allocate_reserved(void) {
  EventPayloadPool t;
  int buffer[2];
  bool used[2];
  void *payload;
  EventPayloadPool_ctor(&t, (void *)&buffer, used, sizeof(int), 4, 2);
  TEST_ASSERT_EQUAL(LF_OK, t.allocate_reserved(&t, &payload));
  TEST_ASSERT_EQUAL(LF_OK, t.allocate_reserved(&t, &payload));
  TEST_ASSERT_EQUAL(LF_NO_MEM, t.allocate_reserved(&t, &payload));
  TEST_ASSERT_EQUAL(LF_OK, t.allocate(&t, &payload));
  TEST_ASSERT_EQUAL(LF_OK, t.allocate(&t, &payload));
  TEST_ASSERT_EQUAL(LF_NO_MEM, t.allocate(&t, &payload));
}

void test_allocate_full(void) {
  EventPayloadPool t;
  int buffer[2];
  bool used[2];
  void *payload;
  EventPayloadPool_ctor(&t, (void *)&buffer, used, sizeof(int), 2, 0);
  TEST_ASSERT_EQUAL(LF_OK, t.allocate(&t, &payload));
  TEST_ASSERT_EQUAL(LF_OK, t.allocate(&t, &payload));
  TEST_ASSERT_EQUAL(LF_NO_MEM, t.allocate(&t, &payload));
}

void test_free_wrong(void) {
  EventPayloadPool t;
  int buffer[2];
  bool used[2];
  void *payload;

  EventPayloadPool_ctor(&t, (void *)&buffer, used, sizeof(int), 2, 0);
  TEST_ASSERT_EQUAL(LF_OK, t.allocate(&t, &payload));
  TEST_ASSERT_EQUAL(LF_OK, t.allocate(&t, &payload));
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, t.free(&t, (void *)&used));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_allocate_free);
  RUN_TEST(test_allocate_full);
  RUN_TEST(test_allocate_reserved);
  RUN_TEST(test_free_wrong);
  return UNITY_END();
}