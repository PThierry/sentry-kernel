// SPDX-FileCopyrightText: 2026 H2Lab Development Team
// SPDX-License-Identifier: Apache-2.0

#ifndef SCHED_TASKS_H
#define SCHED_TASKS_H

#include <sentry/arch/asm-generic/thread.h>
#include <sentry/ktypes.h>
#include <sentry/managers/task.h>

#if CONFIG_SCHED_TDM
/**
 * @brief Initialize the task-level scheduler backend used by TDM.
 */
kstatus_t tasks_sched_init(void);

/**
 * @brief Select the domain-bound scheduler context to operate on.
 */
kstatus_t tasks_sched_switch_domain(uint8_t domain);

/**
 * @brief Add a task to the currently selected task-level scheduler context.
 */
kstatus_t tasks_sched_schedule(taskh_t t);

/**
 * @brief Elect next task in the currently selected task-level context.
 */
taskh_t tasks_sched_elect(void);

/**
 * @brief Get current task in the currently selected task-level context.
 */
taskh_t tasks_sched_get_current(void);

/**
 * @brief Refresh currently selected task-level context on each tick.
 */
stack_frame_t *tasks_sched_refresh(stack_frame_t *frame);

/**
 * @brief Hook called by TDM before leaving the current domain timeslot.
 */
void tasks_sched_window_leave(void);
#endif

#endif/*!SCHED_TASKS_H*/
