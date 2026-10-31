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

/* EXPERIMENTAL: the Wi-Fi chip's receive buffer (kernel/rxring.c). The chip's
 * single receive ring holds only ~12 full-size frames; a same-length edit of
 * the chip firmware SceWlanBt keeps in memory (with the touched download
 * blocks' checksums recomputed) grows it to 127 or 149 slots, shrinking a
 * transmit buffer pool for the space. No code hooks. It takes effect at the
 * next chip power-cycle (Wi-Fi and Bluetooth off, then on; user/rxcycle.c).
 * Fully reversible to the exact original bytes.
 *
 * op low byte = sub-op; high bits (op >> 8) = size argument for APPLY. */
#define WQ_RXRING_OP_STATUS   0   /* read status/counters, change nothing     */
#define WQ_RXRING_OP_APPLY    1   /* | size << 8: apply 127, 149 or 171 slots  */
#define WQ_RXRING_OP_RESTORE  2   /* restore the stock 83-slot image           */

/* Latched until reboot once the cached image is not what we expect; the first
 * reason wins. Operational failures (mutex) set last_rc but do not
 * latch, so they can be retried. */
#define WQ_RXRING_WHY_NONE      0
#define WQ_RXRING_WHY_RESOLVE   1   /* SceWlanBt / its data segment not found   */
#define WQ_RXRING_WHY_NOTLOADED 2   /* the image-loaded flag (seg1+0x230) is 0  */
#define WQ_RXRING_WHY_SIZE      3   /* image size is neither known size         */
#define WQ_RXRING_WHY_PTR       4   /* image pointer != its memblock base       */
#define WQ_RXRING_WHY_DELTA     5   /* no single code delta (p223/p226) fits    */
#define WQ_RXRING_WHY_BYTES     6   /* an original byte at a patch site differs */
#define WQ_RXRING_WHY_CRC       7   /* a touched download chunk's CRC is wrong  */
#define WQ_RXRING_WHY_MUTEX     8   /* could not take SceWlanBt's power mutex   */
#define WQ_RXRING_WHY_REMAP     9   /* unused                                   */

typedef struct WqRxRingStats {
	unsigned present;      /* 1 = SceWlanBt and its data segment resolved      */
	unsigned build;        /* 223 (3.60-3.65) or 226 (3.67-3.74) from the delta, 0 = unknown */
	unsigned loaded;       /* seg1+0x230: the image-loaded flag                */
	unsigned img_addr;     /* seg1+0x228: cached image pointer                 */
	unsigned img_size;     /* seg1+0x22c: cached image size                    */
	unsigned power;        /* seg1+0x4: power word (bit0 chip, bit1 WLAN, bit2 BT) */
	unsigned slots;        /* ring slot count implied by the applied variant: 83/127/149/171 */
	unsigned applied;      /* 0 stock, 127, 149 or 171                         */
	unsigned delta;        /* code delta used (0 or 0xC8), 0xFFFFFFFF = undetermined */
	unsigned chunks;       /* download chunks rewritten by the last apply/restore */
	unsigned why;          /* WQ_RXRING_WHY_* (latched)                        */
	unsigned why_detail;   /* failing chip address or field                    */
	int      last_rc;      /* return code of the last op                       */
} WqRxRingStats;
int wqRxRing(int op, WqRxRingStats *out);   /* WQ_RXRING_OP_*; WQ_OK or WQ_ERR_* */
int wqForceReboot(void);   /* EXPERIMENTAL: syscon reset, rxcycle watchdog last resort */

/* EXPERIMENTAL: a chosen MAC address for the Wi-Fi interface (kernel/
 * macaddr.c). SET goes through the driver's own get-MAC control call with its
 * GET turned into a SET; on success the driver's copy and SceNetPs's wlan0
 * follow. The chip returns to its factory address at every power-up, so the
 * caller re-sends after each one (powerups counts them). mac0 = bytes 0..3,
 * mac1 = bytes 4..5, as for wqRoamAssistArm. Always fills *out. */
#define WQ_MAC_OP_STATUS    0   /* read state; installs nothing, walks nothing  */
                                /* 1: unused                                    */
#define WQ_MAC_OP_SET       2   /* SET mac0/mac1 now (link should be down)      */
#define WQ_MAC_OP_HOOK      3   /* install the hooks (if not yet), then STATUS  */
#define WQ_MAC_OP_WALK      4   /* HOOK, plus a fresh ctx->wlan0 walk           */
/* SET returns WQ_ERR_BUSY while the chip is off, powering up, or
 * within WQ_MAC_SETTLE_MS of a power-up: retry later. */
#define WQ_MAC_SETTLE_MS    500

typedef struct WqMacStatus {
	uint32_t hooked;        /* both hooks installed                            */
	uint32_t ctx_seen;      /* driver context known (any command since boot)   */
	uint32_t ctl_check;     /* 0 ok; 1 no layout/ctx, 2 desc[0x14] is not the
	                         * control entry, 3 desc+0x6c is not the ctx       */
	uint32_t chain;         /* 0 = ctx->wlan0 walk OK, else the failing step   */
	uint32_t powerups;      /* chip power-ups seen since the hooks went in     */
	uint32_t sets;          /* SETs the chip was sent                          */
	uint32_t have_factory;
	int32_t  last_rc;       /* control call return of the last SET             */
	uint8_t  drv[6];        /* driver's copy now (ctx+0x728)                   */
	uint8_t  lladdr[6];     /* SceNetPs wlan0 link-level address now           */
	uint8_t  descmac[6];    /* address in the attach descriptor now            */
	uint8_t  factory[6];    /* the console's own address                       */
	uint8_t  live[6];       /* address the chip reported in the last SET       */
	uint8_t  pad[2];
} WqMacStatus;
int wqMacAddr(int op, unsigned mac0, unsigned mac1, WqMacStatus *out);

/* A small negative WQ_ERR_* returned by a kernel export reaches user code
 * with bit 30 cleared (the syscall layer: -5 arrives as 0xBFFFFFFB). Compare
 * kernel results with a WQ_ERR_* value only through this. */
static inline int wq_kret(int r)
{
	return ((unsigned)r >> 16) == 0xBFFFu ? (int)((unsigned)r | 0x40000000u) : r;
}

/* ---- HUD renderer (read-only overlay) ----------------------------------- */

/* Enable/disable the sceDisplaySetFrameBuf overlay hook. */
int wqHudSetEnabled(int enabled);

/* Push the latest HUD state for the renderer to draw. Cheap; call from the
 * user module's HUD sampling thread. */
int wqHudSetState(const WqHudState *state);

/* Set the screen corner the overlay draws in (WQ_HUD_POS_*). Config, not
 * per-frame: called when the "hudpos" setting changes and re-applied at boot.
 * Out-of-range values are ignored. */
int wqHudSetPos(int pos);

/* Register the calling process as one of the system screens the HUD draws over
 * (WQ_HUD_PROC_*). Called by the SceShell module at boot and by the Settings
 * plugin when the Settings app starts. */
int wqHudRegisterProc(int role);

/* In-game HUD: 1 = draw over every process, 0 (default) = only over the
 * registered system processes, so games and apps are left alone. */
int wqHudSetInGame(int on);

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
	int hud;        /* 0 off, 1 Extended, 2 Compact (1 = the old 'on')     */
	int autoreconnect;
	int roam;
	int ht40;
	int profile;    /* 0 maxperf / 1 balanced / 2 maxbatt */
	int devmode;    /* Developer mode: :1339 remote-control listener on.
	                 * Default 0 = off. */
	int rxring;     /* EXPERIMENTAL Wi-Fi receive buffer: 0 default / 1 enhanced
	                 * (127 slots) / 2 maximum (149). */
	int hudpos;     /* WQ_HUD_POS_* - which screen corner the overlay draws in. */
	int hudgame;    /* In-game HUD: 1 = also draw over games and apps. Default 0. */
	int macmode;    /* EXPERIMENTAL MAC address: 0 stock / 1 custom.           */
	int mac0, mac1; /* custom address: bytes 0..3, bytes 4..5 (wqMacAddr form) */
	int fmac0, fmac1; /* the console's own address, same form; 0 = not known
	                 * yet. Last fields: a settings.bin from an older build is
	                 * shorter and the missing tail loads as 0. */
} WqUiState;

#define WQ_UI_CMD_MAX 64              /* command buffer size for wqUiSubmit/Drain */
int wqUiSubmit(const char *line);     /* NPXS10015 -> queue a dispatch command */
int wqUiReadState(WqUiState *out);    /* v0.4.1 callers: the first 40 bytes only */
/* NPXS10015 / quick menu -> read the published state, at most `size` bytes. */
int wqUiReadStateEx(WqUiState *out, unsigned size);
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
