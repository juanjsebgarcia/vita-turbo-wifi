/*
 * klog.h - kernel-side logger for the SceWifiTurbo kernel module.
 *
 * The kernel module cannot open ux0 the same way the user module does at will,
 * so the primary sink is ksceDebugPrintf with a "[wifiturbo]" tag (visible over
 * a kernel debug channel / PSM / udcd console). In addition we open a kernel
 * logfile at ur0:data/wifiturbo/wifiturbo_kernel.log via ksceIo* when feasible,
 * best-effort and failure-tolerant.
 *
 * HOT-PATH RULE: the sceDisplay framebuffer hook runs every frame. Do NOT call
 * these from inside it beyond a single one-shot "first frame" line (and that one
 * uses ksceDebugPrintf directly, never the file path - see kernel/hud.c).
 *
 * Levels mirror the user logger. The self-contained formatter means no libc.
 */
#ifndef WIFITURBO_KLOG_H
#define WIFITURBO_KLOG_H

#include <stdint.h>

#define WQ_KLOG_ERR    0
#define WQ_KLOG_WARN   1
#define WQ_KLOG_INFO   2
#define WQ_KLOG_DEBUG  3
#define WQ_KLOG_TRACE  4

#ifndef WQ_KLOG_DEFAULT_LEVEL
#define WQ_KLOG_DEFAULT_LEVEL  WQ_KLOG_DEBUG   /* ksceDebugPrintf is cheap        */
#endif

#define WQ_KLOG_DIR   "ur0:data/wifiturbo"
#define WQ_KLOG_PATH  WQ_KLOG_DIR "/wifiturbo_kernel.log"
#define WQ_KLOG_MAX_BYTES  (128 * 1024)

void wq_klog_init(void);
void wq_klog_fini(void);

void wq_klog(int level, const char *fmt, ...);

#define WQ_KLOGE(...) wq_klog(WQ_KLOG_ERR,   __VA_ARGS__)
#define WQ_KLOGW(...) wq_klog(WQ_KLOG_WARN,  __VA_ARGS__)
#define WQ_KLOGI(...) wq_klog(WQ_KLOG_INFO,  __VA_ARGS__)
#define WQ_KLOGD(...) wq_klog(WQ_KLOG_DEBUG, __VA_ARGS__)
#define WQ_KLOGT(...) wq_klog(WQ_KLOG_TRACE, __VA_ARGS__)

#endif /* WIFITURBO_KLOG_H */
