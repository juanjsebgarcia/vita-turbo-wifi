/*
 * klog.c - kernel-side logger implementation. See klog.h.
 *
 * Primary sink: ksceDebugPrintf("[wifiturbo] ...").
 * Secondary sink: an append-only, size-capped ur0 logfile via ksceIo*, opened
 * best-effort and disabled silently on any failure.
 *
 * A single mutex guards the fd. The formatter is a self-contained printf subset
 * (no libc in kernel), identical in shape to the user logger's.
 */
#include <psp2kern/kernel/threadmgr.h>   /* ksceKernelGetSystemTimeWide, mutex   */
#include <psp2kern/kernel/debug.h>       /* ksceDebugPrintf                      */
#include <psp2kern/io/fcntl.h>           /* ksceIoOpen/Write/Close/Lseek/Remove  */
#include <psp2kern/io/stat.h>            /* ksceIoMkdir                          */
#include <stdarg.h>

#include "klog.h"

static SceUID   g_mtx = -1;
static int      g_inited = 0;
static int      g_level  = WQ_KLOG_DEFAULT_LEVEL;
static SceUID   g_fd = -1;
static int      g_file_disabled = 0;
static uint32_t g_file_bytes = 0;

static void lock(void)   { if (g_mtx >= 0) ksceKernelLockMutex(g_mtx, 1, NULL); }
static void unlock(void) { if (g_mtx >= 0) ksceKernelUnlockMutex(g_mtx, 1); }

/* ---- self-contained printf subset (see user/log.c for the twin) --------- */

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

/* ---- file plumbing (best-effort) ---------------------------------------- */

static void file_open_locked(void)
{
	if (g_file_disabled || g_fd >= 0)
		return;
	ksceIoMkdir("ur0:data", 0777);
	ksceIoMkdir(WQ_KLOG_DIR, 0777);

	SceUID fd = ksceIoOpen(WQ_KLOG_PATH,
	                       SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
	if (fd < 0) {
		g_file_disabled = 1;
		return;
	}
	SceOff end = ksceIoLseek(fd, 0, SCE_SEEK_END);
	g_file_bytes = end > 0 ? (uint32_t)end : 0;
	g_fd = fd;
}

static void file_write_locked(const char *line, int len)
{
	if (g_file_disabled)
		return;
	if (g_fd < 0)
		file_open_locked();
	if (g_fd < 0)
		return;

	if (g_file_bytes + (uint32_t)len + 1 > WQ_KLOG_MAX_BYTES) {
		/* Simple rotate: truncate in place by reopening O_TRUNC. One file keeps
		 * the kernel side dependency-light (no rename dance from kernel io). */
		ksceIoClose(g_fd);
		g_fd = ksceIoOpen(WQ_KLOG_PATH,
		                  SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
		g_file_bytes = 0;
		if (g_fd < 0) { g_file_disabled = 1; return; }
	}

	int w1 = ksceIoWrite(g_fd, line, len);
	int w2 = ksceIoWrite(g_fd, "\n", 1);
	if (w1 < 0 || w2 < 0) {
		ksceIoClose(g_fd);
		g_fd = -1;
		g_file_disabled = 1;
		return;
	}
	g_file_bytes += (uint32_t)w1 + (uint32_t)w2;
}

/* ---- public API --------------------------------------------------------- */

void wq_klog_init(void)
{
	if (g_inited)
		return;
	g_level = WQ_KLOG_DEFAULT_LEVEL;
	g_file_disabled = 0;
	g_file_bytes = 0;
	g_fd = -1;
	if (g_mtx < 0)
		g_mtx = ksceKernelCreateMutex("WqKLogMtx", 0, 0, NULL);
	g_inited = 1;
	lock();
	file_open_locked();
	unlock();
}

void wq_klog_fini(void)
{
	if (!g_inited)
		return;
	lock();
	if (g_fd >= 0) { ksceIoClose(g_fd); g_fd = -1; }
	unlock();
	if (g_mtx >= 0) { ksceKernelDeleteMutex(g_mtx); g_mtx = -1; }
	g_inited = 0;
}

static const char *level_name(int level)
{
	switch (level) {
	case WQ_KLOG_ERR:   return "ERR";
	case WQ_KLOG_WARN:  return "WARN";
	case WQ_KLOG_INFO:  return "INFO";
	case WQ_KLOG_DEBUG: return "DEBUG";
	case WQ_KLOG_TRACE: return "TRACE";
	default:            return "?";
	}
}

void wq_klog(int level, const char *fmt, ...)
{
	if (level > g_level || !fmt)
		return;

	char line[192];
	int pos = 0;

	uint64_t t = (uint64_t)ksceKernelGetSystemTimeWide();  /* microseconds        */
	uint32_t secs = (uint32_t)(t / 1000000u);
	uint32_t us   = (uint32_t)(t % 1000000u);

	/* "[wifiturbo] [<s>.<us>] <LVL> " prefix. */
	const char *tag = "[wifiturbo] [";
	while (*tag && pos < (int)sizeof(line) - 1) line[pos++] = *tag++;
	pos = fmt_uint(line, sizeof(line), pos, secs, 10, 0, 0, 0);
	if (pos < (int)sizeof(line) - 1) line[pos++] = '.';
	pos = fmt_uint(line, sizeof(line), pos, us, 10, 0, 6, 1);
	if (pos < (int)sizeof(line) - 1) line[pos++] = ']';
	if (pos < (int)sizeof(line) - 1) line[pos++] = ' ';
	const char *lvl = level_name(level);
	while (*lvl && pos < (int)sizeof(line) - 1) line[pos++] = *lvl++;
	if (pos < (int)sizeof(line) - 1) line[pos++] = ' ';

	va_list ap;
	va_start(ap, fmt);
	pos += fmt_v(line + pos, (int)sizeof(line) - pos, fmt, ap);
	va_end(ap);

	/* Primary sink: always. The debug channel adds its own newline handling, so
	 * we feed the composed line as a single %s plus an explicit newline. */
	ksceDebugPrintf("%s\n", line);

	/* Secondary sink: the ur0 file, best-effort. */
	if (g_inited) {
		lock();
		file_write_locked(line, pos);
		unlock();
	}
}
