/*
 * persist.c - settings saved to ur0:data/wifiturbo/settings.bin (see persist.h).
 */
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <string.h>

#include "persist.h"
#include "log.h"
#include "../include/wifiturbo.h"

#define WQ_PERSIST_DIR   "ur0:data/wifiturbo"
#define WQ_PERSIST_PATH  WQ_PERSIST_DIR "/settings.bin"
#define WQ_PERSIST_MAGIC 0x54535457u    /* 'WTST' */

typedef struct {
	uint32_t  magic;
	WqUiState s;
} wq_persist_blob;

void wq_persist_load(WqUiState *out)
{
	memset(out, 0, sizeof(*out));
	out->keepwifi = 1;   /* default ON (keeps Wi-Fi up) */

	SceUID fd = sceIoOpen(WQ_PERSIST_PATH, SCE_O_RDONLY, 0);
	if (fd < 0)
		return;                       /* no saved settings - keep defaults */
	wq_persist_blob b;
	memset(&b, 0, sizeof(b));
	int n = sceIoRead(fd, &b, sizeof(b));
	sceIoClose(fd);
	/* Accept the previous layout too (no rxring field, one int shorter). */
	if ((n == (int)sizeof(b) || n == (int)sizeof(b) - (int)sizeof(int)) &&
	    b.magic == WQ_PERSIST_MAGIC)
		*out = b.s;
	WQ_LOGI("persist: loaded keepwifi=%d hud=%d autorecon=%d roam=%d ht40=%d "
	        "profile=%d rxring=%d", out->keepwifi, out->hud,
	        out->autoreconnect, out->roam, out->ht40, out->profile, out->rxring);
}

void wq_persist_save(const WqUiState *s)
{
	sceIoMkdir("ur0:data", 0777);
	sceIoMkdir(WQ_PERSIST_DIR, 0777);
	SceUID fd = sceIoOpen(WQ_PERSIST_PATH,
	                      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if (fd < 0) {
		WQ_LOGW("persist: save open failed");
		return;
	}
	wq_persist_blob b;
	b.magic = WQ_PERSIST_MAGIC;
	b.s = *s;
	sceIoWrite(fd, &b, sizeof(b));
	sceIoClose(fd);
}
