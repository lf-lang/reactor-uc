#include "reactor-uc/network_channel/frame.h"
#include "unity.h"
#include <string.h>

// Known-answer vectors for CRC-32/ISO-HDLC (a.k.a. IEEE 802.3, zlib crc32).
void test_crc32_empty(void) { TEST_ASSERT_EQUAL_HEX32(0x00000000, crc32((const uint8_t*)"", 0)); }

void test_crc32_check_vector(void) {
  // The standard "check" value: CRC32("123456789") == 0xCBF43926
  const char* s = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc32((const uint8_t*)s, strlen(s)));
}

void test_crc32_single_byte(void) {
  const uint8_t b = 0x00;
  TEST_ASSERT_EQUAL_HEX32(0xD202EF8D, crc32(&b, 1));
}

void test_crc32_detects_single_bit_flip(void) {
  uint8_t a[16];
  uint8_t b[16];
  memset(a, 0xA5, sizeof(a));
  memcpy(b, a, sizeof(a));
  b[7] ^= 0x01;
  TEST_ASSERT_NOT_EQUAL(crc32(a, sizeof(a)), crc32(b, sizeof(b)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_empty);
  RUN_TEST(test_crc32_check_vector);
  RUN_TEST(test_crc32_single_byte);
  RUN_TEST(test_crc32_detects_single_bit_flip);
  return UNITY_END();
}