/*
 * chipobserve.c - read the SNR / noise floor out of the SceWlanBt driver's own
 * RSSI_INFO (0xA4) replies, by hooking the driver's completion callback for
 * that command. See chipobserve.h.
 *
 * The hook body is HOT PATH (it runs for every RSSI_INFO reply) so it does
 * ZERO I/O - only plain loads and stores into g_link.
 */
#include <taihen.h>

#include "chipobserve.h"
#include "wlanbt.h"
#include "klog.h"

/* RSSI_INFO (0xA4) response body = mwifiex host_cmd_ds_802_11_rssi_info_rsp
 * (mwifiex fw.h): all __le16 signed dBm. */
#define RSP_OFF_NDATA          0x02
#define RSP_OFF_DATA_RSSI_AVG  0x0a
#define RSP_OFF_DATA_NF_AVG    0x0c
#define RSP_OFF_BCN_RSSI_AVG   0x12
#define RSP_OFF_BCN_NF_AVG     0x14

/* Link internals parsed from the RSSI_INFO replies. Written in the hook by
 * plain stores; read via chipobserve_get_link. */
static WqLinkInternals g_link;

/* ---- SNR / noise floor: the driver's own RSSI_INFO completion -------------
 * The driver keeps only the beacon RSSI from each 0xA4 reply (ctx+0x7e0) and
 * drops the noise floor. Its async completion (0x8100ac44) routes cmd 0xA4 to
 * FUN_81004838(node, ctx+0x7d8), with node+0xc = reply body and node+0x10 =
 * body length (0x1e = mwifiex rssi_info_rsp). Hooking THAT callback sees only
 * RSSI_INFO replies - never the association burst or the blocking 0x2f44
 * path. Plain loads + stores only.
 * Installed lazily (chipobserve_rssi_ensure) once the link is up. The
 * callback's offset comes from the firmware's layout (wlanbt.c). */
#define RSSI_RSP_LEN           0x1e

static SceUID          g_rssi_hook = -1;
static tai_hook_ref_t  g_rssi_ref;

static int hook_rssi_cb(int node, int out)
{
	if (node && *(const volatile int *)(uintptr_t)(node + 0x10) == RSSI_RSP_LEN) {
		const volatile int16_t *b =
		    (const volatile int16_t *)(uintptr_t)*(const volatile int *)(uintptr_t)(node + 0xc);
		if (b) {
			int ndata     = b[RSP_OFF_NDATA / 2];
			int data_rssi = b[RSP_OFF_DATA_RSSI_AVG / 2];
			int data_nf   = b[RSP_OFF_DATA_NF_AVG / 2];
			int bcn_rssi  = b[RSP_OFF_BCN_RSSI_AVG / 2];
			int bcn_nf    = b[RSP_OFF_BCN_NF_AVG / 2];
			int nf = (ndata > 0 && data_nf) ? data_nf : bcn_nf;
			if (nf < 0 && bcn_rssi < 0) {          /* both real dBm readings  */
				g_link.beacon_rssi_dbm = (int16_t)bcn_rssi;
				g_link.data_rssi_dbm   = (int16_t)data_rssi;
				g_link.noise_floor_dbm = (int16_t)nf;
				g_link.snr_db          = (int16_t)(bcn_rssi - nf);
				g_link.valid           = 1;
			}
		}
	}
	return TAI_CONTINUE(int, g_rssi_ref, node, out);
}

int chipobserve_rssi_ensure(void)
{
	if (g_rssi_hook >= 0)
		return WQ_OK;
	const WlanbtLayout *l = wlanbt_layout();
	SceUID modid;
	uintptr_t text;
	if (!l || wlanbt_text(l->rssi_cb + 6, &modid, &text) != WQ_OK)
		return WQ_ERR_RESOLVE;
	/* Original prologue: ldr r3,[r0,#0x10]; cmp r3,#0x1e; bne (3.60-3.74). */
	static const uint8_t orig[6] = { 0x03, 0x69, 0x1E, 0x2B, 0x1D, 0xD1 };
	const volatile uint8_t *t = (const volatile uint8_t *)text + l->rssi_cb;
	for (int i = 0; i < 6; i++) {
		if (t[i] != orig[i]) {
			WQ_KLOGW("chipobserve: RSSI_INFO callback bytes mismatch - refusing");
			return WQ_ERR_RESOLVE;
		}
	}
	g_rssi_hook = taiHookFunctionOffsetForKernel(KERNEL_PID, &g_rssi_ref, modid, 0,
	                                             l->rssi_cb, 1, hook_rssi_cb);
	if (g_rssi_hook < 0) {
		WQ_KLOGW("chipobserve: RSSI_INFO callback hook failed (0x%08x)",
		         (unsigned)g_rssi_hook);
		g_rssi_hook = -1;
		return WQ_ERR_RESOLVE;
	}
	WQ_KLOGI("chipobserve: hooked RSSI_INFO callback +0x%x (SNR/NF)", (unsigned)l->rssi_cb);
	return WQ_OK;
}

/* Copy the link internals parsed from the RSSI_INFO replies. valid=0 until a
 * reply carrying real dBm readings has been seen. */
int chipobserve_get_link(WqLinkInternals *out)
{
	*out = g_link;
	return WQ_OK;
}

void chipobserve_fini(void)
{
	if (g_rssi_hook >= 0) {
		taiHookReleaseForKernel(g_rssi_hook, g_rssi_ref);
		g_rssi_hook = -1;
	}
}
