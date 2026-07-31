#ifndef REACTOR_UC_ZEPHYR_BLE_CHANNEL_H
#define REACTOR_UC_ZEPHYR_BLE_CHANNEL_H

/**
 * @brief A Bluetooth Low Energy NetworkChannel for federated Reactor-UC on
 * Zephyr / Nordic (nRF5x) boards.
 *
 * One channel == one BLE connection between exactly two boards:
 *   - one board is the PERIPHERAL: it advertises under `device_name` and hosts a
 *     small GATT service (NUS-compatible UUIDs), then waits for a central;
 *   - the other board is the CENTRAL: it scans for `device_name`, connects,
 *     discovers the service, subscribes to notifications and writes to the RX
 *     characteristic.
 *
 * MULTIPLE LINKS PER BOARD: several channels may coexist. A CENTRAL board (e.g.
 * a fusion federate) can hold one channel per upstream PERIPHERAL, each with its
 * own `device_name` and connection parameters (not tested yet).
 * The runtime keeps a registry and routes every BLE event to the owning
 * channel. (A single board should host at most one PERIPHERAL channel,
 * because legacy advertising exposes one device name at a time. Set
 * CONFIG_BT_MAX_CONN to the number of links the board needs.)
 *
 * Data path (multiplexed onto each connection):
 *   central    --(GATT Write Without Response on RX char)-->  peripheral
 *   peripheral --(GATT Notify on TX char)-->                  central
 *
 * This is a POLLED channel, modelled on UartPolledChannel: the BLE stack
 * callbacks (which run in the Bluetooth RX context) only append received bytes
 * into `receive_buffer` and wake the reactor event loop via platform->notify();
 * the actual framing/deserialization/dispatch happens later in poll() on the
 * main thread.
 */

#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct BleChannel BleChannel;

typedef enum {
  BLE_CHANNEL_ROLE_PERIPHERAL, // advertises and waits for a central (server-like)
  BLE_CHANNEL_ROLE_CENTRAL,    // scans for `device_name` and connects (client-like)
} BleChannelRole;

/**
 * @brief Link-layer connection parameters that define the time-triggered schedule.
 *
 * The CENTRAL applies these once at connection setup (bt_conn_le_create). The
 * PERIPHERAL accepts them. They are the controlled inputs to the latency bound, so
 * they are reported (via the `le_param_updated` log) once the controller confirms
 * the negotiated values.
 */
typedef struct {
  uint16_t interval;            // connection interval in 1.25 ms units
  uint16_t latency;             // slave latency, in skipped connection events (0..499)
  uint16_t supervision_timeout; // BLE supervision timeout in 10 ms units
} BleConnParams;

// Compute the BLE unit values from milliseconds (compile-time for constants).
// Returns the BLE connection interval in 1.25 ms units (6..3200 = 7.5 ms..4 s).
#define BLE_CI_UNITS(ms) ((uint16_t)((ms) / 1.25f))
// Returns the BLE timeout in 10 ms units (10..3200 = 100 ms..32 s).
#define BLE_TIMEOUT_UNITS(ms) ((uint16_t)((ms) / 10))

#define BLE_CHANNEL_BUFFERSIZE 1024

struct BleChannel {
  PolledNetworkChannel super;
  NetworkChannelState state;
  // Guards receive_buffer/receive_buffer_index. Must be NON-BLOCKING: one holder
  // is the cooperative BT RX thread (a callback context), where blocking on a
  // k_mutex would stall all ATT processing.
  struct k_spinlock rx_lock;
  // Re-arms the link (advertise/scan) after a drop, off the disconnected callback.
  struct k_work recover_work;

  BleChannelRole role;
  const char* device_name; // Peripheral advertises it, while central scans for it

  BleConnParams params;  // Requested connection parameters (central applies them)
  uint16_t act_interval; // Actual negotiated CI (1.25 ms units), set by le_param_updated
  uint16_t act_latency;  // Actual negotiated slave latency
  uint16_t act_timeout;  // Actual negotiated supervision timeout (10 ms units)

  struct bt_conn* conn;   // The single active connection of THIS link (NULL when down)
  bt_addr_le_t peer_addr; // Central: address of the peer we initiated to (routing key)

  // Central-only GATT-client bookkeeping. Each param block is per-channel, so the
  // GATT callbacks recover their `self` via CONTAINER_OF and several links can be
  // discovered/subscribed independently.
  uint16_t peer_rx_handle; // Value handle of the peripheral's RX (we write here)
  uint16_t peer_tx_handle; // Value handle of the peripheral's TX (we subscribe)
  struct bt_gatt_subscribe_params subscribe_params;
  struct bt_gatt_discover_params discover_params;
  struct bt_gatt_exchange_params mtu_params;
  bool subscribed;
  bool discovered;

  uint16_t tx_payload; // Usable bytes per PDU = negotiated ATT MTU - 3

  // Receive buffering (filled in BLE callback context, drained in poll()).
  FederateMessage output;
  unsigned char receive_buffer[BLE_CHANNEL_BUFFERSIZE];
  unsigned int receive_buffer_index;
  // Incremented every time the buffer is flushed on link loss. poll() snapshots it before
  // scanning and re-checks it under `rx_lock` before compacting, so a flush that lands
  // mid-poll cannot make it consume offsets that no longer exist.
  uint32_t rx_generation;

  // Send scratch buffer (framed message before chunking onto PDUs).
  unsigned char send_buffer[BLE_CHANNEL_BUFFERSIZE];

  FederatedConnectionBundle* bundle;
  void (*receive_callback)(FederatedConnectionBundle* bundle, const FederateMessage* message);
};

/**
 * @brief Construct a BLE channel for one federated link.
 *
 * @param self        Storage for the channel (lives inside the connection bundle)
 * @param role        BLE_CHANNEL_ROLE_PERIPHERAL or BLE_CHANNEL_ROLE_CENTRAL
 * @param device_name Advertised name of the PERIPHERAL end of THIS link (both ends agree)
 * @param params      Connection interval, slave latency, and supervision timeout for this link
 */
void BleChannel_ctor(BleChannel* self, BleChannelRole role, const char* device_name, BleConnParams params);

#endif // REACTOR_UC_ZEPHYR_BLE_CHANNEL_H
