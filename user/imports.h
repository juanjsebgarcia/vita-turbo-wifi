/*
 * imports.h - is a weak import connected yet?
 *
 * Every system library is imported weak (CMakeLists.txt), so the module loads
 * even when a library is not loaded in the process yet. The loader then leaves
 * the stub as
 *     sub r12, pc, #8 ; mov pc, #0            (e24fc008 e3a0f000)
 * and patches it to movw/movt/bx r12 once the library loads. Calling it before
 * that jumps to address 0 and takes the process down (issue #17: SceShell, when
 * many plugins delay its own start-up past our bring-up). Check before the
 * first call into any library that may load after us.
 */
#ifndef WIFITURBO_IMPORTS_H
#define WIFITURBO_IMPORTS_H

#include <stdint.h>

static inline int wq_import_ready(const void *fn)
{
	const volatile uint32_t *w = (const volatile uint32_t *)((uintptr_t)fn & ~(uintptr_t)1);
	return !(w[0] == 0xE24FC008u && w[1] == 0xE3A0F000u);
}

#endif /* WIFITURBO_IMPORTS_H */
