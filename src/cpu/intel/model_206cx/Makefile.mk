## SPDX-License-Identifier: GPL-2.0-only

# CPU driver and AP startup are intentionally deferred.
ifneq ($(CONFIG_CPU_INTEL_COMMON_TIMEBASE),y)
ramstage-y += timebase_stub.c
endif

subdirs-y += ../../intel/microcode
cpu_microcode_bins += 3rdparty/intel-microcode/intel-ucode/06-2c-02
