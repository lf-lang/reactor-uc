#include "reactor-uc/logging.h"
#include "reactor-uc/platform/esp-idf/esp_idf.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "reactor-uc/environment.h"

static PlatformEspidf platform;

void Platform_vprintf(const char *fmt, va_list args) {
  vprintf(fmt, args);
}

instant_t PlatformEspidf_get_physical_time(Platform * super) {
  (void)super;
  int64_t now = esp_timer_get_time();
  return now * 1000;
}

lf_ret_t PlatformEspidf_wait_for(Platform * super, instant_t duration) {
  (void)super;
  if (duration <= 0) {
    return LF_OK;
  }
  int64_t sleep_duration_usec = duration / 1000;
  LF_DEBUG(PLATFORM, "Waiting duration " PRId64 " usec", sleep_duration_usec);
  vTaskDelay(pdUS_TO_TICKS(sleep_duration_usec));
  return LF_OK;
}

lf_ret_t PlatformEspidf_wait_until(Platform *super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  return PlatformEspidf_wait_for(super, sleep_duration);
}

lf_ret_t PlatformEspidf_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformEspidf *self = (PlatformEspidf *)super;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);

  // time struct
  TickType_t target = (uint32_t)(wakeup_time / 1000);
  BaseType_t ret = xSemaphoreTake(self->sem, target);

  // if the semaphore was taken, it means the wait was interrupted
  if (ret == pdTRUE) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    return LF_SLEEP_INTERRUPTED;
  }

  // wait completed
  if (ret != pdFALSE) {
    LF_ERR(PLATFORM, "Wait until failed");
  }
  LF_DEBUG(PLATFORM, "Wait until completed");
  return LF_OK;
}

void PlatformEspidf_notify(Platform *super) {
  PlatformEspidf *self = (PlatformEspidf *)super;
  LF_DEBUG(PLATFORM, "New async event");
  xSemaphoreGive(self->sem);
}

void Platform_ctor(Platform *super) {
  PlatformEspidf *self = (PlatformEspidf *)super;
  super->get_physical_time = PlatformEspidf_get_physical_time;
  super->wait_until = PlatformEspidf_wait_until;
  super->wait_for = PlatformEspidf_wait_for;
  super->wait_until_interruptible = PlatformEspidf_wait_until_interruptible;
  super->notify = PlatformEspidf_notify;

  // Initialize binary semaphore with initial count 0 and limit 1.
  self->sem = xSemaphoreCreateBinary();
  if (self->sem == NULL) {
    LF_ERR(PLATFORM, "Failed to create semaphore");
  }
}

Platform *Platform_new() {
  return &platform.super;
}

void MutexEspidf_unlock(Mutex *super) {
  MutexEspidf *self = (MutexEspidf *)super;
  BaseType_t ret = xSemaphoreTake(self->mutex, portMAX_DELAY);
  if (ret != pdTRUE) {
    LF_ERR(PLATFORM, "Failed to take mutex in unlock");
  }
}

void MutexEspidf_lock(Mutex *super) {
  MutexEspidf *self = (MutexEspidf *)super;
  BaseType_t ret = xSemaphoreGive(self->mutex);
  if (ret != pdTRUE) {
    LF_ERR(PLATFORM, "Failed to give mutex in lock");
  }
}

void Mutex_ctor(Mutex *super) {
  MutexEspidf *self = (MutexEspidf *)super;
  super->lock = MutexEspidf_lock;
  super->unlock = MutexEspidf_unlock;
  self->mutex = xSemaphoreCreateMutex();
  if (self->mutex == NULL) {
    LF_ERR(PLATFORM, "Failed to create mutex");
  }
  // Initialize the mutex
  BaseType_t ret = xSemaphoreTake(self->mutex, 0);
  if (ret != pdTRUE) {
    LF_ERR(PLATFORM, "Failed to initialize mutex in ctor");
  }
}
