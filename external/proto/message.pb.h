/* Automatically generated nanopb header */
/* Generated by nanopb-1.0.0-dev */

#ifndef PB_MESSAGE_PB_H_INCLUDED
#define PB_MESSAGE_PB_H_INCLUDED
#include "nanopb/pb.h"

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
typedef enum _MessageType {
    MessageType_TAGGED_MESSAGE = 1,
    MessageType_START_TAG_SIGNAL = 2
} MessageType;

/* Struct definitions */
typedef struct _Tag {
    int64_t time;
    uint32_t microstep;
} Tag;

typedef struct _StartTagSignal {
    Tag tag;
} StartTagSignal;

typedef PB_BYTES_ARRAY_T(832) TaggedMessage_payload_t;
typedef struct _TaggedMessage {
    Tag tag;
    int32_t conn_id;
    TaggedMessage_payload_t payload;
} TaggedMessage;

typedef struct _FederateMessage {
    MessageType type;
    pb_size_t which_message;
    union {
        TaggedMessage tagged_message;
        StartTagSignal start_tag_signal;
    } message;
} FederateMessage;


#ifdef __cplusplus
extern "C" {
#endif

/* Helper constants for enums */
#define _MessageType_MIN MessageType_TAGGED_MESSAGE
#define _MessageType_MAX MessageType_START_TAG_SIGNAL
#define _MessageType_ARRAYSIZE ((MessageType)(MessageType_START_TAG_SIGNAL+1))




#define FederateMessage_type_ENUMTYPE MessageType


/* Initializer values for message structs */
#define Tag_init_default                         {0, 0}
#define StartTagSignal_init_default              {Tag_init_default}
#define TaggedMessage_init_default               {Tag_init_default, 0, {0, {0}}}
#define FederateMessage_init_default             {_MessageType_MIN, 0, {TaggedMessage_init_default}}
#define Tag_init_zero                            {0, 0}
#define StartTagSignal_init_zero                 {Tag_init_zero}
#define TaggedMessage_init_zero                  {Tag_init_zero, 0, {0, {0}}}
#define FederateMessage_init_zero                {_MessageType_MIN, 0, {TaggedMessage_init_zero}}

/* Field tags (for use in manual encoding/decoding) */
#define Tag_time_tag                             1
#define Tag_microstep_tag                        2
#define StartTagSignal_tag_tag                   1
#define TaggedMessage_tag_tag                    1
#define TaggedMessage_conn_id_tag                2
#define TaggedMessage_payload_tag                3
#define FederateMessage_type_tag                 1
#define FederateMessage_tagged_message_tag       2
#define FederateMessage_start_tag_signal_tag     3

/* Struct field encoding specification for nanopb */
#define Tag_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, INT64,    time,              1) \
X(a, STATIC,   REQUIRED, UINT32,   microstep,         2)
#define Tag_CALLBACK NULL
#define Tag_DEFAULT NULL

#define StartTagSignal_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, MESSAGE,  tag,               1)
#define StartTagSignal_CALLBACK NULL
#define StartTagSignal_DEFAULT NULL
#define StartTagSignal_tag_MSGTYPE Tag

#define TaggedMessage_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, MESSAGE,  tag,               1) \
X(a, STATIC,   REQUIRED, INT32,    conn_id,           2) \
X(a, STATIC,   REQUIRED, BYTES,    payload,           3)
#define TaggedMessage_CALLBACK NULL
#define TaggedMessage_DEFAULT NULL
#define TaggedMessage_tag_MSGTYPE Tag

#define FederateMessage_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, UENUM,    type,              1) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,tagged_message,message.tagged_message),   2) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,start_tag_signal,message.start_tag_signal),   3)
#define FederateMessage_CALLBACK NULL
#define FederateMessage_DEFAULT (const pb_byte_t*)"\x08\x01\x00"
#define FederateMessage_message_tagged_message_MSGTYPE TaggedMessage
#define FederateMessage_message_start_tag_signal_MSGTYPE StartTagSignal

extern const pb_msgdesc_t Tag_msg;
extern const pb_msgdesc_t StartTagSignal_msg;
extern const pb_msgdesc_t TaggedMessage_msg;
extern const pb_msgdesc_t FederateMessage_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define Tag_fields &Tag_msg
#define StartTagSignal_fields &StartTagSignal_msg
#define TaggedMessage_fields &TaggedMessage_msg
#define FederateMessage_fields &FederateMessage_msg

/* Maximum encoded size of messages (where known) */
#define FederateMessage_size                     870
#define MESSAGE_PB_H_MAX_SIZE                    FederateMessage_size
#define StartTagSignal_size                      19
#define Tag_size                                 17
#define TaggedMessage_size                       865

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
