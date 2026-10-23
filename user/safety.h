/*
 * safety.h - the AUTO-REVERT SAFETY HARNESS.
 *
 * ABSOLUTE SAFETY CONSTRAINT (non-negotiable): no self-lockout. Every action
 * that can drop or change the Wi-Fi link must arm a mandatory, capped
 * auto-revert BEFORE the change takes effect, restored from a thread that
 * survives a network teardown. Never a one-way change that only applies on
 * reboot; never airplane mode.
 *
 * This module is that spine. The risky features (power profile, HT40) hang off
 * it: they snapshot the current state, hand the harness an
 * `apply` and a `revert` callback, and the harness guarantees the revert runs
 * unless the user confirms within the cap.
 *
 * Why it survives teardown: the watchdog is a plain user thread created at
 * module init that lives for the module's lifetime and never depends on the
 * network link. The revert callbacks only call the kernel driver setters and
 * re-associate, none of which need the link to be up. So even if the
 * change severs Wi-Fi completely, the timer still fires and still restores the
 * previous state.
 */
#ifndef WIFITURBO_SAFETY_H
#define WIFITURBO_SAFETY_H

#include "../include/wifiturbo.h"

#define WQ_SAFETY_CAP_MS_DEFAULT   15000   /* 15 s default cap                 */
#define WQ_SAFETY_CTX_MAX          160     /* inline revert context size       */
#define WQ_SAFETY_SLOTS            4

/* apply/revert operate on the inline ctx the caller handed to wq_safety_arm.
 * Both must be self-contained: they may run from the watchdog thread with no
 * network link available. Return WQ_OK on success. */
typedef int (*wq_apply_fn)(void *ctx);
typedef int (*wq_revert_fn)(void *ctx);

int  wq_safety_init(void);
void wq_safety_fini(void);

/*
 * Arm a capped auto-revert, then apply the change. Ordering is strict:
 *   1. the slot (revert fn + ctx copy + deadline) is armed first;
 *   2. then `apply` is called;
 *   3. if apply fails, the harness reverts immediately and frees the slot.
 * So the revert is always armed before the change can take effect.
 *
 * `label` names the change for the HUD/dispatcher ("HT40", "MAXPERF", ...).
 * `cap_ms` 0 -> WQ_SAFETY_CAP_MS_DEFAULT.
 * Returns a token >= 0, or a negative WQ_ERR_*.
 */
int wq_safety_arm(const char *label, uint32_t cap_ms,
                  wq_apply_fn apply, wq_revert_fn revert,
                  const void *ctx, uint32_t ctx_len);

/* User confirmed the new state is good: cancel the pending revert. */
int wq_safety_confirm(int token);

/* Abort now: run the revert immediately and free the slot. */
int wq_safety_revert_now(int token);

/* The currently-pending token (most recent), or -1 if none. */
int wq_safety_pending(void);

/* One-line status for the HUD/dispatcher, e.g. "REVERT HT40 in 12s - confirm".
 * Writes at most `cap` bytes incl. NUL. Returns 1 if a revert is pending. */
int wq_safety_status(char *out, uint32_t cap);

#endif /* WIFITURBO_SAFETY_H */
