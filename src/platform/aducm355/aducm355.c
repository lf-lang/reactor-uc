#include "reactor-uc/logging.h"
#include "reactor-uc/platform/aducm355/aducm355.h"
#include "reactor-uc/environment.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <ADuCM355.h>
#include <stdio.h>
#include "ad5940.h"
#include "DioLib.h"
#include "RtcLib.h"
#include "PwrLib.h"
#include "ClkLib.h"
#include "UrtLib.h"

#define CLOCK_FREQ_HZ 32768                 // The frequency of the RTC
#define CLOCK_EPOCH_DURATION_NS SEC(131072) // The duration of an entire epoch, i.e. one roll-over.

#if !defined(LF_BUSY_WAIT)
#define CLOCK_MIN_HIBERNATE_DURATION MSEC(1) // If a requested sleep is below 1 msec we do a busy wait instead
#else
#define CLOCK_MIN_HIBERNATE_DURATION FOREVER // If a requested sleep is below 1 msec we do a busy wait instead
#endif

static PlatformAducm355 platform;

/** Returns the current number of clock ticks from the RTC. */
static uint32_t get_ticks() {
  uint32_t ticks;
  uint16_t unused;
  RtcGetSnap(&ticks, &unused);
  return ticks;
}

/** Perform a busy-wait. */
void busy_wait(long int length) {
  while (length > 0)
    length--;
}

void Platform_vprintf(const char *fmt, va_list args) {
  vprintf(fmt, args);
}

/** Initialize the clock system. This function was copied from vendor examples. */
void ClockInit(void) {
  DigClkSel(DIGCLK_SOURCE_HFOSC);
  ClkDivCfg(1, 1);
}

/** Initialize the UART system with a baud rate of 57600. This function was copied from vendor examples. */
void UartInit(void) {
  // DioCfg(pADI_GPIO0,0x500000);                // Setup P0[11:10] as UART pins
  DioCfgPin(pADI_GPIO0, PIN10, 1);                            // Setup P0.10 as UART pin
  DioCfgPin(pADI_GPIO0, PIN11, 1);                            // Setup P0.11 as UART pin
  UrtCfg(pADI_UART0, B115200, (BITM_UART_COMLCR_WLS | 3), 0); // Configure UART for 115200 baud rate
  UrtFifoCfg(pADI_UART0, RX_FIFO_1BYTE,                       // Configure the UART FIFOs for 8 bytes deep
             BITM_UART_COMFCR_FIFOEN);
  UrtFifoClr(pADI_UART0, BITM_UART_COMFCR_RFCLR // Clear the Rx/TX FIFOs
                             | BITM_UART_COMFCR_TFCLR);
}

/** ISR for the RTC1 interrupts. For handling wakeup-alarms. */
void RTC1_Int_Handler(void) {
  if (pADI_RTC1->SR0 & BITM_RTC_SR0_ALMINT) // alarm interrupt
  {
    RtcIntClrSR0(BITM_RTC_SR0_ALMINT);
  }
}

/** Enter low-power hibernate mode. Copied from vendor examples. */
void EnterHibernateMode(void) {
  pADI_AFE->PSWFULLCON = 0x6000; // Close PL2, PL, , switches to tie Excitation Amplifiers N and D terminals to 1.8V LDO
  pADI_AFE->NSWFULLCON = 0xC00;  // Close Nl and NL2.

  pADI_AFE->TSWFULLCON = 0x0000; // open all T switches
  pADI_AFE->DSWFULLCON = 0x0000; // open all D switches
  pADI_AFE->SWCON = 0x10000;     // Use xFULLSWCON registers
  pADI_AFE->HSRTIACON = 0xF;     // open switch at HPTIA near RTIA
  pADI_AFE->DE1RESCON = 0xFF;    // open switch at HPTIA near RTIA05
  pADI_AFE->DE0RESCON = 0xFF;    // open switch at HPTIA near RTIA03

  busy_wait(10000);
  DioSetPin(pADI_GPIO0, 0x4); // Set P0.2
  AfePwrCfg(AFE_HIBERNATE);
  busy_wait(1000); // Wait for AFE to enter Hibernate mode

  PwrCfg(ENUM_PMG_PWRMOD_HIBERNATE, // Place digital die in Hibernate
         BITM_PMG_PWRMOD_MONVBATN, BITM_PMG_SRAMRET_BNK2EN);
}

/** Initialze the RTC clock which we use for physical time and sleeping. */
void RtcInit(void) {
  busy_wait(1000000);
  RtcCfgCR0(BITM_RTC_CR0_CNTEN, 0); // disable RTC
  /*set initial count value*/
  RtcSetCnt(0);
  RtcSetPre(RTC1_PRESCALE_1);
  // Clear any pending interrupts
  RtcIntClrSR0(BITM_RTC_SR0_ALMINT);
  // Enable interrupts on alarm and roll-over.
  RtcCfgCR0(BITM_RTC_CR0_ALMEN | BITM_RTC_CR0_ALMINTEN, 1);

  /*Enable RTC interrupt in NVIC*/
  NVIC_EnableIRQ(RTC1_EVT_IRQn);
  // Globle enable for the RTC1
  RtcCfgCR0(BITM_RTC_CR0_CNTEN, 1);
}

instant_t PlatformAducm355_get_physical_time(Platform *super) {
  PlatformAducm355 *self = (PlatformAducm355 *)super;

  // To translate the current RTC counter value into nanoseconds with the minimal loss of
  // precision we translate the 32bit counter value alone, and later adds it to the epoch value
  // which is incremented by the ISR.
  // To translate from cnt -> nsecs we multiply the count by 1e9 and divide by the frequency.
  // We do the multiplication first to avoid any loss in precision. This works because the
  // maximum value of cnt * SEC(1) will fit in a 64bit value.
  // Before reading the epoch value we must enter a critical section to avoid a race condition

  MUTEX_LOCK(self->mutex);
  uint64_t ticks = get_ticks();
  if (ticks < self->ticks_last) {
    self->epoch += CLOCK_EPOCH_DURATION_NS;
  }
  self->ticks_last = ticks;
  MUTEX_UNLOCK(self->mutex);

  uint64_t nsecs = (ticks * SEC(1)) / CLOCK_FREQ_HZ;

  instant_t res = self->epoch + nsecs;
  return res;
}

lf_ret_t PlatformAducm355_wait_for(Platform *super, instant_t duration) {
  if (duration <= 0)
    return LF_OK;
  instant_t wakeup_time = duration + super->get_physical_time(super);
  return super->wait_until(super, wakeup_time);
}

lf_ret_t PlatformAducm355_wait_until(Platform *super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);

  // If the requested sleep duration is shorter than the minimum hibernation
  // duration, we do a busy wait instead.
  interval_t duration = wakeup_time - super->get_physical_time(super);
  if (duration <= CLOCK_MIN_HIBERNATE_DURATION) {
    while (super->get_physical_time(super) < wakeup_time) {
      busy_wait(100000);
    }
    return LF_OK;
  }

  // Again we try to keep maximum precision and avoid floating point arithmetic
  // We want to calculate the alarm value, i.e. the value of the 32 bit counter
  // when to wake up. We dont care if it is the wrong epoch, because we will check
  // the physical time and go back to sleep until next epoch if necessary.

  // Find the number of nanseconds into an epoch when we want to wakeup.
  // This fits in a uint32
  uint64_t wakeup_after_epoch_ns = wakeup_time % CLOCK_EPOCH_DURATION_NS;

  // Then we translate this into ticks, we can safely do the multiplication first
  // and avoid overflows when using 64bit values.
  uint64_t wakeup_after_epoch_ticks = (wakeup_after_epoch_ns * CLOCK_FREQ_HZ) / SEC(1);

  uint32_t alarm_time = (uint32_t)wakeup_after_epoch_ticks;

  while (super->get_physical_time(super) < wakeup_time) {
    RtcSetAlarm(alarm_time, 0);
    EnterHibernateMode();
  }
  return LF_OK;
}

lf_ret_t PlatformAducm355_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformAducm355 *self = (PlatformAducm355 *)super;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);

  while (super->get_physical_time(super) < wakeup_time && !self->new_async_event) {
    busy_wait(100000);
  }
  lf_ret_t return_value = self->new_async_event ? LF_SLEEP_INTERRUPTED : LF_OK;
  self->new_async_event = false;
  return return_value;
}

void PlatformAducm355_notify(Platform *super) {
  PlatformAducm355 *self = (PlatformAducm355 *)super;
  LF_DEBUG(PLATFORM, "New async event");
  self->new_async_event = true;
}

void Platform_ctor(Platform *super) {
  PlatformAducm355 *self = (PlatformAducm355 *)super;
  super->get_physical_time = PlatformAducm355_get_physical_time;
  super->wait_until = PlatformAducm355_wait_until;
  super->wait_for = PlatformAducm355_wait_for;
  super->wait_until_interruptible = PlatformAducm355_wait_until_interruptible;
  super->notify = PlatformAducm355_notify;
  self->ticks_last = 0;
  self->epoch = 0;
  self->new_async_event = false;
  self->num_nested_critical_sections = 0;

  Mutex_ctor(&self->mutex.super);

  AD5940_Initialize();
  ClockInit();
  UartInit();
  RtcInit();
}

Platform *Platform_new() {
  return &platform.super;
}

void MutexAducm355_unlock(Mutex *super) {
  (void)super;
  PlatformAducm355 *platform = (PlatformAducm355 *)_lf_environment->platform;

  platform->num_nested_critical_sections--;
  if (platform->num_nested_critical_sections == 0) {
    __enable_irq();
  }
}

void MutexAducm355_lock(Mutex *super) {
  (void)super;
  PlatformAducm355 *platform = (PlatformAducm355 *)_lf_environment->platform;
  if (platform->num_nested_critical_sections == 0) {
    __disable_irq();
  }
  platform->num_nested_critical_sections++;
}

void Mutex_ctor(Mutex *super) {
  super->lock = MutexAducm355_lock;
  super->unlock = MutexAducm355_unlock;
}
