/*
 * rxring.c - EXPERIMENTAL: the Wi-Fi chip's receive buffer (Settings: "Wi-Fi
 * receive buffer").
 *
 * The Wi-Fi chip's single receive ring holds only 83 slots, about 11-12
 * full-size frames, and it overflows during bursts while the Vita drains it.
 * The ring is sized by the chip firmware that SceWlanBt keeps in memory and
 * sends to the chip at every power-up. Growing the ring to 127 or 149 slots
 * (and shrinking a transmit buffer pool by the same amount) is a set of
 * same-length edits to that in-memory copy, plus the checksum of each block
 * of the download it touches.
 *
 * Safety rules:
 *   - refuses unless the image is loaded, one of the two known sizes, its
 *     pointer is the base of its memblock, every original byte at every edit
 *     site matches at a single offset (0 for p223, 0xC8 for p226)
 *     consistent with the image size, and every touched block's checksums
 *     verify before any edit;
 *   - applies every edit for the chosen size, never a subset, and recomputes
 *     each touched block's checksum;
 *   - edits under SceWlanBt's power mutex (seg1+0x8) so a power-up cannot
 *     send a half-edited image, writing through the read-only mapping with
 *     the DACR opened (ro_write);
 *   - writes the edited bytes back to memory (cache clean) before the next
 *     power-up;
 *   - is fully reversible to the exact original bytes (restore, and in
 *     module_stop). Re-applying the same size is a no-op; switching sizes is
 *     restore then apply.
 *
 * The change takes effect only at the next chip power-cycle (Wi-Fi and
 * Bluetooth both off, then on: user/rxcycle.c); the running chip is untouched.
 * Other firmware versions are refused by the pointer/size/byte checks.
 */
#include <psp2kern/kernel/modulemgr.h>   /* ksceKernelSearchModuleByName */
#include <psp2kern/kernel/sysmem.h>      /* ksceKernelGetMemBlockBase     */
#include <psp2kern/kernel/sysclib.h>     /* memcmp                       */
#include <psp2kern/kernel/cpu.h>         /* ksceKernelDcacheCleanRange   */
#include <psp2kern/kernel/threadmgr.h>   /* SceKernelFastMutex + lock    */
#include <taihen.h>
int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);

#include "rxring.h"
#include "wlanbt.h"
#include "klog.h"
#include "../include/wifiturbo.h"

/* SceWlanBt seg1 (data) fields that hold the in-memory chip image. */
#define RXR_SEG1_POWER   0x004u   /* bit0 chip powered, bit1 WLAN on, bit2 BT on */
#define RXR_SEG1_MUTEX   0x008u   /* power SceKernelFastMutex                     */
#define RXR_SEG1_UID     0x224u   /* memblock UID of the image                    */
#define RXR_SEG1_PTR     0x228u   /* image pointer (the download stream)          */
#define RXR_SEG1_SIZE    0x22cu   /* image size                                   */
#define RXR_SEG1_LOADED  0x230u   /* loaded flag (image read once at boot)        */
#define RXR_SEG1_NEED    0x234u   /* bytes from the base we read                  */

#define RXR_SIZE_P223    403696u  /* 3.60-3.65 */
#define RXR_SIZE_P226    403916u  /* 3.67-3.74 */
#define RXR_DELTA_P226   0xC8u    /* p226 code sites sit +0xC8 later              */

/* Variants, in table-column order. */
enum { V_STOCK = 0, V_127 = 1, V_171 = 2, V_149 = 3, V_N = 4 };
static const unsigned rxr_slots[V_N] = { 83, 127, 171, 149 };   /* ring 3 slot count */
static const unsigned rxr_argsz[V_N] = { 0, 127, 171, 149 };    /* 'apply <n>' value  */

/* Every size-dependent edit. 149 follows the same rules as 127 and 171:
 * count 0x95, last pointer 148*8 = 0x4A0, descriptors at the ring end
 * 0xC001F800, pool 0xC001FCB0 with 19 x 2040 B in a 39024 B budget (0x1000 and
 * 0x2000 cases 9 and 5), flag clear 152 B. `addr` is the p223 chip
 * address; code sites get the per-build delta added, the .data byte does not.
 * Bytes are in memory order, as the tool fwcheck.py checks them. Same-length
 * edits only. V_STOCK is the original, used for the arm-time byte check. */
#define RXR_N_EDITS 14
static const struct {
	uint32_t addr;        /* p223 chip address                                 */
	uint8_t  len;
	uint8_t  is_data;     /* 1 = compressed .data literal (no code delta)       */
	uint8_t  b[V_N][4];   /* [V_STOCK | V_127 | V_171 | V_149]                  */
	const char *what;
} k_edits[RXR_N_EDITS] = {
	{ 0x0000dbe4, 2, 0, {{0x53,0x22},{0x7f,0x22},{0xab,0x22},{0x95,0x22}}, "ring3 slot count"      },
	{ 0x0000dc1a, 2, 0, {{0x52,0x2c},{0x7e,0x2c},{0xaa,0x2c},{0x94,0x2c}}, "init loop (count-1)"   },
	{ 0x0000dc5e, 2, 0, {{0x53,0x20},{0x7f,0x20},{0xab,0x20},{0x95,0x20}}, "hw count register"     },
	{ 0x0000dca0, 4, 0, {{0x30,0x41,0x01,0xc0},{0x00,0xe2,0x01,0xc0},{0x00,0x0e,0x02,0xc0},{0x00,0xf8,0x01,0xc0}}, "descriptor array base" },
	{ 0x0000d6c6, 2, 0, {{0x29,0x24},{0x3f,0x24},{0x55,0x24},{0x4a,0x24}}, "last-slot pointer"     },
	{ 0x0000da34, 2, 0, {{0x53,0x2c},{0x7f,0x2c},{0xab,0x2c},{0x95,0x2c}}, "scan bound"            },
	{ 0x0000d5c8, 2, 0, {{0x80,0x21},{0x80,0x21},{0xb0,0x21},{0x98,0x21}}, "flag-array clear"      },
	{ 0x0001becc, 4, 0, {{0x00,0xb6,0x01,0xc0},{0x00,0xe6,0x01,0xc0},{0x60,0x13,0x02,0xc0},{0xb0,0xfc,0x01,0xc0}}, "pool base" },
	{ 0x0001bb6c, 2, 0, {{0x1c,0x24},{0x15,0x24},{0x10,0x24},{0x13,0x24}}, "0xD9 0x800 blocks"     },
	{ 0x0001bb50, 2, 0, {{0x0e,0x24},{0x0b,0x24},{0x08,0x24},{0x09,0x24}}, "0xD9 0x1000 blocks"    },
	{ 0x0001bb5e, 2, 0, {{0x07,0x24},{0x05,0x24},{0x04,0x24},{0x05,0x24}}, "0xD9 0x2000 blocks"    },
	{ 0x0001bbfe, 2, 0, {{0x1c,0x2c},{0x15,0x2c},{0x10,0x2c},{0x13,0x2c}}, "alloc limit cmp"       },
	{ 0x0001bc04, 2, 0, {{0x1c,0x22},{0x15,0x22},{0x10,0x22},{0x13,0x22}}, "alloc limit movs"      },
	{ 0x040007c1, 1, 1, {{0x1c},{0x15},{0x10},{0x13}},               ".data boot pool count" },
};
/* The .data count byte and its two preceding literal bytes must be f4 07 1c
 * (pool config size u16 0x07F4, count u8 0x1C). */
static const uint8_t k_data_guard[2] = { 0xf4, 0x07 };

#define U32(a)  (*(volatile uint32_t *)(a))

static int g_variant = V_STOCK;   /* size currently written to the cached image  */
static int g_delta_set;           /* code delta determined this boot              */
static unsigned g_delta;          /* 0 (p223) or 0xC8 (p226)                       */
static unsigned g_chunks;         /* chunks rewritten by the last apply/restore    */
static unsigned g_why, g_why_detail;

static void latch(unsigned why, unsigned detail)
{
	if (g_why == WQ_RXRING_WHY_NONE) {
		g_why = why;
		g_why_detail = detail;
	}
}

/* CRC32 as the download stream uses it: poly 0x04C11DB7, init 0, MSB-first, no
 * reflection, no xorout. */
static uint32_t crc32_msb(const uint8_t *d, unsigned n)
{
	uint32_t crc = 0;
	for (unsigned i = 0; i < n; i++) {
		crc ^= (uint32_t)d[i] << 24;
		for (int b = 0; b < 8; b++)
			crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04C11DB7u : (crc << 1);
	}
	return crc;
}

static uint32_t rd32le(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Walk the download stream (a run of 16-byte headers {cmd,addr,len,hcrc}, each
 * followed by `len` payload bytes whose last 4 are the body CRC). Find the
 * chunk whose body covers chip address `chip`. Returns 1 and fills *cpos (the
 * header offset in the image) and *len (the header's length field), else 0. */
static int find_chunk(const uint8_t *img, unsigned size, uint32_t chip,
                      unsigned *cpos, unsigned *len)
{
	unsigned pos = 0;
	while (pos + 16 <= size) {
		uint32_t cmd = rd32le(img + pos);
		uint32_t addr = rd32le(img + pos + 4);
		uint32_t clen = rd32le(img + pos + 8);
		if (cmd == 4)
			break;                               /* final jump: no payload      */
		if (clen < 4 || (uint64_t)pos + 16 + clen > size)
			break;                               /* malformed                   */
		if (chip >= addr && chip < addr + (clen - 4)) {
			*cpos = pos;
			*len = clen;
			return 1;
		}
		pos += 16 + clen;
	}
	return 0;
}

/* Verify both CRCs of the chunk at cpos. The header CRC over
 * the first 12 bytes is accepted in either byte order (we never rewrite it);
 * the payload CRC over the body is big-endian (we do rewrite it). */
static int chunk_crc_ok(const uint8_t *img, unsigned cpos, unsigned len)
{
	uint32_t hc = crc32_msb(img + cpos, 12);
	const uint8_t *h = img + cpos + 12;
	uint32_t h_be = ((uint32_t)h[0] << 24) | ((uint32_t)h[1] << 16) | ((uint32_t)h[2] << 8) | h[3];
	uint32_t h_le = ((uint32_t)h[3] << 24) | ((uint32_t)h[2] << 16) | ((uint32_t)h[1] << 8) | h[0];
	if (hc != h_be && hc != h_le)
		return 0;
	unsigned bodylen = len - 4;
	uint32_t pc = crc32_msb(img + cpos + 16, bodylen);
	const uint8_t *q = img + cpos + 16 + bodylen;
	uint32_t p_be = ((uint32_t)q[0] << 24) | ((uint32_t)q[1] << 16) | ((uint32_t)q[2] << 8) | q[3];
	return pc == p_be;
}

/* Write `n` bytes into the cached image, which SceWlanBt maps read-only.
 * sysmem refuses to remap it back to RW (seen on device), so open the Domain
 * Access Control Register to all-manager for the copy, as hud.c does and as
 * the SDK's inline ksceKernelDomainTextMemcpy does. IRQs are masked so the
 * thread cannot be preempted (or migrated to a core with the stock DACR)
 * mid-copy; each call copies at most 4 bytes. Pure CPU ops: no imports. The
 * caller cleans the image afterwards. */
static void ro_write(uint8_t *dst, const uint8_t *src, unsigned n)
{
	uint32_t cpsr, prev;
	__asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
	__asm__ volatile("cpsid i" ::: "memory");
	__asm__ volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(prev));
	__asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(0xFFFFFFFFu));
	__asm__ volatile("isb" ::: "memory");
	for (unsigned j = 0; j < n; j++)
		((volatile uint8_t *)dst)[j] = src[j];
	__asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(prev));
	__asm__ volatile("isb" ::: "memory");
	__asm__ volatile("msr cpsr_c, %0" :: "r"(cpsr) : "memory");
}

/* Recompute the body CRC and store it big-endian at the end of the payload. */
static void chunk_crc_fix(uint8_t *img, unsigned cpos, unsigned len)
{
	unsigned bodylen = len - 4;
	uint32_t pc = crc32_msb(img + cpos + 16, bodylen);
	uint8_t be[4] = { (uint8_t)(pc >> 24), (uint8_t)(pc >> 16), (uint8_t)(pc >> 8), (uint8_t)pc };
	ro_write(img + cpos + 16 + bodylen, be, 4);
}

/* Offset in the image of edit `i` at code delta `delta`, or 0 if its chunk is
 * not found. Also returns the chunk's header offset and length in cpos, len. */
static unsigned edit_offset(const uint8_t *img, unsigned size, int i, unsigned delta,
                            unsigned *cpos, unsigned *len)
{
	uint32_t chip = k_edits[i].addr + (k_edits[i].is_data ? 0 : delta);
	if (!find_chunk(img, size, chip, cpos, len))
		return 0;
	uint32_t chunk_addr = rd32le(img + *cpos + 4);
	return *cpos + 16 + (chip - chunk_addr);
}

/* Do every site at delta `delta` currently hold variant `var`'s bytes (and the
 * .data guard)? On the first mismatch (or a chunk not found) returns 0 and sets
 * *bad to that edit's p223 address; returns 1 if all match. */
static int check_variant(const uint8_t *img, unsigned size, int var, unsigned delta, uint32_t *bad)
{
	for (int i = 0; i < RXR_N_EDITS; i++) {
		unsigned cpos, len;
		unsigned off = edit_offset(img, size, i, delta, &cpos, &len);
		if (!off) { *bad = k_edits[i].addr; return 0; }
		if (k_edits[i].is_data &&
		    (img[off - 2] != k_data_guard[0] || img[off - 1] != k_data_guard[1])) {
			*bad = k_edits[i].addr; return 0;
		}
		for (int j = 0; j < k_edits[i].len; j++)
			if (img[off + j] != k_edits[i].b[var][j]) { *bad = k_edits[i].addr; return 0; }
	}
	return 1;
}

/* Log the bytes found at the patch site with p223 address `addr` (from
 * check_variant's *bad) at code delta `delta`, so a refusal names what it saw.
 * Refusal path only, never hot. */
static void log_site(const uint8_t *img, unsigned size, uint32_t addr, unsigned delta)
{
	for (int i = 0; i < RXR_N_EDITS; i++) {
		if (k_edits[i].addr != addr)
			continue;
		unsigned cpos, len;
		unsigned off = edit_offset(img, size, i, delta, &cpos, &len);
		if (!off) {
			WQ_KLOGW("rxring:   site 0x%x (%s) @delta 0x%x: no chunk covers it", addr, k_edits[i].what, delta);
			return;
		}
		/* .data: include the two guard bytes before the count byte. */
		unsigned from = k_edits[i].is_data ? off - 2 : off;
		unsigned n = k_edits[i].is_data ? 3 : k_edits[i].len;
		unsigned v = 0;
		for (unsigned j = 0; j < n; j++)
			v = (v << 8) | img[from + j];
		/* klog has no '*' width: print 4 bytes; the last n are the site. */
		WQ_KLOGW("rxring:   site 0x%x (%s) @delta 0x%x: saw %08x (last %u bytes, memory order)",
		         addr, k_edits[i].what, delta, v, n);
		return;
	}
}

struct img_info {
	SceUID    modid;
	uintptr_t seg1;
	SceUID    uid;
	uintptr_t ptr;
	unsigned  size;
	unsigned  loaded;
	unsigned  power;
};

/* Resolve SceWlanBt's data segment and read the cached-image fields. Returns a
 * WQ_RXRING_WHY_* reason (0 = ok). Reads only; changes nothing. */
static unsigned resolve_img(struct img_info *o)
{
	SceUID modid = ksceKernelSearchModuleByName("SceWlanBt");
	uintptr_t seg1 = 0;
	if (modid < 0 || module_get_offset(KERNEL_PID, modid, 1, 0, &seg1) < 0 || !seg1 ||
	    !wlanbt_seg_contains(1, seg1, RXR_SEG1_NEED))
		return WQ_RXRING_WHY_RESOLVE;
	o->modid  = modid;
	o->seg1   = seg1;
	o->power  = U32(seg1 + RXR_SEG1_POWER);
	o->uid    = (SceUID)U32(seg1 + RXR_SEG1_UID);
	o->ptr    = U32(seg1 + RXR_SEG1_PTR);
	o->size   = U32(seg1 + RXR_SEG1_SIZE);
	o->loaded = U32(seg1 + RXR_SEG1_LOADED);
	return 0;
}

/* The core transition: verify the image holds variant `from`, then write
 * variant `to` and fix the touched chunks' CRCs, all under the power mutex.
 * On any identity failure it latches a reason and changes nothing. */
static int do_edit(int from, int to)
{
	struct img_info im;
	unsigned why = resolve_img(&im);
	if (why) { latch(why, 0); return WQ_ERR_RESOLVE; }

	if (im.loaded != 1)                 { latch(WQ_RXRING_WHY_NOTLOADED, im.loaded); return WQ_ERR_NOT_READY; }
	if (im.size != RXR_SIZE_P223 && im.size != RXR_SIZE_P226) {
		latch(WQ_RXRING_WHY_SIZE, im.size); return WQ_ERR_RESOLVE;
	}
	void *base = 0;
	if (ksceKernelGetMemBlockBase(im.uid, &base) < 0 || (uintptr_t)base != im.ptr) {
		latch(WQ_RXRING_WHY_PTR, im.ptr);
		return WQ_ERR_RESOLVE;
	}
	const uint8_t *img = (const uint8_t *)im.ptr;
	unsigned size = im.size;
	unsigned size_delta = (size == RXR_SIZE_P226) ? RXR_DELTA_P226 : 0;

	/* Determine the code delta. From stock: the delta is the one at which every
	 * original byte matches, and it must agree with the image size. Once set
	 * (we only reach a patched state via a stock apply this boot), reuse it. */
	unsigned delta;
	uint32_t bad = 0;
	if (from == V_STOCK) {
		int ok0 = check_variant(img, size, V_STOCK, 0, &bad);
		uint32_t bad2 = 0;
		int okC = check_variant(img, size, V_STOCK, RXR_DELTA_P226, &bad2);
		if (ok0 == okC) {                /* neither or both: cannot disambiguate */
			/* Detail = the failing site at the delta the image size implies. */
			latch(WQ_RXRING_WHY_DELTA, size_delta ? bad2 : bad);
			WQ_KLOGW("rxring: cannot determine code delta (match@0=%d @0xC8=%d, size %u) - refusing",
			         ok0, okC, size);
			if (!ok0) log_site(img, size, bad, 0);
			if (!okC) log_site(img, size, bad2, RXR_DELTA_P226);
			return WQ_ERR_RESOLVE;
		}
		delta = ok0 ? 0 : RXR_DELTA_P226;
		if (delta != size_delta) {       /* bytes and size must agree           */
			latch(WQ_RXRING_WHY_DELTA, size);
			WQ_KLOGW("rxring: code delta 0x%x disagrees with image size %u - refusing", delta, size);
			return WQ_ERR_RESOLVE;
		}
	} else {
		delta = g_delta_set ? g_delta : size_delta;
		if (!check_variant(img, size, from, delta, &bad)) {
			latch(WQ_RXRING_WHY_BYTES, bad);
			WQ_KLOGW("rxring: site 0x%x not at the expected (variant %d) bytes - refusing", bad, from);
			log_site(img, size, bad, delta);
			return WQ_ERR_RESOLVE;
		}
	}
	if (from == V_STOCK && !check_variant(img, size, V_STOCK, delta, &bad)) {
		latch(WQ_RXRING_WHY_BYTES, bad);
		WQ_KLOGW("rxring: original byte at site 0x%x differs - refusing", bad);
		log_site(img, size, bad, delta);
		return WQ_ERR_RESOLVE;
	}

	/* Collect the distinct chunks the edits touch, and verify every one's CRC
	 * before changing anything. */
	unsigned chunk_pos[RXR_N_EDITS];
	int n_chunks = 0;
	for (int i = 0; i < RXR_N_EDITS; i++) {
		unsigned cpos, len;
		if (!edit_offset(img, size, i, delta, &cpos, &len)) {
			latch(WQ_RXRING_WHY_BYTES, k_edits[i].addr);
			return WQ_ERR_RESOLVE;
		}
		int seen = 0;
		for (int c = 0; c < n_chunks; c++)
			if (chunk_pos[c] == cpos) { seen = 1; break; }
		if (!seen) {
			if (!chunk_crc_ok(img, cpos, len)) {
				latch(WQ_RXRING_WHY_CRC, cpos);
				WQ_KLOGW("rxring: download chunk at image+0x%x has a bad CRC - refusing", cpos);
				return WQ_ERR_RESOLVE;
			}
			chunk_pos[n_chunks] = cpos;
			n_chunks++;
		}
	}

	/* Everything checks out: take the power mutex so no power-up pushes a
	 * half-edited image, and edit through ro_write (the image stays mapped
	 * read-only; sysmem refuses to remap it back to RW, seen on device). */
	SceKernelFastMutex *mtx = (SceKernelFastMutex *)(im.seg1 + RXR_SEG1_MUTEX);
	if (ksceKernelLockFastMutex(mtx) < 0) {
		WQ_KLOGW("rxring: could not lock SceWlanBt power mutex - not editing");
		return WQ_ERR_BUSY;              /* operational: not latched, retriable  */
	}
	uint8_t *w = (uint8_t *)im.ptr;
	unsigned touched[RXR_N_EDITS], touched_len[RXR_N_EDITS];
	int n_touched = 0;
	for (int i = 0; i < RXR_N_EDITS; i++) {
		if (memcmp(k_edits[i].b[from], k_edits[i].b[to], k_edits[i].len) == 0)
			continue;                    /* byte unchanged between variants       */
		unsigned cpos, len;
		unsigned off = edit_offset(w, size, i, delta, &cpos, &len);
		ro_write(w + off, k_edits[i].b[to], k_edits[i].len);
		int seen = 0;
		for (int c = 0; c < n_touched; c++)
			if (touched[c] == cpos) { seen = 1; break; }
		if (!seen) {
			touched[n_touched] = cpos;
			touched_len[n_touched] = len;
			n_touched++;
		}
	}
	for (int c = 0; c < n_touched; c++)
		chunk_crc_fix(w, touched[c], touched_len[c]);
	/* Write the edited bytes back to memory before the next push; never
	 * invalidate (4 rule 3). */
	ksceKernelDcacheCleanRange(w, size);
	ksceKernelUnlockFastMutex(mtx);

	g_delta = delta;
	g_delta_set = 1;
	g_variant = to;
	g_chunks = (unsigned)n_touched;
	WQ_KLOGI("rxring: %s -> %u slots (%d chunks, delta 0x%x, image 0x%08x/%u)",
	         (to == V_STOCK) ? "restore" : "apply", rxr_slots[to], n_touched, delta,
	         (unsigned)im.ptr, size);
	return WQ_OK;
}

/* Public: apply the requested size. Re-applying the same size is a no-op;
 * switching sizes restores stock first, then applies. */
static int rxring_apply(unsigned argsz)
{
	int to;
	if (argsz == 127)      to = V_127;
	else if (argsz == 171) to = V_171;
	else if (argsz == 149) to = V_149;
	else                   return WQ_ERR_ARG;
	if (g_why != WQ_RXRING_WHY_NONE)
		return WQ_ERR_BUSY;              /* latched: reboot to retry             */
	if (g_variant == to)
		return WQ_OK;
	if (g_variant != V_STOCK) {
		int r = do_edit(g_variant, V_STOCK);
		if (r != WQ_OK)
			return r;
	}
	return do_edit(V_STOCK, to);
}

static int rxring_restore(void)
{
	if (g_why != WQ_RXRING_WHY_NONE)
		return WQ_ERR_BUSY;
	if (g_variant == V_STOCK)
		return WQ_OK;
	return do_edit(g_variant, V_STOCK);
}

int rxring_op(int op, WqRxRingStats *out)
{
	unsigned arg = (unsigned)op >> 8;
	int r = WQ_OK;
	switch (op & 0xFF) {
	case WQ_RXRING_OP_STATUS:  break;
	case WQ_RXRING_OP_APPLY:   r = rxring_apply(arg); break;
	case WQ_RXRING_OP_RESTORE: r = rxring_restore();  break;
	default:                   r = WQ_ERR_ARG;        break;
	}

	if (out) {
		struct img_info im;
		unsigned why = resolve_img(&im);
		memset(out, 0, sizeof(*out));
		out->present = (why == 0);
		if (why == 0) {
			out->loaded   = im.loaded;
			out->img_addr = (unsigned)im.ptr;
			out->img_size = im.size;
			out->power    = im.power;
		}
		out->build      = g_delta_set ? (g_delta ? 226 : 223)
		                : (why == 0 ? (im.size == RXR_SIZE_P226 ? 226 : 223) : 0);
		out->slots      = rxr_slots[g_variant];
		out->applied    = rxr_argsz[g_variant];
		out->delta      = g_delta_set ? g_delta : 0xFFFFFFFFu;
		out->chunks     = g_chunks;
		out->why        = g_why;
		out->why_detail = g_why_detail;
		out->last_rc    = r;
	}
	return r;
}

void rxring_fini(void)
{
	if (g_variant != V_STOCK && g_why == WQ_RXRING_WHY_NONE) {
		WQ_KLOGI("rxring: restoring stock image on module stop");
		do_edit(g_variant, V_STOCK);
	}
}
