/*
 * wifiturbo_kernel_api.h - the "SceWifiTurbo" export library the kernel module
 * publishes for the user module to import.
 *
 * Why a kernel module at all: the levers live inside the SceWlanBt kernel
 * driver (checked instruction patches for HT40 and the maxbatt DTIM skip, the
 * driver's own PS flag setter, the join-path roam assist hooks, and the
 * RSSI_INFO callback hook for SNR/NF), none of which userland can reach. The
 * HUD renderer also lives in the kernel module because sceDisplaySetFrameBuf is
 * hookable system-wide only from kernel, which lets the overlay show in every
 * app rather than just SceShell.
 *
 * These functions cross the user->kernel boundary, so pointer arguments are
 * copied in/out with ksceKernelMemcpy{User,Kernel}* inside the kernel (the
 * usual Vita pattern). Keep the structs small and POD.
 *
 * Every function here must also be listed in wifiturbo_kernel.yml and defined
 * in kernel/main.c (or uibridge.c).
 */
#ifndef WIFITURBO_KERNEL_API_H
#define WIFITURBO_KERNEL_API_H

#include "wifiturbo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- power profiles ---------------------------------------------------- */
/* maxbatt DTIM skipping: on != 0 patches SceWlanBt's join-time PS_MODE_ENH
 * builder (FUN_81006014) to send multiple_dtims = 3 instead of 1; on == 0
 * restores it. Takes effect at the next join, so the caller reassociates
 * inside the auto-revert harness. Refuses (WQ_ERR_RESOLVE) on any firmware
 * whose original bytes differ. */
int wqPsSetDtimSkip(int on);
int wqPsGetDtimSkip(void);   /* 1 = patch active */
int wqPsSetFlag(int on);     /* driver PS flag via FUN_81009628(6|7,1); returns new flag */
int wqPsGetFlag(void);       /* 1 on, 0 off, <0 unresolved */

/* ---- 40 MHz-on-2.4 (HT40) ---------------------------------------------- */
/* enable!=0 patches SceWlanBt's two hard-coded 20 MHz immediates (ASSOCIATE HT
 * Cap Info, 11N_CFG ht_tx_cap) so the next association advertises 20/40 MHz;
 * enable==0 releases the patch (kernel/ht40patch.c). A width change can drop
 * the association, so remote changes go through the auto-revert harness.
 * *prev_htcap_out (if non-NULL) receives the advertised HT cap before the
 * change (0x116e if the patch was active, else 0x112c). */
int wqChipSet11nHt40(int enable, uint16_t *prev_htcap_out);

/* ---- HUD renderer (read-only overlay) ----------------------------------- */

/* Enable/disable the sceDisplaySetFrameBuf overlay hook. */
int wqHudSetEnabled(int enabled);

/* Push the latest HUD state for the renderer to draw. Cheap; call from the
 * user module's HUD sampling thread. */
int wqHudSetState(const WqHudState *state);

/* ---- read-only self-test probes ----------------------------------------- */

/* Round-trip cookie returned by wqSelfTestPing(): proves the user->kernel
 * syscall path reaches the kernel module and marshals a return value back.
 * 'WTB1'. */
#define WQ_SELFTEST_MAGIC   0x57544231u

/* Trivial side-effect-free syscall used by `selftest` to confirm the kernel
 * module is loaded and reachable. Always returns WQ_SELFTEST_MAGIC; changes
 * nothing. */
int wqSelfTestPing(void);

/* HUD status bits returned by wqHudGetStatus() - read-only, changes nothing. */
#define WQ_HUD_STATUS_HOOK       0x1 /* sceDisplay framebuffer hook installed   */
#define WQ_HUD_STATUS_ENABLED    0x2 /* overlay currently enabled (sampler on)  */
#define WQ_HUD_STATUS_DRAW_ARMED 0x4 /* framebuffer pixel-write armed           */
int wqHudGetStatus(void);

/* Arm/disarm the kernel framebuffer PIXEL-WRITE. Disarmed at boot; 'hud on'
 * (user/main.c wq_hud_runtime_set) arms it together with the overlay and
 * 'hud off' disarms it again. */
int wqHudSetDrawArmed(int on);

/* SNR / noise floor parsed from the driver's own RSSI_INFO (0xA4) replies by a
 * hook on its completion callback (no command is sent). valid=0 until a reply
 * is seen. The first call installs the hook, so the user side only calls this
 * while connected (after the association burst). */
int wqChipObserveGetLink(WqLinkInternals *out);

/* ---- Settings-app IPC bridge (SceWifiTurbo syscalls, reachable from any
 * process) -----------------------------------------------------------------
 * The Settings app is NPXS10015, a separate process from the resident SceShell
 * core, so the Settings front-end talks to the core through this tiny kernel
 * mailbox. wqUiSubmit/wqUiReadState are called from NPXS10015; wqUiDrain/
 * wqUiPublish from the resident SceShell module (off the keepwifi tick). */
typedef struct {
	int keepwifi;
	int hud;
	int autoreconnect;
	int roam;
	int ht40;
	int profile;    /* 0 maxperf / 1 balanced / 2 maxbatt */
	int devmode;    /* Developer mode: :1339 remote-control listener on.
	                 * Default 0 = off. */
} WqUiState;

#define WQ_UI_CMD_MAX 64              /* command buffer size for wqUiSubmit/Drain */
int wqUiSubmit(const char *line);     /* NPXS10015 -> queue a dispatch command */
int wqUiReadState(WqUiState *out);    /* NPXS10015 -> read the published state */
int wqUiDrain(char *line, int cap);   /* resident  -> pop a queued command (1/0)*/
int wqUiPublish(const WqUiState *s);  /* resident  -> publish current state    */

/* Join-path roam assist (kernel/roamassist.c).
 * Arms the NEXT SceWlanBt join only: full channel sweep + strongest same-SSID
 * BSS first (or BSSID mac0/mac1 first with WQ_RA_TARGET). flags = 0 disarms.
 * Follow with a real reassociation (wq_net_reassociate). */
int wqRoamAssistArm(unsigned flags, unsigned mac0, unsigned mac1);
int wqRoamAssistGetStatus(WqRoamAssistStatus *out);

#ifdef __cplusplus
}
#endif

#endif /* WIFITURBO_KERNEL_API_H */
