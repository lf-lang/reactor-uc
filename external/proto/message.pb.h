/* Automatically generated nanopb header */
/* Generated by nanopb-1.0.0-dev */

#ifndef PB_MESSAGE_PB_H_INCLUDED
#define PB_MESSAGE_PB_H_INCLUDED
#include "nanopb/pb.h"

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
typedef enum _FederateState {
    FederateState_INITIALIZING = 0,
    FederateState_NEGOTIATING = 1,
    FederateState_RUNNING = 2
} FederateState;

/* Struct definitions */
typedef struct _Tag {
    int64_t time;
    uint32_t microstep;
} Tag;

typedef PB_BYTES_ARRAY_T(832) TaggedMessage_payload_t;
typedef struct _TaggedMessage {
    Tag tag;
    int32_t conn_id;
    TaggedMessage_payload_t payload;
} TaggedMessage;

typedef struct _StartupHandshakeRequest {
    char dummy_field;
} StartupHandshakeRequest;

typedef struct _StartupHandshakeResponse {
    FederateState state;
} StartupHandshakeResponse;

typedef struct _StartTimeProposal {
    int64_t time;
    uint32_t step;
} StartTimeProposal;

typedef struct _StartTimeResponse {
    int64_t time;
} StartTimeResponse;

typedef struct _StartTimeRequest {
    char dummy_field;
} StartTimeRequest;

typedef struct _StartupCoordination {
    pb_size_t which_message;
    union {
        StartupHandshakeRequest startup_handshake_request;
        StartupHandshakeResponse startup_handshake_response;
        StartTimeProposal start_time_proposal;
        StartTimeResponse start_time_response;
        StartTimeRequest start_time_request;
    } message;
} StartupCoordination;

typedef struct _FederateMessage {
    pb_size_t which_message;
    union {
        TaggedMessage tagged_message;
        StartupCoordination startup_coordination;
    } message;
} FederateMessage;


#ifdef __cplusplus
extern "C" {
#endif

/* Helper constants for enums */
#define _FederateState_MIN FederateState_INITIALIZING
#define _FederateState_MAX FederateState_RUNNING
#define _FederateState_ARRAYSIZE ((FederateState)(FederateState_RUNNING+1))




#define StartupHandshakeResponse_state_ENUMTYPE FederateState







/* Initializer values for message structs */
#define Tag_init_default                         {0, 0}
#define TaggedMessage_init_default               {Tag_init_default, 0, {0, {0}}}
#define StartupHandshakeRequest_init_default     {0}
#define StartupHandshakeResponse_init_default    {_FederateState_MIN}
#define StartTimeProposal_init_default           {0, 0}
#define StartTimeResponse_init_default           {0}
#define StartTimeRequest_init_default            {0}
#define StartupCoordination_init_default         {0, {StartupHandshakeRequest_init_default}}
#define FederateMessage_init_default             {0, {TaggedMessage_init_default}}
#define Tag_init_zero                            {0, 0}
#define TaggedMessage_init_zero                  {Tag_init_zero, 0, {0, {0}}}
#define StartupHandshakeRequest_init_zero        {0}
#define StartupHandshakeResponse_init_zero       {_FederateState_MIN}
#define StartTimeProposal_init_zero              {0, 0}
#define StartTimeResponse_init_zero              {0}
#define StartTimeRequest_init_zero               {0}
#define StartupCoordination_init_zero            {0, {StartupHandshakeRequest_init_zero}}
#define FederateMessage_init_zero                {0, {TaggedMessage_init_zero}}

/* Field tags (for use in manual encoding/decoding) */
#define Tag_time_tag                             1
#define Tag_microstep_tag                        2
#define TaggedMessage_tag_tag                    1
#define TaggedMessage_conn_id_tag                2
#define TaggedMessage_payload_tag                3
#define StartupHandshakeResponse_state_tag       1
#define StartTimeProposal_time_tag               1
#define StartTimeProposal_step_tag               2
#define StartTimeResponse_time_tag               1
#define StartupCoordination_startup_handshake_request_tag 1
#define StartupCoordination_startup_handshake_response_tag 2
#define StartupCoordination_start_time_proposal_tag 3
#define StartupCoordination_start_time_response_tag 4
#define StartupCoordination_start_time_request_tag 5
#define FederateMessage_tagged_message_tag       2
#define FederateMessage_startup_coordination_tag 3

/* Struct field encoding specification for nanopb */
#define Tag_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, INT64,    time,              1) \
X(a, STATIC,   REQUIRED, UINT32,   microstep,         2)
#define Tag_CALLBACK NULL
#define Tag_DEFAULT NULL

#define TaggedMessage_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, MESSAGE,  tag,               1) \
X(a, STATIC,   REQUIRED, INT32,    conn_id,           2) \
X(a, STATIC,   REQUIRED, BYTES,    payload,           3)
#define TaggedMessage_CALLBACK NULL
#define TaggedMessage_DEFAULT NULL
#define TaggedMessage_tag_MSGTYPE Tag

#define StartupHandshakeRequest_FIELDLIST(X, a) \

#define StartupHandshakeRequest_CALLBACK NULL
#define StartupHandshakeRequest_DEFAULT NULL

#define StartupHandshakeResponse_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, UENUM,    state,             1)
#define StartupHandshakeResponse_CALLBACK NULL
#define StartupHandshakeResponse_DEFAULT NULL

#define StartTimeProposal_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, INT64,    time,              1) \
X(a, STATIC,   REQUIRED, UINT32,   step,              2)
#define StartTimeProposal_CALLBACK NULL
#define StartTimeProposal_DEFAULT NULL

#define StartTimeResponse_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, INT64,    time,              1)
#define StartTimeResponse_CALLBACK NULL
#define StartTimeResponse_DEFAULT NULL

#define StartTimeRequest_FIELDLIST(X, a) \

#define StartTimeRequest_CALLBACK NULL
#define StartTimeRequest_DEFAULT NULL

#define StartupCoordination_FIELDLIST(X, a) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,startup_handshake_request,message.startup_handshake_request),   1) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,startup_handshake_response,message.startup_handshake_response),   2) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,start_time_proposal,message.start_time_proposal),   3) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,start_time_response,message.start_time_response),   4) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,start_time_request,message.start_time_request),   5)
#define StartupCoordination_CALLBACK NULL
#define StartupCoordination_DEFAULT NULL
#define StartupCoordination_message_startup_handshake_request_MSGTYPE StartupHandshakeRequest
#define StartupCoordination_message_startup_handshake_response_MSGTYPE StartupHandshakeResponse
#define StartupCoordination_message_start_time_proposal_MSGTYPE StartTimeProposal
#define StartupCoordination_message_start_time_response_MSGTYPE StartTimeResponse
#define StartupCoordination_message_start_time_request_MSGTYPE StartTimeRequest

#define FederateMessage_FIELDLIST(X, a) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,tagged_message,message.tagged_message),   2) \
X(a, STATIC,   ONEOF,    MESSAGE,  (message,startup_coordination,message.startup_coordination),   3)
#define FederateMessage_CALLBACK NULL
#define FederateMessage_DEFAULT NULL
#define FederateMessage_message_tagged_message_MSGTYPE TaggedMessage
#define FederateMessage_message_startup_coordination_MSGTYPE StartupCoordination

extern const pb_msgdesc_t Tag_msg;
extern const pb_msgdesc_t TaggedMessage_msg;
extern const pb_msgdesc_t StartupHandshakeRequest_msg;
extern const pb_msgdesc_t StartupHandshakeResponse_msg;
extern const pb_msgdesc_t StartTimeProposal_msg;
extern const pb_msgdesc_t StartTimeResponse_msg;
extern const pb_msgdesc_t StartTimeRequest_msg;
extern const pb_msgdesc_t StartupCoordination_msg;
extern const pb_msgdesc_t FederateMessage_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define Tag_fields &Tag_msg
#define TaggedMessage_fields &TaggedMessage_msg
#define StartupHandshakeRequest_fields &StartupHandshakeRequest_msg
#define StartupHandshakeResponse_fields &StartupHandshakeResponse_msg
#define StartTimeProposal_fields &StartTimeProposal_msg
#define StartTimeResponse_fields &StartTimeResponse_msg
#define StartTimeRequest_fields &StartTimeRequest_msg
#define StartupCoordination_fields &StartupCoordination_msg
#define FederateMessage_fields &FederateMessage_msg

/* Maximum encoded size of messages (where known) */
#define FederateMessage_size                     868
#define MESSAGE_PB_H_MAX_SIZE                    FederateMessage_size
#define StartTimeProposal_size                   17
#define StartTimeRequest_size                    0
#define StartTimeResponse_size                   11
#define StartupCoordination_size                 19
#define StartupHandshakeRequest_size             0
#define StartupHandshakeResponse_size            2
#define Tag_size                                 17
#define TaggedMessage_size                       865

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
