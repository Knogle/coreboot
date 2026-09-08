#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Explicit public-source bootstrap only: no make, firmware, ROM or target I/O.

set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
source "${script_dir}/x58_layout.sh"
source "${script_dir}/x58_payload_pins.sh"
mode=prepare
case "${1:-}" in
	--check) mode=check ;;
	-h|--help)
		echo "usage: $0 [--check]"
		echo "No argument: explicitly fetch pinned public payload sources and prepare the trace commit."
		echo "--check: read-only local preflight, no fetch or checkout."
		exit 0 ;;
	"") ;;
	*) echo "unknown option: $1" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { echo "too many arguments" >&2; exit 2; }
seabios_dir="${x58_coreboot_dir}/payloads/external/SeaBIOS/seabios"
ipxe_dir="${x58_coreboot_dir}/payloads/external/iPXE/ipxe"
patch="${x58_tools_dir}/patches/seabios-b52ca86-usb-deferred-trace.patch"
[[ -f "${patch}" && "$(sha256sum "${patch}" | awk '{print $1}')" == \
	"${X58_SEABIOS_PATCH_SHA256}" ]] || {
	echo "missing or changed pinned SeaBIOS patch" >&2; exit 1;
}

require_clean()
{
	local directory="$1" status root
	[[ -d "${directory}" ]] || {
		echo "missing payload checkout: ${directory}; run prepare_x58_payloads.sh explicitly" >&2;
		return 1;
	}
	root="$(git -C "${directory}" rev-parse --show-toplevel)"
	[[ "${root}" == "${directory}" ]] || {
		echo "payload path is not its own Git checkout: ${directory}" >&2; return 1;
	}
	git -C "${directory}" diff --quiet
	git -C "${directory}" diff --cached --quiet
	status="$(git -C "${directory}" status --porcelain --untracked-files=all)"
	if [[ -n "${status}" && !( "${directory}" == "${ipxe_dir}" &&
		"${status}" == '?? ipxe.rom' ) ]]; then
		echo "refusing to modify dirty payload checkout: ${directory}" >&2
		return 1
	fi
}

ensure_source()
{
	local directory="$1" url="$2" commit="$3"
	if [[ ! -e "${directory}" ]]; then
		[[ "${mode}" == prepare ]] || { require_clean "${directory}"; return 1; }
		mkdir -p -- "$(dirname -- "${directory}")"
		git -c core.hooksPath=/dev/null clone --no-checkout -- "${url}" "${directory}"
		git -C "${directory}" -c core.hooksPath=/dev/null checkout --detach "${commit}"
	fi
	require_clean "${directory}"
	if ! git -C "${directory}" cat-file -e "${commit}^{commit}" 2>/dev/null; then
		[[ "${mode}" == prepare ]] || {
			echo "missing pinned commit in ${directory}" >&2; return 1;
		}
		git -C "${directory}" fetch --no-tags -- "${url}" "${commit}"
	fi
}

ensure_source "${seabios_dir}" "${X58_SEABIOS_URL}" "${X58_SEABIOS_BASE}"
ensure_source "${ipxe_dir}" "${X58_IPXE_URL}" "${X58_IPXE_COMMIT}"

if [[ "${mode}" == prepare ]]; then
	# Recreate the commit rather than assuming that a private object is upstream.
	trace_work="$(mktemp -d /tmp/x58-payload-trace.XXXXXX)"
	trap 'rm -rf -- "${trace_work}"' EXIT
	git -c core.hooksPath=/dev/null clone --quiet --no-hardlinks \
		"${seabios_dir}" "${trace_work}/seabios"
	git -C "${trace_work}/seabios" -c core.hooksPath=/dev/null checkout --quiet --detach "${X58_SEABIOS_BASE}"
	git -C "${trace_work}/seabios" apply --check "${patch}"
	git -C "${trace_work}/seabios" apply "${patch}"
	git -C "${trace_work}/seabios" diff --check
	git -C "${trace_work}/seabios" add src/Kconfig src/hw/usb.h \
		src/hw/usb.c src/hw/usb-ehci.c src/hw/usb-uhci.c src/hw/usb-hid.c
	env GIT_AUTHOR_NAME='X58 bring-up' GIT_AUTHOR_EMAIL=x58-local@example.invalid \
		GIT_COMMITTER_NAME='X58 bring-up' GIT_COMMITTER_EMAIL=x58-local@example.invalid \
		GIT_AUTHOR_DATE=2026-09-06T00:00:00Z GIT_COMMITTER_DATE=2026-09-06T00:00:00Z \
		git -C "${trace_work}/seabios" -c core.hooksPath=/dev/null \
		-c commit.gpgSign=false commit --quiet -m 'usb: add deferred bring-up trace'
	[[ "$(git -C "${trace_work}/seabios" rev-parse HEAD)" == "${X58_SEABIOS_TRACE}" ]] || {
		echo "SeaBIOS patch commit is not reproducible; refusing continuation" >&2; exit 1;
	}
	git -C "${seabios_dir}" fetch --quiet "${trace_work}/seabios" "${X58_SEABIOS_TRACE}"
	git -C "${seabios_dir}" -c core.hooksPath=/dev/null checkout --quiet --detach "${X58_SEABIOS_TRACE}"
	git -C "${ipxe_dir}" -c core.hooksPath=/dev/null checkout --quiet --detach "${X58_IPXE_COMMIT}"
fi
require_clean "${seabios_dir}"
require_clean "${ipxe_dir}"
[[ "$(git -C "${seabios_dir}" rev-parse HEAD)" == "${X58_SEABIOS_TRACE}" ]] || {
	echo "SeaBIOS trace revision not selected; run explicit preparation" >&2; exit 1;
}
[[ "$(git -C "${ipxe_dir}" rev-parse HEAD)" == "${X58_IPXE_COMMIT}" ]] || {
	echo "iPXE pinned revision not selected; run explicit preparation" >&2; exit 1;
}
printf 'Payload sources ready: SeaBIOS %s; iPXE %s\n' "${X58_SEABIOS_TRACE}" "${X58_IPXE_COMMIT}"
echo 'No firmware image was built. Intel microcode and the coreboot toolchain are separate prerequisites.'
