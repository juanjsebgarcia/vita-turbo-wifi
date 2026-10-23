# vita-turbo-wifi - build and deploy helpers.
#
#   make                      build both modules (uses your VitaSDK if $VITASDK
#                             is set, otherwise the pinned VitaSDK Docker image)
#   make install VITA_IP=...  upload the modules to ur0:tai/ over FTP
#   make deploy  VITA_IP=...  install, then reboot the Vita so they load
#   make log     VITA_IP=...  fetch the plugin logs into ./logs/
#   make config               print the lines to add to ur0:tai/config.txt
#   make clean
#
# install/deploy/log talk to vitacompanion (FTP on :1337, commands on :1338)
# running on the Vita. Output: build/wifiturbo_kernel.skprx, build/wifiturbo.suprx

BUILD   ?= build
VITA_IP ?=
# Pinned so every build uses the same toolchain (VitaSDK 2026.08, GCC 15.2).
VITASDK_IMAGE ?= vitasdk/vitasdk@sha256:8227da1b596d981f4513cd9b36bda1e43857f6dae5bcf4312c1cd40167f7d9bb

MODULES := $(BUILD)/wifiturbo_kernel.skprx $(BUILD)/wifiturbo.suprx
FTP      = ftp://$(VITA_IP):1337

.PHONY: all build install deploy log config clean check-ip

all: build

build:
ifdef VITASDK
	cmake -S . -B $(BUILD) -DCMAKE_TOOLCHAIN_FILE=$(VITASDK)/share/vita.toolchain.cmake
	cmake --build $(BUILD) -j
else
	docker run --rm -v "$(CURDIR)":/src -w /src $(VITASDK_IMAGE) sh -c '\
	  cmake -S . -B $(BUILD) -DCMAKE_TOOLCHAIN_FILE=$$VITASDK/share/vita.toolchain.cmake && \
	  cmake --build $(BUILD) -j$$(nproc) && \
	  chown -R $(shell id -u):$(shell id -g) $(BUILD)'
endif

check-ip:
	@test -n "$(VITA_IP)" || { echo "Set VITA_IP, e.g. make $(MAKECMDGOALS) VITA_IP=192.168.1.50"; exit 1; }

install: build check-ip
	@for f in $(MODULES); do \
	  echo "upload $$f"; curl -sS -T $$f "$(FTP)/ur0:/tai/$$(basename $$f)" || exit 1; \
	done
	@echo "Installed. If this is the first install, add the plugin to config.txt (make config)."

deploy: install
	@printf 'reboot\n' | nc -w 3 $(VITA_IP) 1338 >/dev/null || true
	@echo "Rebooting the Vita - the plugin loads at boot."

log: check-ip
	@mkdir -p logs
	curl -sS "$(FTP)/ur0:/data/wifiturbo/wifiturbo.log" -o logs/wifiturbo.log
	curl -sS "$(FTP)/ur0:/data/wifiturbo/wifiturbo_kernel.log" -o logs/wifiturbo_kernel.log
	@echo "Saved logs/wifiturbo.log and logs/wifiturbo_kernel.log"

config:
	@printf '%s\n' '*KERNEL' 'ur0:tai/wifiturbo_kernel.skprx' '' '*main' 'ur0:tai/wifiturbo.suprx' '' \
	  '*NPXS10015' 'ur0:tai/wifiturbo.suprx'

clean:
	rm -rf $(BUILD) logs
