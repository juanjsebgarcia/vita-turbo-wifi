/*
 * main.c - user module (injected into SceShell).
 *
 * module_start only brings up the logger and starts the dispatcher worker
 * (user/dispatch.c), which ~3 s later brings up, in order: the auto-revert
 * safety harness, the SceNetCtl diagnostics, the libnetctl scan reader, the
 * keepwifi worker, and the saved settings (wq_settings_boot), then serves the
 * :1339 command surface. This file also owns the HUD sampler thread
 * (wq_hud_runtime_set), which feeds the kernel renderer.
 */
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <string.h>
#include <taihen.h>   /* taiGetModuleInfo - which process are we in            */

#include "../include/wifiturbo.h"
#include "../include/wifiturbo_kernel_api.h"
#include "log.h"
#include "safety.h"
#include "diag.h"
#include "dispatch.h"
#include "powerprofile.h"   /* wq_power_current_profile - HUD profile line     */
#include "../settings_app/wifiturbo_i18n.h"   /* wq_i18n_lang - HUD language   */
#include "advanced.h"
#include "../settings_app/settings_ui.h"

static int    g_sampler_run = 0;
static SceUID g_sampler = -1;

/* Sample diagnostics once a second and push to the kernel HUD renderer, with
 * the safety harness's one-line status as the HUD notice. */
static int sampler_thread(SceSize args, void *argp)
{
	(void)args; (void)argp;
	while (g_sampler_run) {
		WqHudState s;
		if (diag_sample(&s) == WQ_OK) {
			strncpy(s.profile, wq_power_profile_name(wq_power_current_profile()),
			        sizeof(s.profile) - 1);
			s.profile[sizeof(s.profile) - 1] = 0;
			s.lang = wq_i18n_lang();   /* HUD labels follow the system language */
			wq_safety_status(s.notice, sizeof(s.notice));
			wqHudSetState(&s);
		}
		sceKernelDelayThread(1000 * 1000);
	}
	return 0;
}

/* The ONLY path that arms the HUD overlay and its sampler thread. Called from
 * the 'hud on|off' command (dispatch.c), never from module_start, so the
 * plugin boots completely inert. Idempotent: a second 'hud on' will not spawn a
 * second sampler. */
int wq_hud_runtime_set(int on)
{
	if (on) {
		if (g_sampler < 0) {
			g_sampler_run = 1;
			g_sampler = sceKernelCreateThread("WqHudSampler", sampler_thread,
			                                  0x40, 0x2000, 0, 0, NULL);
			if (g_sampler >= 0) {
				sceKernelStartThread(g_sampler, 0, NULL);
			} else {
				g_sampler_run = 0;
				WQ_LOGW("hud on: sampler thread create failed (%d)", g_sampler);
			}
		}
		wqHudSetEnabled(1);
		wqHudSetDrawArmed(1);   /* the DACR blit is proven - draw for real now  */
		WQ_LOGI("hud runtime ENABLED+DRAW (sampler %s)",
		        g_sampler >= 0 ? "running" : "unavailable");
	} else {
		wqHudSetEnabled(0);
		wqHudSetDrawArmed(0);
		g_sampler_run = 0;
		if (g_sampler >= 0) {
			sceKernelWaitThreadEnd(g_sampler, NULL, NULL);
			sceKernelDeleteThread(g_sampler);
			g_sampler = -1;
		}
		WQ_LOGI("hud runtime DISABLED (sampler stopped)");
	}
	return 0; /* WQ_OK */
}

int module_start(SceSize argc, const void *args)
{
	(void)argc; (void)args;

	/* The ONE wifiturbo.suprx is listed under both *main (SceShell) and
	 * *NPXS10015 (the Settings app) in config.txt. Branch on the process: the
	 * Settings app has no SceShell module, so there we install ONLY the Settings
	 * hooks and run none of the SceShell-side bring-up (dispatcher, keepwifi,
	 * overlay). The two instances are separate processes and talk
	 * via the kernel mailbox (uibridge). */
	tai_module_info_t ti;
	ti.size = sizeof(ti);
	int in_shell = (taiGetModuleInfo("SceShell", &ti) >= 0);
	if (!in_shell) {
		/* Settings-app (NPXS10015) instance: log to the shared file so we can
		 * see the Settings path too, then install only the Settings hooks. */
		wq_log_init();
		WQ_LOGI("[SET] settings-proc instance: installing Settings hooks");
		wq_settings_hooks_install();
		return 0; /* SCE_KERNEL_START_SUCCESS */
	}

	wq_log_init();      /* FIRST of all - capture the whole bring-up            */
	WQ_LOGI("=== wifiturbo user v%s start === log=%s "
	        "(ur0:data/wifiturbo/wifiturbo.log)", WQ_VERSION_STR,
	        wq_log_level_name(wq_log_get_level()));

	/* BOOT-SAFE: module_start touches NOTHING but the logger and then returns.
	 * Every subsystem bring-up - the safety revert spine, the SceNetCtl
	 * diagnostics, the scan verb resolve, and the TCP dispatcher socket
	 * - is deferred onto the dispatcher worker thread (user/dispatch.c), which
	 * sleeps ~3s before any SceNet/SceNetCtl call, exactly as the proven
	 * vitacompanion user plugin does.
	 *
	 * WHY: opening a socket (sceNetSocket) or registering a netctl callback
	 * (sceNetCtlInit) at ~0.4s into SceShell boot - before the shell has
	 * initialised SceNet - faults hard enough to wedge the entire cold boot
	 * (recovered only by holding L). The durable log proved it: it stopped dead
	 * right after this banner, between the old 'dispatcher started' line and the
	 * next. Deferring all network/feature bring-up past the SceNet-ready point
	 * keeps the boot stone-cold safe. The HUD additionally stays OFF until an
	 * explicit 'hud on' (see wq_hud_runtime_set), so nothing ever draws at boot. */
	wq_dispatch_start();   /* spawns the deferred worker; returns immediately    */
	WQ_LOGI("user start complete (boot-safe) - subsystems arm ~3s post-boot on "
	        "the dispatcher worker; run 'selftest', then 'hud on'");
	return 0; /* SCE_KERNEL_START_SUCCESS */
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc; (void)args;

	/* Settings-app instance: only the Settings hooks were installed. */
	tai_module_info_t ti;
	ti.size = sizeof(ti);
	if (taiGetModuleInfo("SceShell", &ti) < 0) {
		wq_settings_hooks_release();
		return 0;
	}

	WQ_LOGI("=== wifiturbo user stop ===");
	wq_dispatch_stop();

	wq_hud_runtime_set(0); /* stop the sampler + disable the overlay (if armed) */
	wq_adv_fini();
	diag_fini();
	wq_safety_fini();   /* LAST feature - fires still-pending reverts on exit   */
	wq_log_fini();      /* truly LAST - closes the log file                    */
	return 0; /* SCE_KERNEL_STOP_SUCCESS */
}
