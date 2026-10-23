/*
 * uibridge.c - a tiny kernel mailbox so the Settings app (NPXS10015, a separate
 * process) can drive the resident SceShell core. Exposed through the existing
 * SceWifiTurbo syscall library, which is reachable from any process, so it needs
 * no network and survives Wi-Fi down.
 *
 *   NPXS10015 (wifiturbo.suprx, Settings hooks): wqUiSubmit(cmd), wqUiReadState(&st)
 *   SceShell  (wifiturbo.suprx, off the tick): wqUiDrain(&cmd) -> wq_dispatch_exec,
 *                                              wqUiPublish(&st)
 *
 * A small command ring + one published state snapshot, sized for the low-rate
 * Settings UI.
 */
#include <psp2kern/kernel/sysmem.h>    /* ksceKernelMemcpy{User,Kernel}         */
#include <psp2kern/kernel/sysclib.h>   /* memcpy/memset                         */

#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"   /* WQ_UI_CMD_MAX, WqUiState      */

/* Single-producer (NPXS10015) / single-consumer (SceShell) command ring, so a
 * burst of Settings toggles in one pump window isn't collapsed to the last one
 * (that lost every change but the final toggle). */
#define WQ_UI_RING 16
static char              g_ring[WQ_UI_RING][WQ_UI_CMD_MAX];
static volatile unsigned g_head, g_tail;
static WqUiState         g_state;

/* NPXS10015 side: enqueue one command (caller passes a WQ_UI_CMD_MAX user buffer). */
int wqUiSubmit(const char *line)
{
	char k[WQ_UI_CMD_MAX];
	if (!line || ksceKernelMemcpyUserToKernel(k, line, sizeof(k)) < 0)
		return WQ_ERR_ARG;
	k[WQ_UI_CMD_MAX - 1] = 0;
	unsigned h = g_head;
	if (h - g_tail >= WQ_UI_RING)
		return WQ_ERR_NOT_READY;              /* ring full - drop */
	memcpy(g_ring[h % WQ_UI_RING], k, WQ_UI_CMD_MAX);
	g_head = h + 1;
	return WQ_OK;
}

/* NPXS10015 side: read the state the resident core last published. */
int wqUiReadState(WqUiState *out)
{
	if (!out)
		return WQ_ERR_ARG;
	ksceKernelMemcpyKernelToUser(out, &g_state, sizeof(g_state));
	return WQ_OK;
}

/* Resident side: pop a queued command into a kernel-copied user buffer.
 * Returns 1 if a command was waiting, 0 otherwise. */
int wqUiDrain(char *line, int cap)
{
	if (!line || cap <= 0 || g_tail == g_head)
		return 0;
	int n = cap < WQ_UI_CMD_MAX ? cap : WQ_UI_CMD_MAX;
	ksceKernelMemcpyKernelToUser(line, g_ring[g_tail % WQ_UI_RING], n);
	g_tail = g_tail + 1;
	return 1;
}

/* Resident side: publish the current wifiturbo state for the Settings UI. */
int wqUiPublish(const WqUiState *s)
{
	WqUiState k;
	if (!s || ksceKernelMemcpyUserToKernel(&k, s, sizeof(k)) < 0)
		return WQ_ERR_ARG;
	g_state = k;
	return WQ_OK;
}
