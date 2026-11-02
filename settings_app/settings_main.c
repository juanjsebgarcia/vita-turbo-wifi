/*
 * settings_main.c - module entry of wifiturbo_settings.suprx, the Wi-Fi Turbo
 * page in the Settings app (NPXS10015).
 *
 * The page used to be compiled into wifiturbo.suprx, which SceShell loads
 * too: its code, text and buffers then took SceShell memory it never used.
 * Now config.txt loads this module under *NPXS10015. An older config.txt that
 * still lists wifiturbo.suprx there keeps working: that module loads this one
 * (user/main.c). If both happen, the second copy finds the first and leaves.
 */
#include <psp2/kernel/modulemgr.h>
#include <taihen.h>

#include "../user/log.h"
#include "settings_ui.h"

#define START_NO_RESIDENT 1   /* SCE_KERNEL_START_NO_RESIDENT: unload me */

int module_start(SceSize argc, const void *args)
{
	(void)argc; (void)args;
	tai_module_info_t ti;
	ti.size = sizeof(ti);
	if (taiGetModuleInfo("wifiturbo_settings", &ti) >= 0 &&
	    ti.modid != sceKernelGetModuleIdByAddr((void *)module_start))
		return START_NO_RESIDENT;   /* already loaded in this process */
	wq_log_init();
	WQ_LOGI("[SET] Settings page module: installing Settings hooks");
	wq_settings_hooks_install();
	return 0; /* SCE_KERNEL_START_SUCCESS */
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc; (void)args;
	wq_settings_hooks_release();
	wq_log_fini();
	return 0; /* SCE_KERNEL_STOP_SUCCESS */
}
