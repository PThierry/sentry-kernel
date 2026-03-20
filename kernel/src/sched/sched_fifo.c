// SPDX-FileCopyrightText: 2023 Ledger SAS
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <string.h>
#include <uapi/handle.h>
#include <sentry/ktypes.h>
#include <sentry/arch/asm-generic/membarriers.h>
#include <sentry/managers/debug.h>
#include <sentry/sched.h>
#include "sched_tasks.h"

/**
 * @brief Currentky ready tasks queue
 *
 * Behave like a ring buffer to avoid any memory move.
 * Each time a task is ready (spawned, event received while not ready,
 * sleep time finished, etc.) it is pushed into the task queue, pushing
 * the 'end_of_queue' offset from one.
 * Each time a task finishes (sleep, yield, blocking wait,...)
 * it is pop'ed and schedule, pushing the 'next_task' from one.
 */
typedef struct sched_fifo_context {
    taskh_t     tasks_queue[CONFIG_MAX_TASKS]; /**< list of eligible user task of the task set */
    uint16_t    next_task;     /**< next task offset in the RB */
    uint16_t    end_of_queue;  /**< end of the RB */
    bool        empty;         /**< RB empty flag */
    taskh_t     current; /* current can be one of tasks_queue user task, or idle */
 } sched_fifo_context_t;

#if CONFIG_SCHED_TDM
typedef struct sched_fifo_domain_ctx {
    uint8_t domain;
    bool initialized;
    sched_fifo_context_t ctx;
} sched_fifo_domain_ctx_t;

static sched_fifo_domain_ctx_t sched_fifo_domains[CONFIG_MAX_TASKS];
static uint8_t sched_fifo_domains_num;
static sched_fifo_context_t *sched_fifo_ctx_ref;
#define sched_fifo_ctx (*sched_fifo_ctx_ref)
#else
static sched_fifo_context_t sched_fifo_ctx;
#endif

static inline taskh_t sched_fifo_idle_task(void)
{
    return mgr_task_get_idle();
}

static inline void sched_fifo_context_reset(sched_fifo_context_t *ctx)
{
    memset(ctx, 0x0, sizeof(sched_fifo_context_t));
    ctx->empty = true;
    ctx->current = sched_fifo_idle_task();
}

/**
 * Initialize FIFO scheduler
 */
kstatus_t sched_fifo_init(void)
{
#if CONFIG_SCHED_TDM
    if (unlikely(sched_fifo_ctx_ref == NULL)) {
        return K_ERROR_BADSTATE;
    }
#endif
    pr_info("initialize scheduler");
    sched_fifo_context_reset(&sched_fifo_ctx);
    return K_STATUS_OKAY;
}

/**
 * @brief enqueue a newly eligible task to the scheduler queue
 *
 * This function add a task to the FIFO queue. This function should be called
 * exclusiverly in handler mode and is **not** reentrant. This should be the nominal
 * usage of the scheduler in Sentry as scheduler manipulation is done in handler
 * mode only.
 *
 * @param[in] t task handler to enqueue into the FIFO stack
 *
 * @return K_STATUS_OKAY when properly pushed. The K_SECURITY_INVSTATE is a
 *   critical value as this means that there is more tasks than configured
 *   in the kernel config at build time, which will lead to schedule problem.
 */
static inline kstatus_t sched_fifo_enqueue_task(taskh_t t)
{
    kstatus_t status = K_SECURITY_INVSTATE;
    if (unlikely((sched_fifo_ctx.next_task == sched_fifo_ctx.end_of_queue) &&
        sched_fifo_ctx.empty == false)) {
        pr_emerg("schedule queue badly dimensioned! unable to schedule %08x!!!", t);
        /* should never happen if CONFIG_MAX_TASKS is valid */
        /*@ assert \false; */
        goto err;
    }
    pr_debug("schedule task handle %08x", t);
    sched_fifo_ctx.tasks_queue[sched_fifo_ctx.end_of_queue] = t;
    sched_fifo_ctx.end_of_queue = (sched_fifo_ctx.end_of_queue + 1) % CONFIG_MAX_TASKS;
    sched_fifo_ctx.empty = false;
    status = K_STATUS_OKAY;
    request_data_membarrier();
err:
    return status;
}

/**
 * @brief dequeue an eligible task from the FIFO scheduler
 *
 * get back the next eligible task from the FIFO scheduler. If no task is eligible,
 * the function return the idle task handle.
 *
 * @return a valid task handle, include idle task specific task handle.
 *
 */
static inline taskh_t sched_fifo_dequeue_task(void)
{
    /* default task is idle */
    taskh_t t = sched_fifo_idle_task();
    if (likely(sched_fifo_ctx.empty == false)) {
        t = sched_fifo_ctx.tasks_queue[sched_fifo_ctx.next_task];
        sched_fifo_ctx.next_task = (sched_fifo_ctx.next_task + 1) % CONFIG_MAX_TASKS;
        if (sched_fifo_ctx.next_task == sched_fifo_ctx.end_of_queue) {
            sched_fifo_ctx.empty = true;
        }
    }
    sched_fifo_ctx.current = t;
    request_data_membarrier();
    return t;
}

kstatus_t sched_fifo_schedule(taskh_t t)
{
    return sched_fifo_enqueue_task(t);
}

taskh_t sched_fifo_elect(void)
{
    /* defaulting on idle */
    taskh_t tsk = sched_fifo_idle_task();
    if (likely(sched_fifo_ctx.empty == false)) {
        tsk = sched_fifo_dequeue_task();
    }
    return tsk;
}

taskh_t sched_fifo_get_current(void)
{
    return sched_fifo_ctx.current;
}

#ifdef CONFIG_BUILD_TARGET_AUTOTEST
kstatus_t sched_fifo_autotest(void)
{
    kstatus_t status = K_STATUS_OKAY;
    return status;
}
#endif

#if CONFIG_SCHED_TDM
static kstatus_t sched_fifo_switch_domain(uint8_t domain)
{
    for (uint8_t i = 0; i < sched_fifo_domains_num; ++i) {
        if (sched_fifo_domains[i].initialized && (sched_fifo_domains[i].domain == domain)) {
            sched_fifo_ctx_ref = &sched_fifo_domains[i].ctx;
            return K_STATUS_OKAY;
        }
    }

    if (unlikely(sched_fifo_domains_num >= CONFIG_MAX_TASKS)) {
        return K_SECURITY_INVSTATE;
    }

    sched_fifo_domains[sched_fifo_domains_num].domain = domain;
    sched_fifo_domains[sched_fifo_domains_num].initialized = true;
    sched_fifo_context_reset(&sched_fifo_domains[sched_fifo_domains_num].ctx);
    sched_fifo_ctx_ref = &sched_fifo_domains[sched_fifo_domains_num].ctx;
    sched_fifo_domains_num++;
    return K_STATUS_OKAY;
}

kstatus_t tasks_sched_init(void)
{
    memset(sched_fifo_domains, 0x0, sizeof(sched_fifo_domains));
    sched_fifo_domains_num = 0;
    sched_fifo_ctx_ref = NULL;
    return K_STATUS_OKAY;
}

kstatus_t tasks_sched_switch_domain(uint8_t domain)
{
    return sched_fifo_switch_domain(domain);
}

kstatus_t tasks_sched_schedule(taskh_t t)
{
    return sched_fifo_schedule(t);
}

taskh_t tasks_sched_elect(void)
{
    return sched_fifo_elect();
}

taskh_t tasks_sched_get_current(void)
{
    if (unlikely(sched_fifo_ctx_ref == NULL)) {
        return sched_fifo_idle_task();
    }
    return sched_fifo_get_current();
}

stack_frame_t *tasks_sched_refresh(stack_frame_t *frame)
{
    return frame;
}

void tasks_sched_window_leave(void)
{
    taskh_t current;
    job_state_t state;

    if (unlikely(sched_fifo_ctx_ref == NULL)) {
        return;
    }

    current = sched_fifo_get_current();
    if (mgr_task_is_idletask(current) == SECURE_TRUE) {
        return;
    }

    if (unlikely(mgr_task_get_state(current, &state) != K_STATUS_OKAY)) {
        return;
    }

    /* Preempted by TDM: put running FIFO task back at tail for next window. */
    if (state == JOB_STATE_READY) {
        (void)sched_fifo_schedule(current);
    }
}
#endif

/*
 * Why not using directly schedule() ? The goal is to support, in a future model,
 * MILS architecture which may requires hierarchycal scheduling, in which, depending on
 * the task domain, the scheduling model varies. In this last case:
 *
 * each domain has a relative priority (inter-domain priority)
 * each domain has its own scheduling model
 * This allows for e.g. to:
 * execute security task in a SCHED_FIFO execution model, with a higher priority
 * than the standard tasks, executed with quantum-based RRMQ.
 * security tasks enforce their execution, but with a long enough periord between
 * each FIFO sched execution.
 * This looks a little, for e.g., like the way Linux handle POSIX RR & FIFO vs PFAIR, but
 * in a more real-time enforced model.
 * By now, as there is a single scheduler() at a time, the symbol is redirected to
 * the currently selected (and compiled) scheduler source.
 */
/* default scheduler is FIFO */
#if !CONFIG_SCHED_TDM
kstatus_t sched_schedule(taskh_t t) __attribute__((alias("sched_fifo_schedule")));
taskh_t sched_elect(void) __attribute__((alias("sched_fifo_elect")));
taskh_t sched_get_current(void) __attribute__((alias("sched_fifo_get_current")));
kstatus_t sched_init(void) __attribute__((alias("sched_fifo_init")));
#ifdef CONFIG_BUILD_TARGET_AUTOTEST
kstatus_t sched_autotest(void) __attribute__((alias("sched_fifo_autotest")));
#endif
#endif
