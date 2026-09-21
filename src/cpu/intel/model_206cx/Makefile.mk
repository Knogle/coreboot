## SPDX-License-Identifier: GPL-2.0-only

# Historical configurations retain the BSP-only milestone. A mainboard selects
# this generic model capability when it supplies the required CPU policy.
ramstage-$(CONFIG_CPU_INTEL_MODEL_206CX_INIT) += cpu_init.c
romstage-$(CONFIG_CPU_INTEL_MODEL_206CX_INIT) += mca_diagnostic.c
ramstage-$(CONFIG_CPU_INTEL_MODEL_206CX_INIT) += mca_diagnostic.c

ifneq ($(CONFIG_CPU_INTEL_COMMON_TIMEBASE),y)
ramstage-y += timebase_stub.c
endif

subdirs-y += ../../intel/microcode
cpu_microcode_bins += 3rdparty/intel-microcode/intel-ucode/06-2c-02
