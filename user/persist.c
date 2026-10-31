/*
 * persist.c - settings saved to ur0:data/wifiturbo/settings.bin (see persist.h).
 */
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stddef.h>   /* offsetof */
#include <string.h>

#include "persist.h"
#include "log.h"
#include "../include/wifiturbo.h"

#define WQ_PERSIST_DIR   "ur0:data/wifiturbo"
#define WQ_PERSIST_PATH  WQ_PERSIST_DIR "/settings.bin"
#define WQ_PERSIST_TMP   WQ_PERSIST_DIR "/settings.tmp"
#define WQ_PERSIST_MAGIC 0x54535457u    /* 'WTST' */

typedef struct {
	uint32_t  magic;
	WqUiState s;
} wq_persist_blob;

/* 1 and *out if `path` holds a valid blob. exact: only a full-size one (the
 * tmp, always written whole by this build). */
static int load_from(const char *path, WqUiState *out, int exact)
{
	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		return 0;
	wq_persist_blob b;
	memset(&b, 0, sizeof(b));
	int n = sceIoRead(fd, &b, sizeof(b));
	sceIoClose(fd);
	/* Accept older layouts too: each added trailing field (rxring, hudpos,
	 * hudgame, then the MAC address fields) makes settings.bin longer, so a
	 * shorter file is just from an earlier build and its missing tail stays 0
	 * (from the memset above). The oldest layout ends at 'devmode'. */
	if (exact && n != (int)sizeof(b))
		return 0;
	if (n >= (int)(sizeof(uint32_t) + offsetof(WqUiState, rxring)) && n <= (int)sizeof(b) &&
	    (n & 3) == 0 && b.magic == WQ_PERSIST_MAGIC) {
		*out = b.s;
		return 1;
	}
	return 0;
}

void wq_persist_load(WqUiState *out)
{
	memset(out, 0, sizeof(*out));
	out->keepwifi = 1;   /* default ON (keeps Wi-Fi up) */
	/* settings.tmp is only left over by a save that did not finish putting it
	 * in place; a full-size one is newer than settings.bin, so it wins and is
	 * put in place now. */
	if (load_from(WQ_PERSIST_TMP, out, 1)) {
		sceIoRemove(WQ_PERSIST_PATH);
		if (sceIoRename(WQ_PERSIST_TMP, WQ_PERSIST_PATH) < 0)
			WQ_LOGW("persist: settings.tmp could not be put in place");
	} else if (!load_from(WQ_PERSIST_PATH, out, 0)) {
		memset(out, 0, sizeof(*out));
		out->keepwifi = 1;            /* no saved settings - keep defaults */
	}
	WQ_LOGI("persist: loaded keepwifi=%d hud=%d hudpos=%d hudgame=%d autorecon=%d "
	        "roam=%d ht40=%d profile=%d rxring=%d macmode=%d", out->keepwifi, out->hud,
	        out->hudpos, out->hudgame,
	        out->autoreconnect, out->roam, out->ht40, out->profile, out->rxring,
	        out->macmode);
}

void wq_persist_save(const WqUiState *s)
{
	sceIoMkdir("ur0:data", 0777);
	sceIoMkdir(WQ_PERSIST_DIR, 0777);
	/* Written whole to settings.tmp first, then put in place, so a crash or
	 * power loss mid-save leaves the old file or the new one, never half. */
	SceUID fd = sceIoOpen(WQ_PERSIST_TMP,
	                      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if (fd < 0) {
		WQ_LOGW("persist: save open failed");
		return;
	}
	wq_persist_blob b;
	b.magic = WQ_PERSIST_MAGIC;
	b.s = *s;
	int n = sceIoWrite(fd, &b, sizeof(b));
	sceIoClose(fd);
	if (n != (int)sizeof(b)) {
		WQ_LOGW("persist: save write failed (0x%08x) - old settings kept", (unsigned)n);
		sceIoRemove(WQ_PERSIST_TMP);
		return;
	}
	sceIoRemove(WQ_PERSIST_PATH);         /* rename does not replace an existing file */
	int r = sceIoRename(WQ_PERSIST_TMP, WQ_PERSIST_PATH);
	if (r < 0)
		WQ_LOGW("persist: rename failed (0x%08x) - settings.tmp kept", (unsigned)r);
}
