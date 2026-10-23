/*
 * log.c - user-side logger implementation. See log.h for the contract.
 *
 * Robustness rules honoured here:
 *   - A single mutex (g_mtx) serialises the ring buffer and the file descriptor.
 *   - Every IO call is checked and tolerated: a failed open/write never
 *     propagates and never crashes the caller; on a bad fd we keep the ring
 *     buffer working and simply stop touching disk.
 *   - Lines are bounded (WQ_LOG_LINE_MAX) and always NUL-terminated.
 *   - The formatter is self-contained (no libc printf), so there is no hidden
 *     allocation or reentrancy surprise.
 */
#include <psp2/kernel/processmgr.h>   /* sceKernelGetProcessTimeWide            */
#include <psp2/kernel/threadmgr.h>    /* mutex                                  */
#include <psp2/io/fcntl.h>            /* sceIoOpen/Write/Close/Lseek            */
#include <psp2/io/stat.h>            /* sceIoMkdir                              */
#include <stdarg.h>
#include <string.h>

#include "log.h"

/* ---- state -------------------------------------------------------------- */

static SceUID g_mtx = -1;
static int    g_inited = 0;
static int    g_level  = WQ_LOG_DEFAULT_LEVEL;

static SceUID   g_fd = -1;          /* append fd, or -1 for ring-only          */
static int      g_file_disabled = 0;/* set after a hard open failure           */
static uint32_t g_file_bytes = 0;   /* bytes written to the current .log       */

/* Ring buffer: fixed lines, overwrite oldest. */
static char g_ring[WQ_LOG_RING_LINES][WQ_LOG_LINE_MAX];
static int  g_ring_head = 0;        /* next slot to write                      */
static int  g_ring_count = 0;       /* how many valid lines (<= RING_LINES)    */

/* ---- tiny locking ------------------------------------------------------- */

static void lock(void)   { if (g_mtx >= 0) sceKernelLockMutex(g_mtx, 1, NULL); }
static void unlock(void) { if (g_mtx >= 0) sceKernelUnlockMutex(g_mtx, 1); }

/* ---- self-contained printf subset --------------------------------------- */
/* Supports %s %d %i %u %x %X %c %p %%, optional leading '0' + width digits.
 * Always NUL-terminates. Returns the length written (bounded by cap-1). */

static int fmt_uint(char *buf, int cap, int pos, unsigned v, unsigned base,
                    int upper, int width, int zero)
{
	const char *digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	char tmp[16];
	int n = 0;
	if (!v) tmp[n++] = '0';
	while (v && n < (int)sizeof(tmp)) { tmp[n++] = digs[v % base]; v /= base; }
	int pad = width - n;
	char padc = zero ? '0' : ' ';
	while (pad-- > 0 && pos < cap - 1) buf[pos++] = padc;
	while (n && pos < cap - 1) buf[pos++] = tmp[--n];
	return pos;
}

static int fmt_v(char *buf, int cap, const char *fmt, va_list ap)
{
	int pos = 0;
	if (cap <= 0)
		return 0;
	for (const char *f = fmt; *f && pos < cap - 1; f++) {
		if (*f != '%') { buf[pos++] = *f; continue; }
		f++;
		if (*f == 0) break;
		int zero = 0, width = 0;
		if (*f == '0') { zero = 1; f++; }
		while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); f++; }
		switch (*f) {
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s) s = "(null)";
			while (*s && pos < cap - 1) buf[pos++] = *s++;
			break;
		}
		case 'd': case 'i': {
			int v = va_arg(ap, int);
			if (v < 0) {
				if (pos < cap - 1) buf[pos++] = '-';
				pos = fmt_uint(buf, cap, pos, (unsigned)(-(v + 1)) + 1u, 10, 0,
				               width > 0 ? width - 1 : 0, zero);
			} else {
				pos = fmt_uint(buf, cap, pos, (unsigned)v, 10, 0, width, zero);
			}
			break;
		}
		case 'u':
			pos = fmt_uint(buf, cap, pos, va_arg(ap, unsigned), 10, 0, width, zero);
			break;
		case 'x':
			pos = fmt_uint(buf, cap, pos, va_arg(ap, unsigned), 16, 0, width, zero);
			break;
		case 'X':
			pos = fmt_uint(buf, cap, pos, va_arg(ap, unsigned), 16, 1, width, zero);
			break;
		case 'p':
			if (pos < cap - 1) buf[pos++] = '0';
			if (pos < cap - 1) buf[pos++] = 'x';
			pos = fmt_uint(buf, cap, pos,
			               (unsigned)(uintptr_t)va_arg(ap, void *), 16, 0, 8, 1);
			break;
		case 'c': {
			char c = (char)va_arg(ap, int);
			if (pos < cap - 1) buf[pos++] = c;
			break;
		}
		case '%':
			if (pos < cap - 1) buf[pos++] = '%';
			break;
		default:
			if (pos < cap - 1) buf[pos++] = '%';
			if (pos < cap - 1) buf[pos++] = *f;
			break;
		}
	}
	buf[pos] = 0;
	return pos;
}

/* ---- file plumbing (all best-effort) ------------------------------------ */

/* Open (append) the log file, creating the directory. Leaves g_fd = -1 on any
 * failure and marks the file permanently disabled so we stop retrying. Caller
 * holds the lock. */
static void file_open_locked(void)
{
	if (g_file_disabled || g_fd >= 0)
		return;
	/* mkdir is harmless if it already exists; ignore the result. */
	sceIoMkdir("ur0:data", 0777);
	sceIoMkdir(WQ_LOG_DIR, 0777);

	SceUID fd = sceIoOpen(WQ_LOG_PATH,
	                      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
	if (fd < 0) {
		g_file_disabled = 1;   /* fall back to ring-only for the session        */
		return;
	}
	/* Current size, for the rotation threshold. Seek-to-end is cheap and avoids
	 * a stat struct. A negative result just starts the counter at 0. */
	SceOff end = sceIoLseek(fd, 0, SCE_SEEK_END);
	g_file_bytes = end > 0 ? (uint32_t)end : 0;
	g_fd = fd;
}

/* Rotate .log -> .log.1 when it grows past the cap. Caller holds the lock. */
static void file_rotate_locked(void)
{
	if (g_fd >= 0) {
		sceIoClose(g_fd);
		g_fd = -1;
	}
	sceIoRemove(WQ_LOG_PATH_1);          /* ignore if absent                     */
	sceIoRename(WQ_LOG_PATH, WQ_LOG_PATH_1);
	g_file_bytes = 0;
	file_open_locked();                  /* reopens a fresh, empty .log          */
}

/* Append one already-formatted line (without newline) to disk. Caller holds
 * the lock. Failure-tolerant: a bad write disables the file for the session. */
static void file_write_locked(const char *line, int len)
{
	if (g_file_disabled)
		return;
	if (g_fd < 0)
		file_open_locked();
	if (g_fd < 0)
		return;

	if (g_file_bytes + (uint32_t)len + 1 > WQ_LOG_MAX_BYTES)
		file_rotate_locked();
	if (g_fd < 0)
		return;

	int w1 = sceIoWrite(g_fd, line, len);
	int w2 = sceIoWrite(g_fd, "\n", 1);
	if (w1 < 0 || w2 < 0) {
		/* Disk went away (card pulled, full, ...). Keep the ring alive. */
		sceIoClose(g_fd);
		g_fd = -1;
		g_file_disabled = 1;
		return;
	}
	g_file_bytes += (uint32_t)w1 + (uint32_t)w2;

	/* DURABILITY: close the fd after every line so the bytes are committed to
	 * storage immediately. An open fd's writes can sit in a buffer and be lost
	 * if the process hard-hangs or the unit loses power before close - that is
	 * exactly why an earlier boot left a 0-byte wifiturbo.log even though
	 * module_start had run. Log volume is tiny (a handful of lines at startup,
	 * then ~1/s only while the HUD is on), so the reopen cost is irrelevant.
	 * The next write reopens in SCE_O_APPEND mode via file_open_locked(). */
	sceIoClose(g_fd);
	g_fd = -1;
}

/* ---- public API --------------------------------------------------------- */

void wq_log_init(void)
{
	if (g_inited)
		return;
	g_level = WQ_LOG_DEFAULT_LEVEL;
	g_ring_head = 0;
	g_ring_count = 0;
	g_file_disabled = 0;
	g_file_bytes = 0;
	g_fd = -1;
	if (g_mtx < 0)
		g_mtx = sceKernelCreateMutex("WqLogMtx", 0, 0, NULL);
	g_inited = 1;

	lock();
	file_open_locked();          /* best-effort; ring works regardless           */
	unlock();
}

void wq_log_fini(void)
{
	if (!g_inited)
		return;
	lock();
	if (g_fd >= 0) {
		sceIoClose(g_fd);
		g_fd = -1;
	}
	unlock();
	if (g_mtx >= 0) {
		sceKernelDeleteMutex(g_mtx);
		g_mtx = -1;
	}
	g_inited = 0;
}

void wq_log_set_level(int level)
{
	if (level < WQ_LOG_ERR) level = WQ_LOG_ERR;
	if (level > WQ_LOG_TRACE) level = WQ_LOG_TRACE;
	g_level = level;
}

int wq_log_get_level(void) { return g_level; }

const char *wq_log_level_name(int level)
{
	switch (level) {
	case WQ_LOG_ERR:   return "ERR";
	case WQ_LOG_WARN:  return "WARN";
	case WQ_LOG_INFO:  return "INFO";
	case WQ_LOG_DEBUG: return "DEBUG";
	case WQ_LOG_TRACE: return "TRACE";
	default:           return "?";
	}
}

void wq_logf(int level, const char *fmt, ...)
{
	if (!g_inited || level > g_level || !fmt)
		return;

	/* Build "[<s>.<us>] <LVL> <msg>" into a bounded stack line. */
	char line[WQ_LOG_LINE_MAX];
	int pos = 0;

	uint64_t t = (uint64_t)sceKernelGetProcessTimeWide();  /* microseconds       */
	uint32_t secs = (uint32_t)(t / 1000000u);
	uint32_t us   = (uint32_t)(t % 1000000u);
	line[pos++] = '[';
	pos = fmt_uint(line, sizeof(line), pos, secs, 10, 0, 0, 0);
	line[pos++] = '.';
	pos = fmt_uint(line, sizeof(line), pos, us, 10, 0, 6, 1);   /* 6-digit micro  */
	line[pos++] = ']';
	line[pos++] = ' ';
	const char *lvl = wq_log_level_name(level);
	while (*lvl && pos < (int)sizeof(line) - 1) line[pos++] = *lvl++;
	line[pos++] = ' ';

	va_list ap;
	va_start(ap, fmt);
	pos += fmt_v(line + pos, (int)sizeof(line) - pos, fmt, ap);
	va_end(ap);

	lock();
	/* Ring buffer: copy into the head slot, advance. */
	memcpy(g_ring[g_ring_head], line, WQ_LOG_LINE_MAX - 1);   /* line is NUL-terminated */
	g_ring[g_ring_head][WQ_LOG_LINE_MAX - 1] = 0;
	g_ring_head = (g_ring_head + 1) % WQ_LOG_RING_LINES;
	if (g_ring_count < WQ_LOG_RING_LINES)
		g_ring_count++;
	/* Disk (best-effort). */
	file_write_locked(line, pos);
	unlock();
}

int wq_log_dump(char *out, int outcap)
{
	if (!out || outcap < 1)
		return 0;
	out[0] = 0;
	int w = 0;
	lock();
	/* Oldest first: the oldest valid line is RING_LINES back from head when the
	 * ring is full, else slot 0. */
	int start = (g_ring_count < WQ_LOG_RING_LINES)
	            ? 0
	            : g_ring_head;   /* head points at the oldest once wrapped        */
	for (int i = 0; i < g_ring_count; i++) {
		int idx = (start + i) % WQ_LOG_RING_LINES;
		const char *s = g_ring[idx];
		while (*s && w < outcap - 1) out[w++] = *s++;
		if (*s) {                 /* ran out of room mid-line                     */
			out[w] = 0;
			unlock();
			return w;
		}
		if (w < outcap - 1) out[w++] = '\n';
	}
	out[w] = 0;
	unlock();
	return w;
}

void wq_log_clear(void)
{
	lock();
	g_ring_head = 0;
	g_ring_count = 0;
	unlock();
}
