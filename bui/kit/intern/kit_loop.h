/* BUI-Awra: loop timers shared by the native hosts (kit_loop.c).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/* The earliest due time (bk_time() seconds), 0 when no timer is armed. */
double bk_loop_next_due(void);
/* `timeout_ms` (-1: forever) shortened to the next loop timer. */
int bk_loop_timeout_ms(int timeout_ms);
/* Runs the due timers; returns how many ran. */
int bk_loop_run_due(void);
/* Implemented by the host in use: interrupts its wait (a timer was added). */
void bk_loop_wake_host(void);
