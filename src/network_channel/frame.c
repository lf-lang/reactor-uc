#include "reactor-uc/network_channel/frame.h"
#include <string.h>

/** Bitwise implementation: no 1 KiB table, which matters on target platforms
 *  where RAM is the binding constraint.
 */
uint32_t lf_crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = -(crc & 1U);
      crc = (crc >> 1) ^ (0xEDB88320U & mask);
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

size_t lf_cobs_encode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_cap) {
  if (dst_cap < LF_COBS_MAX_ENCODED(src_len)) {
    return 0;
  }

  size_t read_idx = 0;
  size_t write_idx = 1; // dst[0] is the first code byte, written when the block closes.
  size_t code_idx = 0;
  uint8_t code = 1;

  while (read_idx < src_len) {
    if (src[read_idx] == 0) {
      dst[code_idx] = code;
      code_idx = write_idx++;
      code = 1;
      read_idx++;
    } else {
      dst[write_idx++] = src[read_idx++];
      code++;
      if (code == 0xFF && read_idx < src_len) {
        // Block is full at 254 data bytes AND more input remains: close it and
        // start a new one. The `read_idx < src_len` guard is what keeps the
        // encoding canonical.
        dst[code_idx] = code;
        code_idx = write_idx++;
        code = 1;
      }
    }
  }
  dst[code_idx] = code;
  return write_idx;
}

size_t lf_cobs_decode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_cap) {
  size_t read_idx = 0;
  size_t write_idx = 0;

  while (read_idx < src_len) {
    const uint8_t code = src[read_idx];
    if (code == 0) {
      return 0; // A zero byte cannot appear inside a COBS frame.
    }
    read_idx++;

    // The code byte promises code-1 literal bytes follow.
    for (uint8_t i = 1; i < code; i++) {
      if (read_idx >= src_len) {
        return 0; // Truncated -> the frame ended mid-block.
      }
      if (src[read_idx] == 0) {
        return 0; // Embedded zero -> malformed.
      }
      if (write_idx >= dst_cap) {
        return 0; // Would overflow the caller's buffer.
      }
      dst[write_idx++] = src[read_idx++];
    }

    // A block shorter than 0xFF represents a zero byte, except at end of input.
    if (code != 0xFF && read_idx < src_len) {
      if (write_idx >= dst_cap) {
        return 0;
      }
      dst[write_idx++] = 0;
    }
  }
  return write_idx;
}

void lf_frame_receiver_init(LfFrameReceiver* self) { memset(self, 0, sizeof(*self)); }

size_t lf_frame_encode(const uint8_t* payload, size_t payload_len, uint8_t* dst, size_t dst_cap) {
  if (payload_len > LF_FRAME_MAX_PAYLOAD) {
    return 0;
  }
  // Stage payload || lf_crc32 so COBS sees one contiguous block.
  uint8_t staging[LF_FRAME_MAX_PAYLOAD + 4];
  memcpy(staging, payload, payload_len);
  const uint32_t crc = lf_crc32(payload, payload_len);
  staging[payload_len + 0] = (uint8_t)(crc & 0xFFU);
  staging[payload_len + 1] = (uint8_t)((crc >> 8) & 0xFFU);
  staging[payload_len + 2] = (uint8_t)((crc >> 16) & 0xFFU);
  staging[payload_len + 3] = (uint8_t)((crc >> 24) & 0xFFU);

  const size_t staged_len = payload_len + 4;
  if (dst_cap < LF_COBS_MAX_ENCODED(staged_len) + 1) {
    return 0; // +1 for the delimiter
  }
  const size_t enc_len = lf_cobs_encode(staging, staged_len, dst, dst_cap - 1);
  if (enc_len == 0) {
    return 0;
  }
  dst[enc_len] = LF_FRAME_DELIMITER;
  return enc_len + 1;
}

LfFrameStatus lf_frame_receiver_push(LfFrameReceiver* self, uint8_t byte, uint8_t* out, size_t out_cap,
                                     size_t* out_len) {
  if (byte != LF_FRAME_DELIMITER) {
    if (self->discarding) {
      return LF_FRAME_NEED_MORE; // still hunting for the next delimiter
    }
    if (self->idx >= LF_FRAME_BUFFER_SIZE) {
      // Bounds check. Enter the discarding state rather than wrapping or
      // truncating: the frame is unrecoverable.
      self->discarding = true;
      self->idx = 0;
      self->stat_overflow++;
      return LF_FRAME_ERR_OVERFLOW;
    }
    self->buf[self->idx++] = byte;
    return LF_FRAME_NEED_MORE;
  }

  // Delimiter reached: close whatever we have.
  if (self->discarding) {
    self->discarding = false;
    self->idx = 0;
    return LF_FRAME_NEED_MORE;
  }
  if (self->idx == 0) {
    return LF_FRAME_NEED_MORE; // no data to decode
  }

  uint8_t decoded[LF_FRAME_MAX_PAYLOAD + 4];
  const size_t decoded_len = lf_cobs_decode(self->buf, self->idx, decoded, sizeof(decoded));
  self->idx = 0;

  if (decoded_len < 5) { // need at least 1 payload byte + 4 CRC bytes
    self->stat_decode_error++;
    return LF_FRAME_ERR_DECODE;
  }

  const size_t payload_len = decoded_len - 4;
  const uint32_t expected = (uint32_t)decoded[payload_len] | ((uint32_t)decoded[payload_len + 1] << 8) |
                            ((uint32_t)decoded[payload_len + 2] << 16) | ((uint32_t)decoded[payload_len + 3] << 24);
  if (lf_crc32(decoded, payload_len) != expected) {
    self->stat_crc_error++;
    return LF_FRAME_ERR_CRC;
  }
  if (payload_len > out_cap) {
    self->stat_decode_error++;
    return LF_FRAME_ERR_DECODE;
  }

  memcpy(out, decoded, payload_len);
  *out_len = payload_len;
  self->stat_frames_ok++;
  return LF_FRAME_OK;
}
