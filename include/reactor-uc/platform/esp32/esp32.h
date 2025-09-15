#ifndef REACTOR_UC_PLATFORM_ESP32_H
#define REACTOR_UC_PLATFORM_ESP32_H

#include "reactor-uc/platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
  Platform super;
  SemaphoreHandle_t sem;
} PlatformEsp32;

typedef struct {
  Mutex super;
  SemaphoreHandle_t mutex;
} MutexEsp32;

#define pdUS_TO_TICKS(xTimeInUs)                                                                                       \
  ((TickType_t)(((uint64_t)(xTimeInUs) * (uint64_t)configTICK_RATE_HZ) / (uint64_t)1000000U))

#define pdNS_TO_TICKS(xTimeInNs)                                                                                       \
  ((TickType_t)(((uint64_t)(xTimeInNs) * (uint64_t)configTICK_RATE_HZ) / (uint64_t)1000000000U))

#define pdTICKS_TO_US(xTicks) ((uint32_t)(((uint64_t)(xTicks) * (uint64_t)1000000U) / (uint64_t)configTICK_RATE_HZ))

#define pdTICKS_TO_NS(xTicks) ((uint32_t)(((uint64_t)(xTicks) * (uint64_t)1000000000U) / (uint64_t)configTICK_RATE_HZ))

#define PLATFORM_T PlatformEsp32
#define MUTEX_T MutexEsp32

#endif
