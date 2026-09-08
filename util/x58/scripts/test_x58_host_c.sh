#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Compile and run simulated-backend host tests, never firmware or hardware.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
source "${script_dir}/x58_layout.sh"
x58_test_dir="$(mktemp -d /tmp/x58-host-c-tests.XXXXXX)"
cleanup()
{
	case "${x58_test_dir}" in
		/tmp/x58-host-c-tests.*) rm -rf -- "${x58_test_dir}" ;;
		*) echo "refusing unexpected cleanup target" >&2 ;;
	esac
}
trap cleanup EXIT
board="${x58_coreboot_dir}/src/mainboard/msi/x58_pro_e"
compiler="${CC:-cc}"
for unit in rommon_script ram_loader rommon_irqprobe; do
	defines=()
	if [[ "${unit}" == rommon_irqprobe ]]; then
		# Required: exclude all native privileged I/O/IDT/LAPIC operations.
		defines=(-DX58_IRQPROBE_HOST_TEST)
	fi
	"${compiler}" -std=c11 -Wall -Wextra -Werror "${defines[@]}" \
		-I "${board}" "${board}/${unit}.c" \
		"${script_dir}/test_x58_${unit}.c" -o "${x58_test_dir}/${unit}"
	"${x58_test_dir}/${unit}"
done
