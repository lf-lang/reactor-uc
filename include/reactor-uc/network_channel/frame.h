#ifndef REACTOR_UC_NETWORK_CHANNEL_FRAME_H
#define REACTOR_UC_NETWORK_CHANNEL_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief CRC-32/ISO-HDLC (IEEE 802.3, same as zlib lf_crc32).
 *
 * Reflected, polynomial 0xEDB88320, init 0xFFFFFFFF, final XOR 0xFFFFFFFF.
 * CRC32("123456789") == 0xCBF43926.
 */
uint32_t lf_crc32(const uint8_t* data, size_t len);

// Worst-case COBS expansion: one overhead byte, plus one per 254 payload bytes.
#define LF_COBS_MAX_ENCODED(n) ((n) + (n) / 254 + 1)

/**
 * @brief COBS-encode `src` into `dst`.
 *
 * The output contains no zero bytes, which is what allows 0x00 to delimit
 * frames unambiguously. The delimiter itself is NOT appended.
 *
 * @return bytes written to dst, or 0 if dst_cap is too small.
 */
size_t lf_cobs_encode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_cap);

/**
 * @brief COBS-decode `src` (which must not contain the 0x00 delimiter).
 *
 * @return bytes written to dst, or 0 on malformed input or insufficient capacity.
 */
size_t lf_cobs_decode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_cap);

/** Largest decoded payload a frame may carry. Must cover a serialized
 * FederateMessage.
 */
#define LF_FRAME_MAX_PAYLOAD 1024

/** Receive buffer: worst-case COBS expansion of the largest payload plus its
 * CRC.
 */
#define LF_FRAME_BUFFER_SIZE LF_COBS_MAX_ENCODED(LF_FRAME_MAX_PAYLOAD + 4)

#define LF_FRAME_DELIMITER 0x00

/** A complete frame: the COBS maximum plus the trailing 0x00 delimiter.
 *  Size every TRANSMIT buffer with this, never with LF_FRAME_BUFFER_SIZE. */
#define LF_FRAME_MAX_FRAME_SIZE (LF_FRAME_BUFFER_SIZE + 1)

typedef enum {
  LF_FRAME_NEED_MORE = 0,    // Byte consumed, no frame completed.
  LF_FRAME_OK = 1,           // A valid frame was decoded into `out`.
  LF_FRAME_ERR_CRC = -1,     // Frame completed but CRC mismatched.
  LF_FRAME_ERR_DECODE = -2,  // COBS decode failed.
  LF_FRAME_ERR_OVERFLOW = -3 // Frame exceeded the buffer
} LfFrameStatus;

/**
 * @brief Receive state machine for COBS+CRC32 frames.
 *
 * Every write is bounds-checked. On overflow the receiver enters a discarding
 * state and resumes at the next 0x00 delimiter, so a corruption or overflow
 * event costs at most one frame and can never write past `buf`.
 */
typedef struct {
  uint8_t buf[LF_FRAME_BUFFER_SIZE];
  size_t idx;
  bool discarding;
  uint32_t stat_frames_ok;
  uint32_t stat_crc_error;
  uint32_t stat_decode_error;
  uint32_t stat_overflow;
} LfFrameReceiver;

void lf_frame_receiver_init(LfFrameReceiver* self);

/**
 * @brief Encode one frame: COBS(payload || crc32_le) || 0x00.
 * @return total bytes written including the trailing delimiter, or 0 on error.
 */
size_t lf_frame_encode(const uint8_t* payload, size_t payload_len, uint8_t* dst, size_t dst_cap);

/**
 * @brief Feed one received byte.
 *
 * On LF_FRAME_OK, `out` holds the payload and `*out_len` its length.
 */
LfFrameStatus lf_frame_receiver_push(LfFrameReceiver* self, uint8_t byte, uint8_t* out, size_t out_cap,
                                     size_t* out_len);

#endif // REACTOR_UC_NETWORK_CHANNEL_FRAME_H