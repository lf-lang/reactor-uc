#include "reactor-uc/network_channel/frame.h"
#include "unity.h"
#include <string.h>

static void assert_roundtrip(const uint8_t* src, size_t len) {
  uint8_t enc[1200];
  uint8_t dec[1200];
  size_t enc_len = lf_cobs_encode(src, len, enc, sizeof(enc));
  TEST_ASSERT_GREATER_THAN(0, enc_len);
  // COBS output must never contain a zero byte.
  for (size_t i = 0; i < enc_len; i++) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0x00, enc[i], "COBS output contained a zero byte");
  }
  size_t dec_len = lf_cobs_decode(enc, enc_len, dec, sizeof(dec));
  TEST_ASSERT_EQUAL(len, dec_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(src, dec, len);
}

void test_cobs_single_nonzero(void) {
  const uint8_t src[] = {0x01};
  assert_roundtrip(src, sizeof(src));
}

void test_cobs_single_zero(void) {
  const uint8_t src[] = {0x00};
  assert_roundtrip(src, sizeof(src));
}

void test_cobs_all_zeros(void) {
  uint8_t src[32];
  memset(src, 0x00, sizeof(src));
  assert_roundtrip(src, sizeof(src));
}

void test_cobs_no_zeros(void) {
  uint8_t src[254];
  memset(src, 0xFF, sizeof(src));
  assert_roundtrip(src, sizeof(src));
}

void test_cobs_mixed(void) {
  const uint8_t src[] = {0x11, 0x22, 0x00, 0x33, 0x00, 0x00, 0x44};
  assert_roundtrip(src, sizeof(src));
}

// Known-answer vectors from Cheshire & Baker (SIGCOMM '97).
void test_cobs_known_answers(void) {
  struct {
    const uint8_t src[8];
    size_t src_len;
    const uint8_t enc[8];
    size_t enc_len;
  } cases[] = {
      {{0x00}, 1, {0x01, 0x01}, 2},
      {{0x00, 0x00}, 2, {0x01, 0x01, 0x01}, 3},
      {{0x11, 0x22, 0x00, 0x33}, 4, {0x03, 0x11, 0x22, 0x02, 0x33}, 5},
      {{0x11, 0x22, 0x33, 0x44}, 4, {0x05, 0x11, 0x22, 0x33, 0x44}, 5},
      {{0x11, 0x00, 0x00, 0x00}, 4, {0x02, 0x11, 0x01, 0x01, 0x01}, 5},
  };
  uint8_t out[16];
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    size_t n = lf_cobs_encode(cases[i].src, cases[i].src_len, out, sizeof(out));
    TEST_ASSERT_EQUAL_MESSAGE(cases[i].enc_len, n, "encoded length differs from the COBS standard");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(cases[i].enc, out, cases[i].enc_len);
  }
}

// A payload that is an exact multiple of 254 bytes is the case where a
// non-canonical encoder emits a spurious trailing block. 254 non-zero bytes
// must encode to exactly 255 bytes: one 0xFF code byte plus the data.
void test_cobs_254_multiple_is_canonical(void) {
  uint8_t src[254];
  uint8_t out[300];
  for (size_t i = 0; i < sizeof(src); i++) {
    src[i] = (uint8_t)(i + 1); // 0x01..0xFE, no zeros
  }
  size_t n = lf_cobs_encode(src, sizeof(src), out, sizeof(out));
  TEST_ASSERT_EQUAL_MESSAGE(255, n, "254 non-zero bytes must encode to exactly 255 bytes");
  TEST_ASSERT_EQUAL_HEX8(0xFF, out[0]);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(src, out + 1, sizeof(src));
}

// The 254-byte boundary is where COBS must emit an extra overhead byte.
void test_cobs_254_boundary(void) {
  uint8_t src[255];
  for (size_t i = 0; i < sizeof(src); i++) {
    src[i] = (uint8_t)(i % 253 + 1); // never zero
  }
  assert_roundtrip(src, 253);
  assert_roundtrip(src, 254);
  assert_roundtrip(src, 255);
}

void test_cobs_max_payload(void) {
  uint8_t src[900];
  for (size_t i = 0; i < sizeof(src); i++) {
    src[i] = (uint8_t)(i * 7);
  }
  assert_roundtrip(src, sizeof(src));
}

void test_cobs_encode_rejects_small_dst(void) {
  const uint8_t src[] = {0x11, 0x22, 0x33};
  uint8_t dst[2];
  TEST_ASSERT_EQUAL(0, lf_cobs_encode(src, sizeof(src), dst, sizeof(dst)));
}

void test_cobs_decode_rejects_truncated(void) {
  // A code byte promising 5 bytes but only 2 follow.
  const uint8_t bad[] = {0x06, 0x11, 0x22};
  uint8_t dst[16];
  TEST_ASSERT_EQUAL(0, lf_cobs_decode(bad, sizeof(bad), dst, sizeof(dst)));
}

void test_cobs_decode_rejects_embedded_zero(void) {
  const uint8_t bad[] = {0x03, 0x11, 0x00};
  uint8_t dst[16];
  TEST_ASSERT_EQUAL(0, lf_cobs_decode(bad, sizeof(bad), dst, sizeof(dst)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_cobs_single_nonzero);
  RUN_TEST(test_cobs_single_zero);
  RUN_TEST(test_cobs_all_zeros);
  RUN_TEST(test_cobs_no_zeros);
  RUN_TEST(test_cobs_mixed);
  RUN_TEST(test_cobs_known_answers);
  RUN_TEST(test_cobs_254_multiple_is_canonical);
  RUN_TEST(test_cobs_254_boundary);
  RUN_TEST(test_cobs_max_payload);
  RUN_TEST(test_cobs_encode_rejects_small_dst);
  RUN_TEST(test_cobs_decode_rejects_truncated);
  RUN_TEST(test_cobs_decode_rejects_embedded_zero);
  return UNITY_END();
}
