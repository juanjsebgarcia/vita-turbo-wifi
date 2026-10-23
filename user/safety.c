/*
 * safety.c - capped auto-revert watchdog.
 *
 * Implementation notes:
 *   - A single watchdog thread (created at init, alive for the module's whole
 *     lifetime) polls the armed slots every WQ_POLL_MS and fires any whose
 *     deadline has passed. It uses sceKernelGetSystemTimeWide() for timing,
 *     which is independent of the network, so a severed link cannot stop a
 *     revert from firing.
 *   - Slots are guarded by a mutex. apply/revert never run while the mutex is
 *     held (we copy out what we need, drop the lock, then call), so a slow
 *     chip command cannot stall the whole table.
 *   - The revert context is copied into the slot, so the caller's stack/heap
 *     need not outlive the arm call.
 */
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <string.h>

#include "safety.h"
#include "log.h"

#define WQ_POLL_MS   200

typedef struct {
	int           in_use;
	int           token;
	char          label[24];
	uint64_t      deadline_us;   /* absolute, from GetSystemTimeWide          */
	uint32_t      cap_ms;
	wq_revert_fn  revert;
	uint8_t       ctx[WQ_SAFETY_CTX_MAX];
} Slot;

static Slot     g_slots[WQ_SAFETY_SLOTS];
static SceUID   g_mtx = -1;
static SceUID   g_thread = -1;
static int      g_run = 0;
static int      g_next_token = 1;

static uint64_t now_us(void)
{
	return (uint64_t)sceKernelGetSystemTimeWide();
}

static void lock(void)   { if (g_mtx >= 0) sceKernelLockMutex(g_mtx, 1, NULL); }
static void unlock(void) { if (g_mtx >= 0) sceKernelUnlockMutex(g_mtx, 1); }

/* The watchdog: fire expired reverts. Runs forever until g_run clears. */
static int watchdog_thread(SceSize args, void *argp)
{
	(void)args; (void)argp;
	while (g_run) {
		for (;;) {
			/* Pull one expired slot out under the lock, then release and
			 * revert so the chip call happens unlocked. Loop until none. */
			wq_revert_fn fn = NULL;
			uint8_t ctx[WQ_SAFETY_CTX_MAX];
			char label[24]; label[0] = 0;
			int token = 0;
			lock();
			uint64_t t = now_us();
			int found = -1;
			for (int i = 0; i < WQ_SAFETY_SLOTS; i++) {
				if (g_slots[i].in_use && t >= g_slots[i].deadline_us) {
					found = i;
					break;
				}
			}
			if (found >= 0) {
				fn = g_slots[found].revert;
				memcpy(ctx, g_slots[found].ctx, sizeof(ctx));
				strncpy(label, g_slots[found].label, sizeof(label) - 1);
				label[sizeof(label) - 1] = 0;
				token = g_slots[found].token;
				memset(&g_slots[found], 0, sizeof(g_slots[found]));
			}
			unlock();

			if (found < 0)
				break;
			/* WHY: timeout - the user did not 'confirm' within the cap. */
			WQ_LOGW("safety: REVERT '%s' token=%d reason=timeout (no confirm)",
			        label, token);
			if (fn) {
				int rr = fn(ctx);   /* restore previous state - link not required */
				if (rr != WQ_OK)
					WQ_LOGE("safety: revert '%s' token=%d FAILED rc=%d",
					        label, token, rr);
			}
		}
		sceKernelDelayThread(WQ_POLL_MS * 1000);
	}
	return 0;
}

int wq_safety_init(void)
{
	memset(g_slots, 0, sizeof(g_slots));
	g_mtx = sceKernelCreateMutex("WqSafetyMtx", 0, 0, NULL);
	if (g_mtx < 0)
		return WQ_ERR_NOT_READY;

	g_run = 1;
	g_thread = sceKernelCreateThread("WqSafetyWatchdog", watchdog_thread,
	                                 0x40, 0x2000, 0, 0, NULL);
	if (g_thread < 0) {
		g_run = 0;
		sceKernelDeleteMutex(g_mtx);
		g_mtx = -1;
		return WQ_ERR_NOT_READY;
	}
	sceKernelStartThread(g_thread, 0, NULL);
	return WQ_OK;
}

void wq_safety_fini(void)
{
	/* On teardown, fire any still-pending reverts so we never leave a risky
	 * change applied. Then stop the watchdog. */
	lock();
	for (int i = 0; i < WQ_SAFETY_SLOTS; i++) {
		if (g_slots[i].in_use && g_slots[i].revert) {
			wq_revert_fn fn = g_slots[i].revert;
			uint8_t ctx[WQ_SAFETY_CTX_MAX];
			int token = g_slots[i].token;
			memcpy(ctx, g_slots[i].ctx, sizeof(ctx));
			memset(&g_slots[i], 0, sizeof(g_slots[i]));
			unlock();
			WQ_LOGW("safety: REVERT token=%d reason=module-unload", token);
			fn(ctx);
			lock();
		}
	}
	unlock();

	g_run = 0;
	if (g_thread >= 0) {
		sceKernelWaitThreadEnd(g_thread, NULL, NULL);
		sceKernelDeleteThread(g_thread);
		g_thread = -1;
	}
	if (g_mtx >= 0) {
		sceKernelDeleteMutex(g_mtx);
		g_mtx = -1;
	}
}

int wq_safety_arm(const char *label, uint32_t cap_ms,
                  wq_apply_fn apply, wq_revert_fn revert,
                  const void *ctx, uint32_t ctx_len)
{
	if (!revert || (ctx_len && !ctx) || ctx_len > WQ_SAFETY_CTX_MAX)
		return WQ_ERR_ARG;
	if (cap_ms == 0)
		cap_ms = WQ_SAFETY_CAP_MS_DEFAULT;

	lock();
	int slot = -1;
	for (int i = 0; i < WQ_SAFETY_SLOTS; i++)
		if (!g_slots[i].in_use) { slot = i; break; }
	if (slot < 0) {
		unlock();
		return WQ_ERR_BUSY;
	}

	Slot *s = &g_slots[slot];
	memset(s, 0, sizeof(*s));
	s->in_use = 1;
	s->token = g_next_token++;
	if (label) {
		strncpy(s->label, label, sizeof(s->label) - 1);
		s->label[sizeof(s->label) - 1] = 0;
	}
	s->cap_ms = cap_ms;
	s->deadline_us = now_us() + (uint64_t)cap_ms * 1000;
	s->revert = revert;
	if (ctx_len)
		memcpy(s->ctx, ctx, ctx_len);
	int token = s->token;
	unlock();

	WQ_LOGI("safety: ARM '%s' token=%d cap_ms=%u deadline_us=%u (slot %d)",
	        label ? label : "?", token, cap_ms, (unsigned)s->deadline_us, slot);

	/* The slot is armed with a live deadline BEFORE apply runs. If apply fails,
	 * revert immediately and free the slot. */
	if (apply) {
		int r = apply(s->ctx);
		if (r != WQ_OK) {
			WQ_LOGW("safety: apply '%s' token=%d FAILED rc=%d -> reverting now",
			        label ? label : "?", token, r);
			wq_safety_revert_now(token);
			return r;
		}
		WQ_LOGI("safety: apply '%s' token=%d OK", label ? label : "?", token);
	}
	return token;
}

static int find_token(int token)
{
	for (int i = 0; i < WQ_SAFETY_SLOTS; i++)
		if (g_slots[i].in_use && g_slots[i].token == token)
			return i;
	return -1;
}

int wq_safety_confirm(int token)
{
	char label[24]; label[0] = 0;
	lock();
	int i = find_token(token);
	if (i >= 0) {
		strncpy(label, g_slots[i].label, sizeof(label) - 1);
		label[sizeof(label) - 1] = 0;
		memset(&g_slots[i], 0, sizeof(g_slots[i])); /* keep the new state      */
	}
	unlock();
	if (i >= 0)
		WQ_LOGI("safety: CONFIRM '%s' token=%d - change kept", label, token);
	else
		WQ_LOGW("safety: confirm token=%d not found", token);
	return i >= 0 ? WQ_OK : WQ_ERR_ARG;
}

int wq_safety_revert_now(int token)
{
	wq_revert_fn fn = NULL;
	uint8_t ctx[WQ_SAFETY_CTX_MAX];
	char label[24]; label[0] = 0;

	lock();
	int i = find_token(token);
	if (i >= 0) {
		fn = g_slots[i].revert;
		memcpy(ctx, g_slots[i].ctx, sizeof(ctx));
		strncpy(label, g_slots[i].label, sizeof(label) - 1);
		label[sizeof(label) - 1] = 0;
		memset(&g_slots[i], 0, sizeof(g_slots[i]));
	}
	unlock();

	if (i < 0) {
		WQ_LOGW("safety: revert token=%d not found", token);
		return WQ_ERR_ARG;
	}
	/* WHY: explicit - user 'revert', or an apply() failure rolling itself back. */
	WQ_LOGW("safety: REVERT '%s' token=%d reason=explicit", label, token);
	if (fn) {
		int rr = fn(ctx);
		if (rr != WQ_OK)
			WQ_LOGE("safety: revert '%s' token=%d FAILED rc=%d", label, token, rr);
	}
	return WQ_OK;
}

int wq_safety_pending(void)
{
	lock();
	int best = -1;
	uint64_t best_deadline = 0;
	for (int i = 0; i < WQ_SAFETY_SLOTS; i++) {
		if (g_slots[i].in_use && g_slots[i].deadline_us >= best_deadline) {
			best_deadline = g_slots[i].deadline_us;
			best = g_slots[i].token;
		}
	}
	unlock();
	return best;
}

int wq_safety_status(char *out, uint32_t cap)
{
	if (!out || cap < 8)
		return 0;
	lock();
	int i = -1;
	uint64_t soonest = (uint64_t)-1;
	for (int j = 0; j < WQ_SAFETY_SLOTS; j++) {
		if (g_slots[j].in_use && g_slots[j].deadline_us < soonest) {
			soonest = g_slots[j].deadline_us;
			i = j;
		}
	}
	if (i < 0) {
		unlock();
		out[0] = 0;
		return 0;
	}
	uint64_t t = now_us();
	int secs = soonest > t ? (int)((soonest - t) / 1000000) : 0;

	/* "REVERT <label> in <n>s - confirm" without printf. */
	char tmp[48];
	char *p = tmp, *end = tmp + sizeof(tmp);
	const char *pre = "REVERT ";
	while (*pre && p < end - 1) *p++ = *pre++;
	const char *l = g_slots[i].label;
	while (*l && p < end - 1) *p++ = *l++;
	const char *mid = " in ";
	while (*mid && p < end - 1) *p++ = *mid++;
	/* int -> decimal */
	char num[8]; int n = 0, v = secs;
	if (!v) num[n++] = '0';
	while (v) { num[n++] = '0' + (v % 10); v /= 10; }
	while (n && p < end - 1) *p++ = num[--n];
	const char *suf = "s-confirm";
	while (*suf && p < end - 1) *p++ = *suf++;
	*p = 0;
	unlock();

	strncpy(out, tmp, cap - 1);
	out[cap - 1] = 0;
	return 1;
}
