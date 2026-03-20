// SPDX-FileCopyrightText: 2026 H2Lab Development Team
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <string.h>
#include <sentry/arch/asm-generic/panic.h>
#include <sentry/ktypes.h>
#include <sentry/managers/debug.h>
#include <sentry/managers/task.h>
#include <sentry/sched.h>
#include "sched_tasks.h"

static_assert(CONFIG_SCHED_TDM_WINDOW_TICKS > 0,
			  "CONFIG_SCHED_TDM_WINDOW_TICKS must be greater than zero");

#define SCHED_TDM_INVALID_SLOT ((uint16_t)CONFIG_MAX_TASKS)

typedef struct sched_tdm_context {
	uint8_t domains[CONFIG_MAX_TASKS];
	uint16_t num_domains;
	uint16_t current_domain_slot;
	uint32_t ticks_in_slot;
} sched_tdm_context_t;

static sched_tdm_context_t sched_tdm_ctx;

static inline uint16_t sched_tdm_find_domain_slot(uint8_t domain)
{
	for (uint16_t i = 0; i < sched_tdm_ctx.num_domains; ++i) {
		if (sched_tdm_ctx.domains[i] == domain) {
			return i;
		}
	}
	return SCHED_TDM_INVALID_SLOT;
}

static uint16_t sched_tdm_register_domain(uint8_t domain)
{
	uint16_t slot = sched_tdm_find_domain_slot(domain);

	if (slot != SCHED_TDM_INVALID_SLOT) {
		return slot;
	}
	if (unlikely(sched_tdm_ctx.num_domains >= CONFIG_MAX_TASKS)) {
		panic(PANIC_KERNEL_SHORTER_KBUFFERS_CONFIG);
	}

	slot = sched_tdm_ctx.num_domains++;
	sched_tdm_ctx.domains[slot] = domain;
	return slot;
}

static inline void sched_tdm_select_current_domain(void)
{
	if ((sched_tdm_ctx.current_domain_slot == SCHED_TDM_INVALID_SLOT) ||
		(sched_tdm_ctx.current_domain_slot >= sched_tdm_ctx.num_domains)) {
		return;
	}

	if (unlikely(tasks_sched_switch_domain(
			sched_tdm_ctx.domains[sched_tdm_ctx.current_domain_slot]) != K_STATUS_OKAY)) {
		panic(PANIC_KERNEL_INVALID_MANAGER_STATE);
	}
}

static stack_frame_t *sched_tdm_switch_to_next_domain(stack_frame_t *frame)
{
	stack_frame_t *out_frame = frame;
	taskh_t next;

	tasks_sched_window_leave();

	if (sched_tdm_ctx.num_domains > 0) {
		sched_tdm_ctx.current_domain_slot =
			(uint16_t)((sched_tdm_ctx.current_domain_slot + 1U) % sched_tdm_ctx.num_domains);
	}
	sched_tdm_select_current_domain();

	next = tasks_sched_elect();
	if (unlikely(mgr_task_get_sp(next, &out_frame) != K_STATUS_OKAY)) {
		panic(PANIC_KERNEL_INVALID_MANAGER_RESPONSE);
	}
	return out_frame;
}

kstatus_t sched_tdm_init(void)
{
	pr_info("initialize TDM scheduler");
	memset(&sched_tdm_ctx, 0x0, sizeof(sched_tdm_context_t));
	sched_tdm_ctx.current_domain_slot = SCHED_TDM_INVALID_SLOT;
	return tasks_sched_init();
}

kstatus_t sched_tdm_schedule(taskh_t t)
{
	uint8_t domain_id;
	kstatus_t status;
	uint16_t domain_slot;

	if (unlikely((status = mgr_task_get_domain(t, &domain_id)) != K_STATUS_OKAY)) {
		return status;
	}

	domain_slot = sched_tdm_register_domain(domain_id);
	if (unlikely(domain_slot == SCHED_TDM_INVALID_SLOT)) {
		panic(PANIC_KERNEL_SHORTER_KBUFFERS_CONFIG);
	}

	if (unlikely((status = tasks_sched_switch_domain(domain_id)) != K_STATUS_OKAY)) {
		return status;
	}
	status = tasks_sched_schedule(t);

	if ((status == K_STATUS_OKAY) &&
		(sched_tdm_ctx.current_domain_slot == SCHED_TDM_INVALID_SLOT)) {
		sched_tdm_ctx.current_domain_slot = domain_slot;
		sched_tdm_ctx.ticks_in_slot = 0;
	}

	return status;
}

taskh_t sched_tdm_elect(void)
{
	if ((sched_tdm_ctx.num_domains == 0) ||
		(sched_tdm_ctx.current_domain_slot == SCHED_TDM_INVALID_SLOT)) {
		return mgr_task_get_idle();
	}

	sched_tdm_select_current_domain();
	return tasks_sched_elect();
}

taskh_t sched_tdm_get_current(void)
{
	if ((sched_tdm_ctx.num_domains == 0) ||
		(sched_tdm_ctx.current_domain_slot == SCHED_TDM_INVALID_SLOT)) {
		return mgr_task_get_idle();
	}

	sched_tdm_select_current_domain();
	return tasks_sched_get_current();
}

stack_frame_t *sched_tdm_refresh(stack_frame_t *frame)
{
	stack_frame_t *out_frame = frame;

	if ((sched_tdm_ctx.num_domains == 0) ||
		(sched_tdm_ctx.current_domain_slot == SCHED_TDM_INVALID_SLOT)) {
		return out_frame;
	}

	sched_tdm_select_current_domain();
	out_frame = tasks_sched_refresh(out_frame);

	sched_tdm_ctx.ticks_in_slot++;
	if (sched_tdm_ctx.ticks_in_slot < CONFIG_SCHED_TDM_WINDOW_TICKS) {
		return out_frame;
	}

	sched_tdm_ctx.ticks_in_slot = 0;
	if (sched_tdm_ctx.num_domains <= 1) {
		return out_frame;
	}

	return sched_tdm_switch_to_next_domain(out_frame);
}

#ifdef CONFIG_BUILD_TARGET_AUTOTEST
kstatus_t sched_tdm_autotest(void)
{
	return K_STATUS_OKAY;
}
#endif

kstatus_t sched_schedule(taskh_t t) __attribute__((alias("sched_tdm_schedule")));
taskh_t sched_elect(void) __attribute__((alias("sched_tdm_elect")));
taskh_t sched_get_current(void) __attribute__((alias("sched_tdm_get_current")));
kstatus_t sched_init(void) __attribute__((alias("sched_tdm_init")));
stack_frame_t *sched_refresh(stack_frame_t *frame) __attribute__((alias("sched_tdm_refresh")));
#ifdef CONFIG_BUILD_TARGET_AUTOTEST
kstatus_t sched_autotest(void) __attribute__((alias("sched_tdm_autotest")));
#endif
