#include "reactor-uc/network_channel/frame.h"
#include "unity.h"
#include <string.h>

static FrameReceiver rx;

void setUp(void) { frame_receiver_init(&rx); }

// Feed every byte of `buf` except the last, asserting none completes a frame,
// then feed the last byte and return its status.
static FrameStatus feed(const uint8_t* buf, size_t len, uint8_t* out, size_t out_cap, size_t* out_len) {
  FrameStatus st = FRAME_NEED_MORE;
  for (size_t i = 0; i < len; i++) {
    st = frame_receiver_push(&rx, buf[i], out, out_cap, out_len);
  }
  return st;
}

void test_encode_decode_roundtrip(void) {
  const uint8_t payload[] = {0x01, 0x00, 0x02, 0xFF, 0x00};
  uint8_t frame[64];
  uint8_t out[64];
  size_t out_len = 0;

  size_t n = frame_encode(payload, sizeof(payload), frame, sizeof(frame));
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL_HEX8(0x00, frame[n - 1]); // delimiter is last

  TEST_ASSERT_EQUAL(FRAME_OK, feed(frame, n, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL(sizeof(payload), out_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, sizeof(payload));
  TEST_ASSERT_EQUAL(1, rx.stat_frames_ok);
  TEST_ASSERT_EQUAL(0, rx.stat_crc_error);
  TEST_ASSERT_EQUAL(0, rx.stat_decode_error);
  TEST_ASSERT_EQUAL(0, rx.stat_overflow);
}

void test_back_to_back_frames(void) {
  const uint8_t a[] = {0xAA, 0xBB};
  const uint8_t b[] = {0xCC};
  uint8_t frame[64];
  uint8_t out[64];
  size_t out_len = 0;

  size_t na = frame_encode(a, sizeof(a), frame, sizeof(frame));
  TEST_ASSERT_EQUAL(FRAME_OK, feed(frame, na, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(a, out, sizeof(a));

  size_t nb = frame_encode(b, sizeof(b), frame, sizeof(frame));
  TEST_ASSERT_EQUAL(FRAME_OK, feed(frame, nb, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL(sizeof(b), out_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(b, out, sizeof(b));
  TEST_ASSERT_EQUAL(2, rx.stat_frames_ok);
  TEST_ASSERT_EQUAL(0, rx.stat_crc_error);
  TEST_ASSERT_EQUAL(0, rx.stat_decode_error);
  TEST_ASSERT_EQUAL(0, rx.stat_overflow);
}

void test_crc_error_is_detected(void) {
  const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
  uint8_t frame[64];
  uint8_t out[64];
  size_t out_len = 0;

  size_t n = frame_encode(payload, sizeof(payload), frame, sizeof(frame));
  frame[2] ^= 0x01; // corrupt a payload byte, keeping it non-zero
  TEST_ASSERT_EQUAL(FRAME_ERR_CRC, feed(frame, n, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL(1, rx.stat_crc_error);
  TEST_ASSERT_EQUAL(0, rx.stat_frames_ok);
  TEST_ASSERT_EQUAL(0, rx.stat_decode_error);
  TEST_ASSERT_EQUAL(0, rx.stat_overflow);
}

// The property that matters most: one corruption event costs exactly one frame.
void test_resync_after_dropped_byte(void) {
  const uint8_t payload[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t frame[64];
  uint8_t out[64];
  size_t out_len = 0;

  size_t n = frame_encode(payload, sizeof(payload), frame, sizeof(frame));

  // Feed a damaged first frame: drop byte 2 but keep the delimiter.
  for (size_t i = 0; i < n; i++) {
    if (i == 2) {
      continue;
    }
    frame_receiver_push(&rx, frame[i], out, sizeof(out), &out_len);
  }

  // The very next intact frame must be accepted.
  TEST_ASSERT_EQUAL(FRAME_OK, feed(frame, n, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL(sizeof(payload), out_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, sizeof(payload));
  // The damaged frame must NOT have counted as accepted.
  TEST_ASSERT_EQUAL(1, rx.stat_frames_ok);
  TEST_ASSERT_EQUAL(0, rx.stat_crc_error);
  TEST_ASSERT_EQUAL(1, rx.stat_decode_error);
  TEST_ASSERT_EQUAL(0, rx.stat_overflow);
}

// Garbage with no delimiter must never write past the buffer, and must recover.
void test_overflow_is_bounded_and_recovers(void) {
  uint8_t out[1200];
  size_t out_len = 0;
  FrameStatus st = FRAME_NEED_MORE;

  // Twice the buffer's worth of non-zero noise.
  for (size_t i = 0; i < FRAME_BUFFER_SIZE * 2; i++) {
    st = frame_receiver_push(&rx, 0x5A, out, sizeof(out), &out_len);
    if (st == FRAME_ERR_OVERFLOW) {
      break;
    }
  }
  TEST_ASSERT_EQUAL(FRAME_ERR_OVERFLOW, st);
  TEST_ASSERT_GREATER_THAN(0, rx.stat_overflow);

  // Keep feeding noise, then a delimiter to close the junk, then a good frame.
  for (int i = 0; i < 50; i++) {
    frame_receiver_push(&rx, 0x5A, out, sizeof(out), &out_len);
  }
  frame_receiver_push(&rx, 0x00, out, sizeof(out), &out_len);

  const uint8_t payload[] = {0x77, 0x88};
  uint8_t frame[64];
  size_t n = frame_encode(payload, sizeof(payload), frame, sizeof(frame));
  TEST_ASSERT_EQUAL(FRAME_OK, feed(frame, n, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, sizeof(payload));
}

void test_empty_frame_is_ignored(void) {
  uint8_t out[64];
  size_t out_len = 0;
  // A lone delimiter (idle line, or the tail of a previous frame) is not an
  // error.
  TEST_ASSERT_EQUAL(FRAME_NEED_MORE, frame_receiver_push(&rx, 0x00, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL(0, rx.stat_crc_error);
  TEST_ASSERT_EQUAL(0, rx.stat_frames_ok);
}

void test_max_size_payload(void) {
  uint8_t payload[832];
  uint8_t frame[FRAME_BUFFER_SIZE];
  uint8_t out[900];
  size_t out_len = 0;
  for (size_t i = 0; i < sizeof(payload); i++) {
    payload[i] = (uint8_t)(i * 13);
  }
  size_t n = frame_encode(payload, sizeof(payload), frame, sizeof(frame));
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL(FRAME_OK, feed(frame, n, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL(sizeof(payload), out_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, sizeof(payload));
}

// FRAME_MAX_PAYLOAD (1024) is the largest payload the receiver accepts.
// This case would need FRAME_BUFFER_SIZE (1033) and `decoded_len` reaches
// exactly FRAME_MAX_PAYLOAD + 4 (1028).
void test_max_payload(void) {
  uint8_t payload[FRAME_MAX_PAYLOAD];
  uint8_t frame[FRAME_MAX_FRAME_SIZE];
  uint8_t out[FRAME_MAX_PAYLOAD];
  size_t out_len = 0;
  for (size_t i = 0; i < sizeof(payload); i++) {
    payload[i] = (uint8_t)(i * 13);
  }
  size_t n = frame_encode(payload, sizeof(payload), frame, sizeof(frame));
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL(FRAME_OK, feed(frame, n, out, sizeof(out), &out_len));
  TEST_ASSERT_EQUAL(sizeof(payload), out_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, sizeof(payload));
}

void test_encode_rejects_small_dst(void) {
  const uint8_t payload[] = {0x01, 0x02, 0x03};
  uint8_t dst[4];
  TEST_ASSERT_EQUAL(0, frame_encode(payload, sizeof(payload), dst, sizeof(dst)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_encode_decode_roundtrip);
  RUN_TEST(test_back_to_back_frames);
  RUN_TEST(test_crc_error_is_detected);
  RUN_TEST(test_resync_after_dropped_byte);
  RUN_TEST(test_overflow_is_bounded_and_recovers);
  RUN_TEST(test_empty_frame_is_ignored);
  RUN_TEST(test_max_size_payload);
  RUN_TEST(test_max_payload);
  RUN_TEST(test_encode_rejects_small_dst);
  return UNITY_END();
}