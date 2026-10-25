# CMake build for Wi-Fi Turbo, the PS Vita Wi-Fi plugin (firmware 3.60-3.68, taiHEN/HENkaku).
#
# Two artifacts:
#   wifiturbo_kernel.skprx - kernel module: SceWlanBt patches/hooks + HUD hook,
#                            and the user-callable SceWifiTurbo syscall library.
#   wifiturbo.suprx        - user module (loaded into SceShell): HUD feed, netctl
#                            reads, reconnect/roaming, safety harness,
#                            dispatcher, Settings page.
#
# BUILD REQUIREMENTS: the VitaSDK toolchain (https://vitasdk.org) and taiHEN's
# headers/stubs (taihen):
#
#   export VITASDK=/usr/local/vitasdk
#   cmake -S . -B build \
#         -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
#   cmake --build build
#
# Order matters: the kernel module's stub library (SceWifiTurbo_stub) is generated
# from wifiturbo_kernel.yml and must exist before the user module links.

cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "VitaSDK toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK or pass -DCMAKE_TOOLCHAIN_FILE")
  endif()
endif()

project(wifiturbo_kernel C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_STANDARD 99)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -O2)

# --------------------------------------------------------------------------
# Kernel module: wifiturbo_kernel.skprx
# --------------------------------------------------------------------------
add_executable(wifiturbo_kernel
  kernel/main.c
  kernel/klog.c
  kernel/chipobserve.c
  kernel/ht40patch.c
  kernel/wlanbt.c
  kernel/rxring.c
  kernel/roamassist.c
  kernel/psprofile.c
  kernel/uibridge.c
  kernel/hud.c
  kernel/hud_i18n.c
)

target_link_libraries(wifiturbo_kernel
  taihenForKernel_stub
  SceSysmemForDriver_stub
  SceSysclibForDriver_stub      # memcpy/memset/strnlen/strncpy for kernel
  SceThreadmgrForDriver_stub
  SceModulemgrForDriver_stub
  taihenModuleUtils_stub        # module_get_offset (SceWlanBt segment base, any firmware)
  SceCpuForKernel_stub
  SceDisplayForDriver_stub
  SceDebugForDriver_stub        # ksceDebugPrintf (kernel logger, kernel/klog.c)
  SceIofilemgrForDriver_stub    # ksceIoOpen/Write/... (kernel logfile)
  SceCpuForDriver_stub          # dcache clean (rxring.c)
  SceSysconForDriver_stub       # wqForceReboot (receive-buffer watchdog)
)
# taiHEN modules enter via module_start, not crt0's main - drop startup files.
target_link_options(wifiturbo_kernel PRIVATE -nostartfiles)

# Build the .skprx and, from the yml, the SceWifiTurbo user stub the .suprx links.
vita_create_self(wifiturbo_kernel.skprx wifiturbo_kernel
  CONFIG ${CMAKE_SOURCE_DIR}/wifiturbo_kernel.yml
  UNSAFE)
vita_create_stubs(wifiturbo-stubs wifiturbo_kernel ${CMAKE_SOURCE_DIR}/wifiturbo_kernel.yml
  KERNEL)

# --------------------------------------------------------------------------
# User module: wifiturbo.suprx (runs inside SceShell)
# --------------------------------------------------------------------------
#
# LINK RECIPE (why weak stubs): a taiHEN user module injected under *main is
# load-resolved inside the SceShell process. A STRONG import of a user library
# that is NOT already loaded in that process makes taiHEN fail the whole module
# load SILENTLY - module_start never runs and no log file ever appears. The
# proven fix (as used by the vitacompanion kernel+user pair, which loads on
# 3.60) is to import every standard system library as its *_stub_weak
# variant: a weak import that cannot be resolved is skipped instead of aborting
# the load, and when the library IS present (all of these are, inside SceShell)
# it resolves normally. Only taihen and our own kernel syscall library are kept
# as STRONG imports - they must be present, and if either is truly missing we
# WANT the load to fail loudly rather than run half-wired.

add_executable(wifiturbo
  user/main.c
  user/log.c
  user/safety.c
  user/diag.c
  user/powerprofile.c
  user/advanced.c
  user/dispatch.c
  user/keepwifi.c
  user/keepalive.c
  user/netjoin.c
  user/rxcycle.c
  user/rxpreset.c
  user/persist.c
  user/settings_entry.c
  settings_app/wifiturbo_settings.c   # Settings-app hooks (run only in NPXS10015)
  settings_app/wifiturbo_i18n.c       # localised UI strings for the Settings page
)

# libwifiturbo_kernel_stub.a is produced by vita_create_stubs above (copied
# into the build dir); make the user module wait for it.
add_dependencies(wifiturbo wifiturbo-stubs)

target_link_libraries(wifiturbo
  # STRONG - must be present, fail loudly if not:
  taihen_stub
  # Our own kernel syscall library (SceWifiTurbo). Linked by FULL PATH, the
  # way vitacompanion links its kernel stub, so it is unambiguously the
  # generated archive and stays a strong import.
  ${CMAKE_BINARY_DIR}/libwifiturbo_kernel_stub.a
  # WEAK - standard SceShell-resident libs (strong imports here would make
  # taiHEN silently drop the whole user module; see the note above):
  SceNetCtl_stub_weak           # sceNetCtl* (diag.c)
  SceNet_stub_weak              # sceNet* sockets (dispatch.c TCP surface)
  SceLibKernel_stub_weak        # sceKernel* (mutex/thread/time), sceIo* helpers
  SceKernelThreadMgr_stub_weak  # sceKernelDelayThread/Delete* (SceThreadmgr lib)
  SceIofilemgr_stub_weak        # sceIoClose/sceIoWrite (the log file)
  SceProcessmgr_stub_weak       # sceKernelPowerLock/Unlock/Tick (keepwifi.c)
  SceRegistryMgr_stub_weak      # sceRegMgrGetKeyInt (keepwifi.c wifi_flag)
  SceWlanBt_stub_weak           # sceWlanGet/SetConfiguration (keepwifi reconnect)
  SceBt_stub_weak               # sceBtGet/SetConfiguration (rxcycle: chip power-down)
  SceKernelModulemgr_stub_weak  # sceKernelGetModuleInfo (netjoin seg base)
  SceAppUtil_stub_weak          # sceAppUtilSystemParamGetInt (i18n lang fallback)
)

# -Tdata: vita-elf-create appends ~1.5 KB of module info to the end of the text
# segment. With data on the very next page, that fails ("segment 1 overlaps")
# whenever the code happens to end in the last ~1.5 KB of a page. A fixed data
# address leaves room regardless; segments are relocated independently at load.
target_link_options(wifiturbo PRIVATE -nostartfiles -Wl,-Tdata=0x81100000)

# Size-optimise the Settings-page UI code (not timing-sensitive).
set_source_files_properties(settings_app/wifiturbo_settings.c settings_app/wifiturbo_i18n.c
  PROPERTIES COMPILE_OPTIONS -Os)

vita_create_self(wifiturbo.suprx wifiturbo
  CONFIG ${CMAKE_SOURCE_DIR}/wifiturbo.yml
  UNSAFE)

# The Settings-app hooks (settings_app/wifiturbo_settings.c) are compiled INTO
# wifiturbo.suprx above - the one suprx is listed under both *main and *NPXS10015
# in config.txt, and module_start branches per process. No separate module.
