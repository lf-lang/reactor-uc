#include "reactor-uc/logging.h"
#include "reactor-uc/platform/aducm355/aducm355.h"
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

#define CLOCK_FREQ_HZ 32768

static PlatformAducm355 platform;

void Platform_vprintf(const char *fmt, va_list args) { vprintf(fmt, args); }

void ClockInit(void)
{
   DigClkSel(DIGCLK_SOURCE_HFOSC);
   ClkDivCfg(1,1);
}

void UartInit(void)
{
   // DioCfg(pADI_GPIO0,0x500000);                // Setup P0[11:10] as UART pins
   DioCfgPin(pADI_GPIO0,PIN10,1);               // Setup P0.10 as UART pin
   DioCfgPin(pADI_GPIO0,PIN11,1);               // Setup P0.11 as UART pin
   UrtCfg(pADI_UART0,57600,
          (BITM_UART_COMLCR_WLS|3),0);         // Configure UART for 57600 baud rate
   UrtFifoCfg(pADI_UART0, RX_FIFO_1BYTE,      // Configure the UART FIFOs for 8 bytes deep
              BITM_UART_COMFCR_FIFOEN);
   UrtFifoClr(pADI_UART0, BITM_UART_COMFCR_RFCLR// Clear the Rx/TX FIFOs
              |BITM_UART_COMFCR_TFCLR);
}

void RTC1_Int_Handler(void)
{
   if(pADI_RTC1->SR0 & BITM_RTC_SR0_ALMINT) // alarm interrupt
   {
      RtcIntClrSR0(BITM_RTC_SR0_ALMINT); //clear interrupt source
   }
   if(pADI_RTC1->SR2 & BITM_RTC_SR2_PSINT) // prescaled, modulo-1 interrupt
   {
      RtcIntClrSR2(BITM_RTC_SR2_PSINT); //clear interrupt source
   }
   if(pADI_RTC1->SR2 & BITM_RTC_SR2_CNTINT)
   {
      RtcIntClrSR2(BITM_RTC_SR2_CNTINT); //clear interrupt
   }
   if(pADI_RTC1->SR0 & BITM_RTC_SR0_MOD60ALMINT) // mod60 alarm interrupt
   {
      RtcIntClrSR0(BITM_RTC_SR0_MOD60ALMINT); //clear interrupt
   }
   if(pADI_RTC1->SR2 & BITM_RTC_SR2_CNTMOD60ROLLINT)
   {
      RtcIntClrSR2(BITM_RTC_SR2_CNTMOD60ROLLINT); //clear interrupt
   }
}

void EnterHibernateMode(void)
{
   pADI_AFE->PSWFULLCON =0x6000;              // Close PL2, PL, , switches to tie Excitation Amplifiers N and D terminals to 1.8V LDO
   pADI_AFE->NSWFULLCON =0xC00;               // Close Nl and NL2.

   pADI_AFE->TSWFULLCON =0x0000;              // open all T switches
   pADI_AFE->DSWFULLCON =0x0000;              // open all D switches
   pADI_AFE->SWCON =0x10000;                  // Use xFULLSWCON registers
   pADI_AFE->HSRTIACON=0xF;                   // open switch at HPTIA near RTIA
   pADI_AFE->DE1RESCON=0xFF;                  // open switch at HPTIA near RTIA05
   pADI_AFE->DE0RESCON=0xFF;                  // open switch at HPTIA near RTIA03  
      
   delay(10000);
   DioSetPin(pADI_GPIO0,0x4);                 // Set P0.2
   AfePwrCfg(AFE_HIBERNATE);
   delay(1000);                                // Wait for AFE to enter Hibernate mode

  PwrCfg(ENUM_PMG_PWRMOD_HIBERNATE,            // Place digital die in Hibernate
         BITM_PMG_PWRMOD_MONVBATN,
         BITM_PMG_SRAMRET_BNK2EN);

}

lf_ret_t PlatformAducm355_initialize(Platform *self) {
  PlatformAducm355 *p = (PlatformAducm355 *)self;
  AD5940_Initialize();
  ClockInit();
  UartInit();
  RtcCfgCR0(BITM_RTC_CR0_CNTEN,0);//disable RTC
   /*set initial count value*/
   RtcSetCnt(0);
   RtcSetPre(RTC1_PRESCALE_1);
   RtcIntClrSR0(BITM_RTC_SR0_ALMINT);//clear interrupt source
   RtcCfgCR0(BITM_RTC_CR0_ALMEN|BITM_RTC_CR0_ALMINTEN,1);

      /*Enable RTC interrupt in NVIC*/
      NVIC_EnableIRQ(RTC1_EVT_IRQn);
      //Globle enable for the RTC1
      RtcCfgCR0(BITM_RTC_CR0_CNTEN,1);
  return LF_OK;
}

instant_t PlatformAducm355_get_physical_time(Platform *self) {
  (void)self;
  uint32_t upper = 0;
  uint32_t cnt1_prev, cnt1;
  uint16_t cnt2;
  int ret = RtcGetSnap(&cnt1, &cnt2);
  validate(ret == 1);

  if (cnt1 < cnt1_prev) {
    upper++;
  }

  uint64_t ticks = (((uint64_t) upper) << 32) | cnt1;

  float seconds = ((float) ticks) / CLOCK_FREQ_HZ;
  float nsecs = seconds * 1e9;

  return (int64_t)nsecs;
}

lf_ret_t PlatformAducm355_wait_for(Platform *self, instant_t duration) {
  (void)self;
  if (duration <= 0)
    return LF_OK;
  instant_t wakeup_time = duration + self->get_physical_time(self);
  return LF_OK;
}

lf_ret_t PlatformAducm355_wait_until(Platform *self, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);
  while(self->get_physical_time(self) < wakeup_time) {}
  return LF_OK;
}

lf_ret_t PlatformAducm355_wait_until_interruptible(Platform *self, instant_t wakeup_time) {
  PlatformAducm355 *p = (PlatformAducm355 *)self;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);
  self->wait_until(self, wakeup_time);
}

void PlatformAducm355_leave_critical_section(Platform *self) {
  PlatformAducm355 *p = (PlatformAducm355 *)self;
  LF_DEBUG(PLATFORM, "Leave critical section");
}

void PlatformAducm355_enter_critical_section(Platform *self) {
  PlatformAducm355 *p = (PlatformAducm355 *)self;
  LF_DEBUG(PLATFORM, "Enter critical section");
}

void PlatformAducm355_new_async_event(Platform *self) {
  LF_DEBUG(PLATFORM, "New async event");
}

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformAducm355_enter_critical_section;
  self->leave_critical_section = PlatformAducm355_leave_critical_section;
  self->get_physical_time = PlatformAducm355_get_physical_time;
  self->wait_until = PlatformAducm355_wait_until;
  self->wait_for = PlatformAducm355_wait_for;
  self->initialize = PlatformAducm355_initialize;
  self->wait_until_interruptible = PlatformAducm355_wait_until_interruptible;
  self->new_async_event = PlatformAducm355_new_async_event;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
