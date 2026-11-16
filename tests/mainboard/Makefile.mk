# SPDX-License-Identifier: GPL-2.0-only

tests-y += b565d4_cpu_topology-test
b565d4_cpu_topology-test-srcs += tests/mainboard/b565d4_cpu_topology-test.c
b565d4_cpu_topology-test-srcs += src/mainboard/asrockrack/b565d4_v1l/ramstage/cpu_topology.c
b565d4_cpu_topology-test-cflags += -Isrc/mainboard/asrockrack/b565d4_v1l/ramstage/include
b565d4_cpu_topology-test-cflags += -Isrc/soc/amd/cezanne/include

tests-y += b565d4_cpu_ids-test
b565d4_cpu_ids-test-srcs += tests/mainboard/b565d4_cpu_ids-test.c
b565d4_cpu_ids-test-cflags += -Isrc/mainboard/asrockrack/b565d4_v1l/ramstage/include
b565d4_cpu_ids-test-cflags += -Isrc/soc/amd/cezanne/include

tests-y += b565d4_pcie_options-test
b565d4_pcie_options-test-srcs += tests/mainboard/b565d4_pcie_options-test.c
b565d4_pcie_options-test-srcs += src/mainboard/asrockrack/b565d4_v1l/ramstage/pcie_options.c
b565d4_pcie_options-test-cflags += -Isrc/mainboard/asrockrack/b565d4_v1l/ramstage/include
b565d4_pcie_options-test-config += CONFIG_B565D4_DESKTOP_NATIVE=0

tests-y += b565d4_pcie_options-desktop-test
b565d4_pcie_options-desktop-test-srcs += tests/mainboard/b565d4_pcie_options-test.c
b565d4_pcie_options-desktop-test-srcs += src/mainboard/asrockrack/b565d4_v1l/ramstage/pcie_options.c
b565d4_pcie_options-desktop-test-cflags += -Isrc/mainboard/asrockrack/b565d4_v1l/ramstage/include
b565d4_pcie_options-desktop-test-config += CONFIG_B565D4_DESKTOP_NATIVE=1
