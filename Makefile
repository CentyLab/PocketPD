.PHONY: test test-root test-tempo build-v1 build-v2 build-hw1_0 build-hw1_1 build-hw1_3 build-all clean dist-clean

.DEFAULT_GOAL := test

DIST_DIR := dist

# Pull firmware_version from [common] section in platformio.ini —
# single source of truth shared with the build envs.
FIRMWARE_VERSION := $(shell awk -F'=' '/^\[common\]/{f=1;next} /^\[/{f=0} \
	f && $$1 ~ /^[[:space:]]*firmware_version[[:space:]]*$$/ {gsub(/[[:space:]]/,"",$$2); print $$2; exit}' platformio.ini)

# $(1) = pio env name (e.g. HW1_0_V2); _V2 stripped for dist label.
# $(2) = version
define copy_uf2
	@mkdir -p $(DIST_DIR)
	@cp .pio/build/$(1)/firmware.uf2 $(DIST_DIR)/PocketPD_$(subst _V2,,$(1))-v$(2).uf2
	@echo "→ $(DIST_DIR)/PocketPD_$(subst _V2,,$(1))-v$(2).uf2"
endef

test: test-root test-tempo

test-root:
	pio test -e native

test-tempo:
	pio test -e native -d lib/tempo

build-v1:
	pio run -e HW1_3
	$(call copy_uf2,HW1_3,1.0.0)

build-v2:
	pio run -e HW1_3_V2
	$(call copy_uf2,HW1_3_V2,$(FIRMWARE_VERSION))

build-hw1_0:
	pio run -e HW1_0_V2
	$(call copy_uf2,HW1_0_V2,$(FIRMWARE_VERSION))

build-hw1_1:
	pio run -e HW1_1_V2
	$(call copy_uf2,HW1_1_V2,$(FIRMWARE_VERSION))

build-hw1_3:
	pio run -e HW1_3_V2
	$(call copy_uf2,HW1_3_V2,$(FIRMWARE_VERSION))

build-all: build-hw1_0 build-hw1_1 build-hw1_3

clean:
	pio run -t clean

dist-clean:
	rm -rf $(DIST_DIR)
