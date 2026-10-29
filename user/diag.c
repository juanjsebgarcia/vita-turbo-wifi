/*
 * diag.c - read-only diagnostics.
 *
 * SceNetCtl side (header psp2/net/netctl.h):
 *   sceNetCtlInetGetState  -> 0..3
 *   sceNetCtlInetGetInfo   -> RSSI dBm/%, channel, SSID, BSSID, link
 *   sceNetCtlInetGetResult -> last error/reason code
 * These are the clean, public userland APIs; they need no RE and no risk.
 *
 * Driver side (via the SceWifiTurbo kernel export):
 *   wqChipObserveGetLink -> SNR / noise floor from the driver's own RSSI_INFO
 *                           replies (a callback hook; no command is sent).
 *
 * The netctl callback records the last drop reason (shown on the HUD); the
 * daemon posts events that surface through sceNetCtlCheckCallback.
 */
#include <psp2/net/netctl.h>
#include <psp2/net/net.h>
#include <string.h>

#include "diag.h"
#include "log.h"
#include "../include/wifiturbo_kernel_api.h"

static int g_inited = 0;
static int g_own_init = 0;     /* our sceNetCtlInit succeeded: we must Term it */
static int g_cb_id = -1;
static volatile int g_last_result = 0;
static int g_init_rc = 0;      /* last sceNetCtlInit return (for selftest)   */

/* sceNetCtlInit's "already initialised" error: SceShell owns netctl and has
 * initialised it, so the state reads still work. */
#define WQ_NETCTL_ERR_ALREADY_INITED  0x80412102u

/* 1 once SceShell has initialised SceNetCtl itself: a state read only succeeds
 * then (before it, it fails with NOT_INITIALIZED, 0x80412101). The bring-up
 * waits on this so diag_init's sceNetCtlInit is never the process's first -
 * being first, during a slow boot, wedged it at the boot warning screen. */
int diag_netctl_ready(void)
{
	int state = WQ_NETCTL_DISCONNECTED;
	return sceNetCtlInetGetState(&state) == 0;
}

/* Event ids from the netctl callback. On an event we read the reason via
 * GetResult and keep it for the HUD's "last drop" line. */
static void netctl_cb(int event_type, void *arg)
{
	(void)arg;
	int state = WQ_NETCTL_DISCONNECTED;
	int rs = sceNetCtlInetGetState(&state);
	if (rs != 0)
		WQ_LOGD("diag: sceNetCtlInetGetState failed rc=0x%08x", (unsigned)rs);

	/* GetResult takes the event id and returns the reason/error. WQ_REASON_*
	 * are the known reason codes (include/wifiturbo.h). */
	int err = 0;
	int rr = sceNetCtlInetGetResult(event_type, &err);
	if (rr == 0) {
		g_last_result = err;
		if (err)
			WQ_LOGI("diag: netctl event=%d reason=0x%08x state=%d",
			        event_type, (unsigned)err, state);
	} else {
		WQ_LOGD("diag: sceNetCtlInetGetResult(event=%d) failed rc=0x%08x",
		        event_type, (unsigned)rr);
	}
}

int diag_init(void)
{
	if (g_inited)
		return WQ_OK;
	/* The bring-up waits for SceShell's own init (diag_netctl_ready), so
	 * "already initialised" is the normal result here: the callback still
	 * registers, and netctl is SceShell's to terminate, not ours. */
	int ri = sceNetCtlInit();
	g_init_rc = ri;
	if (ri < 0 && (unsigned)ri != WQ_NETCTL_ERR_ALREADY_INITED) {
		WQ_LOGW("diag_init: sceNetCtlInit failed rc=0x%08x", (unsigned)ri);
		return WQ_ERR_NOT_READY;
	}
	int rc = sceNetCtlInetRegisterCallback(netctl_cb, NULL, &g_cb_id);
	if (rc < 0)
		WQ_LOGW("diag_init: RegisterCallback rc=0x%08x", (unsigned)rc);
	g_own_init = ri == 0;
	g_inited = 1;
	return WQ_OK;
}

void diag_fini(void)
{
	if (!g_inited)
		return;
	if (g_cb_id >= 0)
		sceNetCtlInetUnregisterCallback(g_cb_id);
	if (g_own_init)
		sceNetCtlTerm();
	g_own_init = 0;
	g_inited = 0;
}

/* Read-only probe for `selftest`: confirm SceNetCtl is initialised and the
 * state read works. Returns WQ_OK (and *state_out, if given) or WQ_ERR_*.
 * sceNetCtlInit failing with "already initialised" is not a failure: SceShell
 * initialised netctl itself, and the state reads below still work. */
int diag_selftest(int *state_out)
{
	if (!g_inited && (unsigned)g_init_rc != WQ_NETCTL_ERR_ALREADY_INITED)
		return WQ_ERR_NOT_READY;
	int state = WQ_NETCTL_DISCONNECTED;
	int r = sceNetCtlInetGetState(&state);
	if (r != 0) {
		WQ_LOGW("diag_selftest: GetState failed rc=0x%08x", (unsigned)r);
		return WQ_ERR_CHIP;
	}
	if (state_out)
		*state_out = state;
	return WQ_OK;
}

int diag_sample(WqHudState *out)
{
	if (!out)
		return WQ_ERR_ARG;
	memset(out, 0, sizeof(*out));
	out->valid = 1;

	/* Pump the callback so g_last_* and GetInfo reflect the current link. */
	sceNetCtlCheckCallback();

	int state = WQ_NETCTL_DISCONNECTED;
	sceNetCtlInetGetState(&state);
	out->netctl_state = state;
	out->last_result = g_last_result;

	SceNetCtlInfo info;
	if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_RSSI_DBM, &info) == 0)
		out->rssi_dbm = (int)info.rssi_dbm;
	if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_RSSI_PERCENTAGE, &info) == 0)
		out->rssi_pct = info.rssi_percentage;
	if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_CHANNEL, &info) == 0)
		out->channel = info.channel;
	if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_SSID, &info) == 0) {
		strncpy(out->ssid, info.ssid, WQ_SSID_MAX);
		out->ssid[WQ_SSID_MAX] = 0;
	}
	if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_BSSID, &info) == 0)
		memcpy(out->bssid, info.bssid.data, 6);

	/* SNR / noise floor from the driver's own RSSI_INFO replies (kernel hook on
	 * its completion callback; no chip command). Only while connected, so the
	 * hook is first installed after the association burst. Retry/fail/beacon-
	 * miss counters have no source (the driver never sends GET_LOG), so they
	 * are not reported at all. */
	if (state == WQ_NETCTL_CONNECTED) {
		WqLinkInternals li;
		if (wqChipObserveGetLink(&li) == WQ_OK && li.noise_floor_dbm < 0) {
			out->snr_db    = li.snr_db;
			out->nf_dbm    = li.noise_floor_dbm;
			out->snr_valid = 1;
		}
	}
	return WQ_OK;
}
