#ifndef REACTOR_UC_PLATFORM_FREERTOS_H
#define REACTOR_UC_PLATFORM_FREERTOS_H

#include "reactor-uc/platform.h"
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct {
  Platform super;
  SemaphoreHandle_t sem;
} PlatformFreertos;

typedef struct {
  Mutex super;
  SemaphoreHandle_t mutex;
} MutexFreertos;

#define pdUS_TO_TICKS(xTimeInUs)                                                                                       \
  ((TickType_t)(((uint64_t)(xTimeInUs) * (uint64_t)configTICK_RATE_HZ) / (uint64_t)1000000U))

#define pdNS_TO_TICKS(xTimeInNs)                                                                                       \
  ((TickType_t)(((uint64_t)(xTimeInNs) * (uint64_t)configTICK_RATE_HZ) / (uint64_t)1000000000U))

#define pdTICKS_TO_US(xTicks) ((uint32_t)(((uint64_t)(xTicks) * (uint64_t)1000000U) / (uint64_t)configTICK_RATE_HZ))

#define pdTICKS_TO_NS(xTicks) ((uint64_t)(((uint64_t)(xTicks) * (uint64_t)1000000000U) / (uint64_t)configTICK_RATE_HZ))

#if (configUSE_64_BIT_TICKS != 1)
#if (configUSE_TICK_HOOK != 1)
#error "Tick hook must be enabled when using 32-bit ticks to handle overflows."
#endif
// Global variables for 32-bit tick overflow handling
extern volatile uint32_t g_tick_overflow_count;
extern volatile uint32_t g_last_tick_count;
#endif

void vApplicationTickHook(void);

#define PLATFORM_T PlatformFreertos
#define MUTEX_T MutexFreertos

#endif // REACTOR_UC_PLATFORM_FREERTOS_H
