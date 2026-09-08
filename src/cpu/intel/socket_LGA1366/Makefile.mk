## SPDX-License-Identifier: GPL-2.0-only

subdirs-y += ../model_206cx

bootblock-y += ../car/non-evict/cache_as_ram.S
bootblock-y += ../car/bootblock.c
bootblock-y += ../../x86/early_reset.S

postcar-y += ../car/non-evict/exit_car.S

romstage-y += ../car/romstage.c
