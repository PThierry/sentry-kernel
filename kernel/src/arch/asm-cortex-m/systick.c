// SPDX-FileCopyrightText: 2023 Ledger SAS
// SPDX-License-Identifier: Apache-2.0

#include <inttypes.h>
#include <assert.h>


#include <sentry/arch/asm-generic/interrupt.h>
#include <sentry/arch/asm-cortex-m/core.h>
#include <sentry/arch/asm-cortex-m/systick.h>
#include <sentry/arch/asm-cortex-m/tick.h>
#include <sentry/io.h>


/**
 *  NOTE:
 * at library build time, no external dep. This means that these symbols
 * need to be declared by others libraries (required, no weak)
 */
/*@
  // TODO: by do, no border effect as managers not yet proven
  assigns \nothing;
 */
uint64_t systime_get_cycle(void);

/*@
  // TODO: by do, no border effect as managers not yet proven
  assigns \nothing;
 */
stack_frame_t *sched_refresh(stack_frame_t *frame);

/*@
  // TODO: by do, no border effect as managers not yet proven
  assigns \nothing;
 */
void mgr_time_delay_tick(void);



// FIXME: systick registers defs is in cmsis (core.h)
#define SCB_SYSTICK_CSR    (SysTick_BASE + 0x0u)

#define SCB_SYSTICK_CSR_ENA_Pos (0u)
#define SCB_SYSTICK_CSR_ENA_Msk (0x1u)

typedef enum scb_systick_enable {
    SCB_SYSTICK_TIMER_DISABLE = 0,
    SCB_SYSTICK_TIMER_ENABLE = 1,
} scb_systick_enable_t;


#define SCB_SYSTICK_CSR_TICKINT_Pos (1u)
#define SCB_SYSTICK_CSR_TICKINT_Msk (0x1u)

typedef enum scb_systick_tickint {
    SCB_SYSTICK_TICKINT_DISABLE = 0,
    SCB_SYSTICK_TICKINT_ENABLE = 1,
} scb_systick_tickint_t;

#define SCB_SYSTICK_CSR_CLKSRC_Pos (2u)
#define SCB_SYSTICK_CSR_CLKSRC_Msk (0x1u)

typedef enum scb_systick_clksource {
    SCB_SYSTICK_CLKSRC_EXT = 0,
    SCB_SYSTICK_CLKSRC_CPU = 1,
} scb_systick_clksource_t;

#define SCB_SYSTICK_CSR_CNTFLAG_Pos (16u)
#define SCB_SYSTICK_CSR_CNTFLAG_Msk (0x1u)

#define SCB_SYSTICK_RVR    (SysTick_BASE + 0x4u)

#define SCB_SYSTICK_RVR_RELOAD_Pos (0u)
#define SCB_SYSTICK_RVR_RELOAD_Msk (0x00ffffffu)

#define SCB_SYSTICK_CVR    (SysTick_BASE + 0x8u)

#define SCB_SYSTICK_CVR_CURRENT_Pos (0u)
#define SCB_SYSTICK_CVR_CURRENT_Msk (0x00ffffffu)

#define SCB_SYSTICK_CALIB  (SysTick_BASE + 0xCu)

#define SCB_SYSTICK_CALIB_TENMS_Pos (0u)
#define SCB_SYSTICK_CALIB_TENMS_Msk (0x00ffffffu)

#define SCB_SYSTICK_CALIB_SKEW_Pos (30u)
#define SCB_SYSTICK_CALIB_SKEW_Msk (0x1u)

typedef enum scb_systick_clkskew {
    SCB_SYSTICK_CLKSKEW_CALIB_VALID = 0,
    SCB_SYSTICK_CLKSKEW_CALIB_INVALID = 1,
} scb_systick_clkskew_t;

#define SCB_SYSTICK_CALIB_NOREF_Pos (31u)
#define SCB_SYSTICK_CALIB_NOREF_Msk (0x1u)

typedef enum scb_systick_clkref {
    SCB_SYSTICK_CLKREF_AVAILABLE = 0,
    SCB_SYSTICK_CLKREF_UNAVAILABLE = 1,
} scb_systick_clkref_t;

static uint64_t jiffies;

/*@
   requires CONFIG_SYSTICK_HZ > 0;
   assigns *((SysTick_Type*)SysTick_BASE);
 */
static void systick_calibrate(void)
{
    uint32_t freq = rcc_get_core_frequency();
    uint32_t reload =  freq / CONFIG_SYSTICK_HZ;

    iowrite32(SCB_SYSTICK_CSR, 0);
    iowrite32(SCB_SYSTICK_CVR, 0);
    iowrite32(SCB_SYSTICK_RVR, (uint32_t)reload - 1);
}


uint64_t systime_get_jiffies(void)
{
    return jiffies;
}

/*@
    assigns ghost_systick_initialized;
    assigns *((SysTick_Type*)SysTick_BASE);
    assigns jiffies;
 */
void systick_init(void)
{
    uint32_t reg;
    /*@ assert ghost_systick_initialized == false; */
    /*@ ghost ghost_systick_initialized = true; */

    static_assert(CONFIG_SYSTICK_HZ > 0, "system tick frequency MUST NOT be null");

    jiffies = 0ULL;

    /* calibrate systick */
    systick_calibrate();
    /* enable interrupt, set clksource as CPU clock */
    reg  = ((SCB_SYSTICK_CLKSRC_CPU & SCB_SYSTICK_CSR_CLKSRC_Msk) << SCB_SYSTICK_CSR_CLKSRC_Pos) |
           ((SCB_SYSTICK_TIMER_ENABLE & SCB_SYSTICK_CSR_ENA_Msk) << SCB_SYSTICK_CSR_ENA_Pos) |
           ((SCB_SYSTICK_TICKINT_ENABLE & SCB_SYSTICK_CSR_TICKINT_Msk) << SCB_SYSTICK_CSR_TICKINT_Pos);
    iowrite32(SCB_SYSTICK_CSR, reg);
}

void systick_stop_and_clear(void)
{
    iowrite32(SCB_SYSTICK_CSR, 0UL);
    iowrite32(SCB_SYSTICK_RVR, 0UL);
    iowrite32(SCB_SYSTICK_CVR, 0UL);
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
}

/*@
  assigns jiffies;
 */
stack_frame_t *systick_handler(stack_frame_t * stack_frame)
{
    jiffies++;
    /*
     * refresh swt-based systime calculation
     * This is done with HZ period, which guarantee that the no DWT loop is
     * missed, except in low power mode (when systick is deactivated).
     */
    systime_get_cycle();
#if CONFIG_SCHED_RRMQ || CONFIG_SCHED_RMA || CONFIG_SCHED_TDM
    /* refresh quantums */
    stack_frame = sched_refresh(stack_frame);
#endif
    /* upgrade delayed tasks (slepping task) */
    mgr_time_delay_tick();
    return stack_frame;
}
