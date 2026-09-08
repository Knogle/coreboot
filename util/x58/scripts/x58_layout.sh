#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Source this file; never infer the coreboot tree from the caller's cwd.

x58_tools_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
if [[ -f "${x58_tools_dir}/../../src/mainboard/msi/x58_pro_e/Kconfig" &&
	-f "${x58_tools_dir}/../../Makefile" ]]; then
	x58_coreboot_dir="$(cd -- "${x58_tools_dir}/../.." && pwd -P)"
elif [[ -f "${x58_tools_dir}/coreboot/src/mainboard/msi/x58_pro_e/Kconfig" &&
	-f "${x58_tools_dir}/coreboot/Makefile" ]]; then
	# Retain compatibility with the original private research-workspace layout.
	x58_coreboot_dir="${x58_tools_dir}/coreboot"
else
	echo "cannot locate the MSI X58 coreboot source tree from ${x58_tools_dir}" >&2
	return 1 2>/dev/null || exit 1
fi
