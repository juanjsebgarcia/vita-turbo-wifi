/*
 * log.h - user-side logger for wifiturbo (the SceShell user module).
 *
 * Goal: make bring-up debuggable. Every interesting event and every error path
 * writes a timestamped line to BOTH a small in-memory ring buffer (dumpable
 * instantly over the dispatcher with no disk) AND, best-effort, an append-only
 * file on disk with simple size-capped rotation.
 *
 * Design constraints (in the spirit of user/safety.c):
 *   - Never crash and never block the caller: every IO call is failure-tolerant,
 *     lines are bounded, and if the log file cannot be opened we silently fall
 *     back to the ring buffer only.
 *   - Safe to call from any user thread: one mutex guards the ring + the fd.
 *   - NEVER log key material. SSIDs and BSSIDs are fine; a passphrase/PSK must
 *     never be passed to these functions. The plugin never reads a stored PSK,
 *     so there is nothing to redact in practice - this is a standing rule for
 *     anyone adding log sites.
 *
 * The formatter is a small self-contained printf-subset (%s %d %i %u %x %X %c
 * %p %%, with optional zero-pad width like %02x / %08x), so the logger pulls in
 * no libc printf dependency and matches the rest of the module's hand-rolled
 * string building.
 */
#ifndef WIFITURBO_LOG_H
#define WIFITURBO_LOG_H

#include <stdint.h>

/* Severity levels, most severe first (a higher runtime level enables more). */
#define WQ_LOG_ERR    0
#define WQ_LOG_WARN   1
#define WQ_LOG_INFO   2
#define WQ_LOG_DEBUG  3
#define WQ_LOG_TRACE  4

/* Compile-time default runtime level. INFO keeps the 1 Hz sampler / polled
 * failure paths (logged at DEBUG/TRACE) quiet unless raised on-device with
 * `log level debug`. Override with -DWQ_LOG_DEFAULT_LEVEL=... at build time. */
#ifndef WQ_LOG_DEFAULT_LEVEL
#define WQ_LOG_DEFAULT_LEVEL  WQ_LOG_INFO
#endif

/* Ring buffer + line geometry + on-disk sizing. */
#define WQ_LOG_RING_LINES  128
#define WQ_LOG_LINE_MAX    160
#define WQ_LOG_MAX_BYTES   (128 * 1024)   /* rotate .log -> .log.1 past this     */

#define WQ_LOG_DIR     "ur0:data/wifiturbo"
#define WQ_LOG_PATH    WQ_LOG_DIR "/wifiturbo.log"
#define WQ_LOG_PATH_1  WQ_LOG_DIR "/wifiturbo.log.1"

/* Bring the logger up (creates the mutex, the dir, opens the file best-effort)
 * and tear it down. Both are safe to call once; init must run before any log
 * call for the file path to be live, but logging before init still no-ops
 * safely (it just drops the line). */
void wq_log_init(void);
void wq_log_fini(void);

/* Runtime level get/set (the `log level ...` dispatcher command). */
void wq_log_set_level(int level);
int  wq_log_get_level(void);
const char *wq_log_level_name(int level);

/* The log call. Does nothing if `level` is above the current runtime level. */
void wq_logf(int level, const char *fmt, ...);

#define WQ_LOGE(...) wq_logf(WQ_LOG_ERR,   __VA_ARGS__)
#define WQ_LOGW(...) wq_logf(WQ_LOG_WARN,  __VA_ARGS__)
#define WQ_LOGI(...) wq_logf(WQ_LOG_INFO,  __VA_ARGS__)
#define WQ_LOGD(...) wq_logf(WQ_LOG_DEBUG, __VA_ARGS__)
#define WQ_LOGT(...) wq_logf(WQ_LOG_TRACE, __VA_ARGS__)

/* Dump the in-memory ring buffer (oldest first) into out, NUL-terminated and
 * bounded by outcap. Returns the number of bytes written. */
int  wq_log_dump(char *out, int outcap);

/* Clear the in-memory ring buffer (the `log clear` command). The on-disk file
 * is left intact. */
void wq_log_clear(void);

#endif /* WIFITURBO_LOG_H */
