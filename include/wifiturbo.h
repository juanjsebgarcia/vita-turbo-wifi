/*
 * wifiturbo.h - shared constants and types for Wi-Fi Turbo (PS Vita Wi-Fi plugin).
 *
 * Firmware 3.60-3.68 CEX, taiHEN/HENkaku. Homebrew interoperability project for the
 * device owner's own console. This header collects the magic numbers (NIDs,
 * struct offsets, reason codes) the C code relies on, with a short note on
 * what each one is.
 *
 * Layout of the project:
 *   kernel/   SceWifiTurbo kernel module (.skprx) - SceWlanBt patches/hooks
 *             (HT40, power profiles, roam assist, SNR/NF) and the read-only HUD
 *             via a sceDisplay hook. Exposes a user-callable export library
 *             "SceWifiTurbo".
 *   user/     user module (.suprx) injected into SceShell - owns the SceNetCtl
 *             reads, reconnect/roaming, the auto-revert safety harness, the
 *             command dispatcher and the Settings integration point.
 */
#ifndef WIFITURBO_H
#define WIFITURBO_H

#include <stdint.h>

/* Release version. The .yml module versions carry major.minor (0.1); bump all
 * three together. Logged in both modules' init banner so a log pins the build. */
#define WQ_VERSION_STR    "0.2.0-alpha"

#define WQ_SSID_MAX               32          /* each SSID 1..32 chars          */

/* ------------------------------------------------------------------------- */
/* SceNetCtl userland state + GetResult reason codes                          */
/* ------------------------------------------------------------------------- */
#define WQ_NETCTL_DISCONNECTED  0
#define WQ_NETCTL_CONNECTING    1
#define WQ_NETCTL_FINALIZING    2
#define WQ_NETCTL_CONNECTED     3

#define WQ_REASON_DEAUTH        0x80412116 /* deauthenticated by the AP        */
#define WQ_REASON_BEACON_LOST   0x80412117
#define WQ_REASON_DHCP_TIMEOUT  0x8041211B

/* ------------------------------------------------------------------------- */
/* SNR / noise floor from the driver's RSSI_INFO (0xA4) replies              */
/* ------------------------------------------------------------------------- */

/* Filled by the kernel's RSSI_INFO callback hook (kernel/chipobserve.c) and
 * copied to the user module by wqChipObserveGetLink. POD; crosses the syscall
 * boundary, so both modules must be built from this same definition. */
typedef struct WqLinkInternals {
	uint8_t  valid;            /* 1 once a reply with real dBm readings is seen */
	/* The 88W8787 is a 1x1 radio, so "beacon" vs "data" here is the beacon-path
	 * vs data-path RSSI the firmware tracks separately, not two spatial chains. */
	int16_t  noise_floor_dbm;  /* data NF avg (beacon NF if no data)           */
	int16_t  beacon_rssi_dbm;  /* beacon-path RSSI avg                         */
	int16_t  data_rssi_dbm;    /* data-path RSSI avg                           */
	int16_t  snr_db;           /* (beacon RSSI - NF)                           */
} WqLinkInternals;

/* ------------------------------------------------------------------------- */
/* HUD shared state - the user module fills this, the kernel renderer draws it */
/* ------------------------------------------------------------------------- */
typedef struct WqHudState {
	uint8_t  valid;
	int32_t  netctl_state;     /* WQ_NETCTL_*                                  */
	int32_t  last_result;      /* last sceNetCtlInetGetResult reason code      */
	int32_t  rssi_dbm;         /* from sceNetCtlInetGetInfo (RSSI_DBM)         */
	uint32_t rssi_pct;         /* RSSI_PERCENTAGE                              */
	uint32_t channel;          /* GET_CHANNEL                                  */
	int32_t  snr_db;           /* beacon RSSI - NF, from the driver's RSSI_INFO */
	int32_t  nf_dbm;           /* noise floor (dBm), same source               */
	uint32_t snr_valid;        /* 1 once a real RSSI_INFO reply was seen; the  */
	                           /* HUD hides SNR/NF until then (no fake 0s)     */
	char     ssid[WQ_SSID_MAX + 1];
	uint8_t  bssid[6];
	char     profile[16];      /* active power/latency profile name           */
	char     notice[40];       /* transient status line (e.g. revert armed)   */
	int32_t  lang;             /* system language (wq_i18n_lang) for HUD i18n  */
} WqHudState;

/* Profiles exposed by the "never sleep"/latency toggle. */
#define WQ_PROFILE_MAX_PERF    0   /* PS off, auto-deep-sleep off: never sleep */
#define WQ_PROFILE_BALANCED    1   /* default-ish PS                           */
#define WQ_PROFILE_MAX_BATTERY 2   /* aggressive PS                            */

/* Error codes returned across the whole plugin. */
#define WQ_OK                   0
#define WQ_ERR_NOT_READY       -1
#define WQ_ERR_RESOLVE         -2   /* could not resolve a module offset/NID  */
#define WQ_ERR_CHIP            -3   /* host command rejected / timed out      */
#define WQ_ERR_ARG             -4
#define WQ_ERR_BUSY            -5

/* ------------------------------------------------------------------------- */
/* libnetctl private scan verbs (advanced.c scan reader)                     */
/* ------------------------------------------------------------------------- */
/* Resolved at runtime from SceLibNetCtl by NID with taiGetModuleExportFunc;
 * resolution fails safe (the reader reports "n/a" rather than guess). */
#define WQ_NETCTL_NID_SCAN_REQ      0x6A56FC3Au /* (ssids, count, cfg) 0x20010 */
#define WQ_NETCTL_NID_SCAN_COUNT    0x47980B9Bu /* () -> count       0x20011   */
#define WQ_NETCTL_NID_SCAN_REC      0x92EB6B1Au /* (i, rec0x60)      0x20012   */
#define WQ_NETCTL_NID_GETRESULT     0x300E8877u /* (event, &err)    0x20003   */
#define WQ_NETCTL_NID_ARM_EVENT     0xAB1462C5u /* (type) arm IpcInt event     */
#define WQ_NETCTL_EVENT_SCAN_DONE   4           /* SCAN_REQ_FINISHED           */
#define WQ_SHELL_ERR_BUSY           0x80412187u /* shared scan/connect busy    */

/* 0x60-byte scan record field offsets. We read these, never a stored key. */
#define WQ_SCANREC_SIZE         0x60
#define WQ_SCANREC_OFF_BSSID    0x10
#define WQ_SCANREC_OFF_SSID     0x18   /* 0x21 bytes                          */
#define WQ_SCANREC_OFF_SSIDLEN  0x3C
#define WQ_SCANREC_OFF_SECURITY 0x40
#define WQ_SCANREC_OFF_RSSIDBM  0x44
#define WQ_SCANREC_OFF_RSSIPCT  0x48
#define WQ_SCANREC_OFF_CHANNEL  0x4C

/* One parsed scan record (subset of the 0x60-byte record above; we read these
 * fields, NEVER a stored key). Filled by the advanced.c scan reader. */
typedef struct WqScanEntry {
	uint8_t  bssid[6];
	char     ssid[WQ_SSID_MAX + 1];
	int32_t  security;         /* raw sec field (+0x40); class per sec_name()  */
	int32_t  rssi_dbm;         /* signed dBm (+0x44)                           */
	int32_t  channel;          /* (+0x4c)                                      */
} WqScanEntry;

/* ---- join-path roam assist (kernel/roamassist.c)
 * One-shot: armed by the user module right before a disconnect+IcmConnect, it
 * makes the NEXT SceWlanBt join (ioctl 0x50110001, FUN_81006ea0) sweep all
 * channels and try the strongest same-SSID BSS first (or a given BSSID). */
#define WQ_RA_ARM      0x1u   /* arm (else disarm)                              */
#define WQ_RA_TARGET   0x2u   /* prefer the given BSSID when it is in the scan  */

typedef struct WqRoamAssistStatus {
	uint32_t armed;          /* 1 = waiting for the next join                  */
	uint32_t hooked;         /* 1 = both hooks installed                       */
	uint32_t seq;            /* completed assisted joins (bumps once per join) */
	uint32_t expired;        /* arms dropped because they went stale           */
	int32_t  join_ret;       /* FUN_81006ea0 return of the last assisted join  */
	uint32_t scans;          /* last assisted join: scans seen (expect 1)      */
	uint32_t reordered;      /* last assisted join: scans whose order we changed */
	uint32_t n_entries;      /* BSS entries in the last assisted scan          */
	uint32_t n_match;        /* ... of which matched the join SSID             */
	uint32_t target_hit;     /* 1 = the target BSSID was found and put first   */
	uint8_t  orig_ch[4];     /* the shell's channel list we replaced           */
	uint8_t  first_bssid[6]; /* entry the stock driver would have tried first  */
	uint8_t  best_bssid[6];  /* entry we put first                             */
	uint8_t  joined_bssid[6];/* BSS the join actually associated (req+8+0x28)  */
	uint8_t  first_rssi;     /* |dBm| of first_bssid (entry +0x32), 0 unknown  */
	uint8_t  best_rssi;      /* |dBm| of best_bssid                            */
	uint8_t  first_ch;       /* channel byte (entry +0x2e)                     */
	uint8_t  best_ch;
	uint8_t  pad[2];
} WqRoamAssistStatus;

#endif /* WIFITURBO_H */
