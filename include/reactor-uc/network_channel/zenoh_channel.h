#ifndef REACTOR_UC_ZENOH_CHANNEL_H
#define REACTOR_UC_ZENOH_CHANNEL_H

#include <zenoh-pico.h>

#include "reactor-uc/network_channel.h"

/**
 * A NetworkChannel on top of Zenoh publish/subscribe (zenoh-pico).
 *
 * Two federates exchange their messages through two keys, one per direction.
 * There is no server side, no port and no address of the peer: a federate only
 * needs a Zenoh session, for example one that connects out to a Zenoh router.
 * The channel therefore also works where the federates cannot reach each other
 * directly.
 *
 * zenoh-pico has to be built with Z_FEATURE_MATCHING, which the channel uses to
 * tell whether the other federate is there.
 */

#define ZENOH_CHANNEL_BUFFERSIZE 1024
#define ZENOH_CHANNEL_KEYSIZE 96
#define ZENOH_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(100)

typedef struct ZenohChannel ZenohChannel;
typedef struct ZenohChannelSession ZenohChannelSession;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

/**
 * @brief The Zenoh session of a federate, shared by all its ZenohChannels.
 *
 * There are three ways to get one:
 * - @ref ZenohChannelSession_open opens it at once.
 * - @ref ZenohChannelSession_configure only states how to open it. The channels
 *   then open it when they are opened, and try again until it works, which
 *   suits a board whose network comes up later.
 * - @ref ZenohChannelSession_borrow uses the session of an application that
 *   already has one.
 */
struct ZenohChannelSession {
  z_owned_session_t owned;
  const z_loaned_session_t* session; // NULL until opened or borrowed.
  bool is_owned;

  // From ZenohChannelSession_configure.
  bool is_configured;
  const char* mode;
  const char* connect;
  const char* listen;
  int open_channels; // Channels that are open on a session they opened themselves.
};

/**
 * @brief The session that code generated from `@interface_zenoh` uses: one per federate.
 */
ZenohChannelSession* ZenohChannel_default_session(void);

/**
 * @brief Open a Zenoh session for the channels of this federate.
 *
 * @param mode "client" or "peer".
 * @param connect Locator to connect to, e.g. "tcp/192.168.1.2:7447", or NULL.
 * @param listen Locator to listen on (peer mode), or NULL.
 * @return LF_OK if the session is open.
 */
lf_ret_t ZenohChannelSession_open(ZenohChannelSession* self, const char* mode, const char* connect, const char* listen);

/**
 * @brief State how to open the session, without opening it yet.
 *
 * The channels on this session open it when they are opened, and close it when
 * the last of them is closed. Has no effect on a session that is already open
 * or borrowed. The strings have to stay valid.
 *
 * @param mode "client" or "peer", or NULL for the default of zenoh-pico.
 * @param connect Locator to connect to, or NULL.
 * @param listen Locator to listen on (peer mode), or NULL.
 */
void ZenohChannelSession_configure(ZenohChannelSession* self, const char* mode, const char* connect,
                                   const char* listen);

/**
 * @brief Use a session that the application has opened and keeps open for as long as the channels exist.
 */
void ZenohChannelSession_borrow(ZenohChannelSession* self, const z_loaned_session_t* session);

/**
 * @brief Close a session opened with @ref ZenohChannelSession_open. A borrowed session is left alone.
 */
void ZenohChannelSession_close(ZenohChannelSession* self);

struct ZenohChannel {
  NetworkChannel super;

  ZenohChannelSession* session;
  char publish_key[ZENOH_CHANNEL_KEYSIZE];
  char subscribe_key[ZENOH_CHANNEL_KEYSIZE];
  z_owned_publisher_t publisher;
  z_owned_subscriber_t subscriber;
  z_owned_mutex_t mutex;
  bool is_open;
  bool wants_open;             // open_connection was called; is_connected retries until the session opens.
  bool counts_as_open_channel; // Counted in the session's open_channels.

  FederateMessage output;
  unsigned char write_buffer[ZENOH_CHANNEL_BUFFERSIZE];
  unsigned char read_buffer[ZENOH_CHANNEL_BUFFERSIZE];

  FederatedConnectionBundle* federated_connection;
  void (*receive_callback)(FederatedConnectionBundle* conn, const FederateMessage* message);
};

/**
 * @brief Construct a channel between the federate @p local and the federate @p remote.
 *
 * This side publishes on `<prefix>/<local>/<remote>` and subscribes to
 * `<prefix>/<remote>/<local>`; the other side is constructed with the two
 * names swapped. The channel is connected once the other side's subscriber is
 * known to this session.
 *
 * @param session The session of this federate. It has to be open, borrowed or configured before open_connection
 *                is called.
 * @param prefix Key prefix shared by the federation, e.g. "lf/my_federation".
 * @param local Name of this federate.
 * @param remote Name of the federate on the other side.
 */
void ZenohChannel_ctor(ZenohChannel* self, ZenohChannelSession* session, const char* prefix, const char* local,
                       const char* remote);

#endif
