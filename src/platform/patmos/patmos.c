#include "reactor-uc/platform/patmos/patmos.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <reactor-uc/environment.h>
#include "reactor-uc/logging.h"
#include "s4noc_channel.h"

#include <machine/rtc.h>
#include <machine/exceptions.h>

static PlatformPatmos platform;

void Platform_vprintf(const char *fmt, va_list args) {
  vprintf(fmt, args);
}

instant_t PlatformPatmos_get_physical_time(Platform *super) {
  (void)super;
  return USEC(get_cpu_usecs());
}

lf_ret_t PlatformPatmos_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformPatmos *self = (PlatformPatmos *)super;

  instant_t now = super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "PlatformPatmos_wait_until_interruptible: now: %llu sleeping until %llu", now, wakeup_time);

  // Do busy sleep
  do {
#if 1
    volatile _IODEV int *s4noc_status = (volatile _IODEV int *)PATMOS_IO_S4NOC;
    volatile _IODEV int *s4noc_data = (volatile _IODEV int *)(PATMOS_IO_S4NOC + 4);
    volatile _IODEV int *s4noc_source = (volatile _IODEV int *)(PATMOS_IO_S4NOC + 8);
    if ((*s4noc_status & 0x02) != 0) {
      int value = *s4noc_data;
      int source = *s4noc_source;
      LF_DEBUG(PLATFORM, "S4NOCPollChannel_poll: Received data 0x%08x (%c%c%c%c) from source %d", value,
               ((char *)&value)[0], ((char *)&value)[1], ((char *)&value)[2], ((char *)&value)[3], source);
      // Get the receive channel for the source core
      S4NOCPollChannel *receive_channel = s4noc_global_state.core_channels[source][get_cpuid()];

      if (receive_channel == NULL) {
        LF_DEBUG(PLATFORM, "No receive_channel for source=%d dest=%d - dropping word", source, get_cpuid());
        continue;
      }

      if (receive_channel->receive_buffer_index + 4 > S4NOC_CHANNEL_BUFFERSIZE) {
        LF_DEBUG(PLATFORM, "Receive buffer overflow: dropping message");
        receive_channel->receive_buffer_index = 0;
        continue;
      }

      ((int *)receive_channel->receive_buffer)[receive_channel->receive_buffer_index / 4] = value;
      receive_channel->receive_buffer_index += 4;
      // S4NOC_CHANNEL_DEBUG("receive_buffer_index ((%d))", receive_channel->receive_buffer_index);
      unsigned int expected_message_size = *((int *)receive_channel->receive_buffer);
      // S4NOC_CHANNEL_DEBUG("Expected message size: ((%d))", expected_message_size);
      if (receive_channel->receive_buffer_index >= expected_message_size + 4) {
        int bytes_left = deserialize_from_protobuf(&receive_channel->output,
                                                   receive_channel->receive_buffer + 4, // skip the 4-byte size header
                                                   expected_message_size                // only the message payload
        );
        // bytes_left = ((bytes_left / 4)+1) * 4;
        LF_DEBUG(PLATFORM, "Bytes Left after attempted to deserialize: %d", bytes_left);

        if (bytes_left >= 0) {
          size_t remaining = (size_t)bytes_left;
          receive_channel->receive_buffer_index = (remaining + 3) & ~3U; // rounded up to the next 4

          LF_DEBUG(PLATFORM, "Message received at core %d from core %d", get_cpuid(), source);
          if (receive_channel->receive_callback != NULL) {
            LF_DEBUG(PLATFORM, "calling user callback at %p!", receive_channel->receive_callback);
            S4NOCPollChannel *self = s4noc_global_state.core_channels[get_cpuid()][source];
            LF_DEBUG(PLATFORM, "receive channel pointer: %p self pointer: %p", (void *)receive_channel, (void *)self);
            FederatedConnectionBundle *federated_connection = self->federated_connection;
            receive_channel->receive_callback(federated_connection, &receive_channel->output);
          } else {
            LF_DEBUG(PLATFORM, "No receive callback registered, dropping message");
          }
        } else {
          LF_DEBUG(PLATFORM, "Error deserializing message, dropping");
          receive_channel->receive_buffer_index = 0;
        }
      } else {
        LF_DEBUG(PLATFORM, "Message not complete yet: received %d of %d bytes", receive_channel->receive_buffer_index,
                 expected_message_size + 4);
      }
    }
#endif
    now = super->get_physical_time(super);
  } while (now < wakeup_time);

  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  return LF_OK;
}

lf_ret_t PlatformPatmos_wait_until(Platform *super, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  instant_t now = super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "PlatformPatmos_wait_until: now: %llu sleeping until %llu", now, wakeup_time);

  // Do busy sleep
  do {
    now = super->get_physical_time(super);
  } while (now < wakeup_time);
  return LF_OK;
}

lf_ret_t PlatformPatmos_wait_for(Platform *super, interval_t duration) {
  if (duration <= 0) {
    return LF_OK;
  }

  instant_t now = super->get_physical_time(super);
  instant_t wakeup = now + duration;
  LF_DEBUG(PLATFORM, "PlatformPatmos_wait_for: now: %llu sleeping for %llu", now, duration);

  // Do busy sleep
  do {
    now = super->get_physical_time(super);
  } while (now < wakeup);

  return LF_OK;
}

void PlatformPatmos_leave_critical_section(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
}

void PlatformPatmos_enter_critical_section(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
}

void PlatformPatmos_notify(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
  self->async_event = true;
}

void Platform_ctor(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
  super->get_physical_time = PlatformPatmos_get_physical_time;
  super->wait_until = PlatformPatmos_wait_until;
  super->wait_for = PlatformPatmos_wait_for;
  super->wait_until_interruptible = PlatformPatmos_wait_until_interruptible;
  super->notify = PlatformPatmos_notify;
  self->num_nested_critical_sections = 0;
  LF_DEBUG(PLATFORM, "PlatformPatmos initialized");
}

Platform *Platform_new(void) {
  return (Platform *)&platform;
}

void MutexPatmos_unlock(Mutex *super) {
  MutexPatmos *self = (MutexPatmos *)super;
  PlatformPatmos *platform = (PlatformPatmos *)_lf_environment->platform;
  platform->num_nested_critical_sections--;
  if (platform->num_nested_critical_sections == 0) {
    intr_enable();
  } else if (platform->num_nested_critical_sections < 0) {
    validate(false);
  }
}

void MutexPatmos_lock(Mutex *super) {
  MutexPatmos *self = (MutexPatmos *)super;
  PlatformPatmos *platform = (PlatformPatmos *)_lf_environment->platform;
  if (platform->num_nested_critical_sections == 0) {
    intr_disable();
  }
  platform->num_nested_critical_sections++;
}

void Mutex_ctor(Mutex *super) {
  MutexPatmos *self = (MutexPatmos *)super;
  super->lock = MutexPatmos_lock;
  super->unlock = MutexPatmos_unlock;
}
