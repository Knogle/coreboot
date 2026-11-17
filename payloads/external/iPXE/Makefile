## SPDX-License-Identifier: GPL-2.0-only

# 2025.12 - Pinned stable revision from December 2025
# When updating, change the name both here and in payloads/external/iPXE/Kconfig
STABLE_COMMIT_ID=7c39c04a537ce29dccc6f2bae9749d1d371429c1

TAG-$(CONFIG_IPXE_MASTER)=origin/master
TAG-$(CONFIG_IPXE_STABLE)=$(STABLE_COMMIT_ID)

project_name=iPXE
project_dir=ipxe
project_git_repo=https://git.ipxe.org/ipxe.git

# GCC 16 fixes from upstream iPXE commits 2d28657, c18d0a2, be35d67.
# Apply only to the pinned 2025.12 EFI donor.
override ipxe_gcc16_base := 7c39c04a537ce29dccc6f2bae9749d1d371429c1
ipxe_gcc16_patch=$(abspath patches/0001-ipxe-gcc16-upstream-backport.patch)
ipxe_patch_state=$(abspath patches-ipxe)

ifeq ($(CONFIG_IPXE_BUILD_EFI),y)
ifeq ($(CONFIG_IPXE_EFI_DEVICE_SPECIFIC),y)
IPXE_BUILD_TARGET := bin-x86_64-efi-sb/$(PXE_ROM_PCI_ID).efi
else
IPXE_BUILD_TARGET := bin-x86_64-efi-sb/ipxe.efi
endif
# Workaround problems with unrecognized ipxe/util/elf2efi relocations
IPXE_EXTRA_CFLAGS := $(CFLAGS_x86_64) -fno-pic
else
IPXE_BUILD_TARGET := bin/$(PXE_ROM_PCI_ID).rom
endif

# iPXE hashes its object archive into the ROM build ID, including DWARF.
# Normalize the source directory for reproducible builds while preserving
# the upstream ID calculation and differences in executable inputs.
ifneq ($(SOURCE_DATE_EPOCH),)
IPXE_EXTRA_CFLAGS += -fdebug-prefix-map=$(abspath $(project_dir)/src)=.
endif
ifneq ($(strip $(IPXE_EXTRA_CFLAGS)),)
PXE_MAKE_OPTS += EXTRA_CFLAGS="$(IPXE_EXTRA_CFLAGS)"
endif

all: build

$(project_dir):
	echo "    Cloning $(project_name) from Git"
	git clone $(project_git_repo) $(project_dir)

fetch: $(project_dir)
	cd $(project_dir); \
		git show $(TAG-y) >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] || [ "$(TAG-y)" = "origin/master" ]; then \
			echo "    Fetching new commits from the $(project_name) repo"; \
			git fetch; \
		fi

# Shared patch-state helper retains the historical on-disk owner marker.
checkout: fetch
	python3 ../patches.py undo --source "$(abspath $(project_dir))" --state "$(ipxe_patch_state)"
	echo "    Checking out $(project_name) revision $(TAG-y)"
	set -e; cd $(project_dir); \
		git checkout master; \
		git branch -D coreboot 2>/dev/null || true; \
		git checkout -b coreboot $(TAG-y)

patches: checkout
	@test -z "$$(git -C $(project_dir) status --porcelain --untracked-files=no)"
ifeq ($(CONFIG_IPXE_STABLE)$(CONFIG_IPXE_BUILD_EFI),yy)
	@test "$$(git -C $(project_dir) rev-parse HEAD)" = "$(ipxe_gcc16_base)"
	python3 ../patches.py apply --source "$(abspath $(project_dir))" --state "$(ipxe_patch_state)" --patch "$(ipxe_gcc16_patch)"
endif
	@git -C $(project_dir) diff --check

config: patches
ifeq ($(CONSOLE_SERIAL),yy)
	sed -i'' 's|//#define\s*CONSOLE_SERIAL.*|#define CONSOLE_SERIAL|' "$(project_dir)/src/config/console.h"
	sed -i'' 's|#define\s*COMCONSOLE.*|#define COMCONSOLE $(IPXE_UART)|' "$(project_dir)/src/config/serial.h"
	sed -i'' 's|#define\s*COMSPEED.*|#define COMSPEED $(CONFIG_TTYS0_BAUD)|'  "$(project_dir)/src/config/serial.h"
endif
ifeq ($(CONFIG_HAS_SCRIPT),y)
	sed -i'' 's|//#define\s*IMAGE_SCRIPT.*|#define IMAGE_SCRIPT|' "$(project_dir)/src/config/general.h"
endif
ifeq ($(CONFIG_IPXE_NO_PROMPT),y)
	sed -i'' 's|#define\s*BANNER_TIMEOUT.*|#define BANNER_TIMEOUT 0|' "$(project_dir)/src/config/general.h"
endif
ifeq ($(CONFIG_IPXE_HAS_HTTPS),y)
	sed -i'' 's|.*DOWNLOAD_PROTO_HTTPS|#define DOWNLOAD_PROTO_HTTPS|g' "$(project_dir)/src/config/general.h"
endif
ifeq ($(CONFIG_IPXE_TRUST_CMD),y)
	sed -i'' 's|.*IMAGE_TRUST_CMD|#define IMAGE_TRUST_CMD|g' "$(project_dir)/src/config/general.h"
endif

ifeq ($(CONFIG_HAS_SCRIPT),y)
IPXE_EMBED_OPTION := EMBED=$(CONFIG_SCRIPT)
endif

build: config $(CONFIG_SCRIPT)
	@set -e; \
	trap 'git -C $(project_dir) restore src/config/console.h src/config/serial.h src/config/general.h' EXIT; \
	echo "    MAKE       $(project_name) $(TAG-y) $(IPXE_EMBED_OPTION)"; \
	$(MAKE) -C $(project_dir)/src $(IPXE_BUILD_TARGET) $(IPXE_EMBED_OPTION) $(PXE_MAKE_OPTS); \
	cp $(project_dir)/src/$(IPXE_BUILD_TARGET) $(project_dir)/ipxe.rom

clean:
	test -d $(project_dir) && $(MAKE) -C $(project_dir)/src veryclean || exit 0
	rm -f $(project_dir)/ipxe.rom

distclean:
	rm -rf $(project_dir) patches-ipxe

.PHONY: all fetch checkout patches config build clean distclean
