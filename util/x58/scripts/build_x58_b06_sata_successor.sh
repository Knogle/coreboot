#!/usr/bin/env bash
# Reproducibly build one corrected-route MSI X58 Pro-E SATA successor.
# The original supported set {b06vv|b06vw|b06vx} remains unchanged.  B06VY is
# the isolated IOAPIC successor; B06VZ adds fixed resources, B06WA adds
# deterministic quiescent HPET decode, B06WB adds an early TCO halt, B06WC
# integrates PIC/ACPI/read-only USB admission, and B06WD corrects the SATA
# software policy before adding exact-gated SeaBIOS PS/2/USB input support.
# B06WE corrects the read-only ACPI SAD/PCIEXBAR gate discovered by B06WD.
# B06WF adds a separately unlocked late USB diagnostic lab to B06WE; B06WG
# promotes only the GPIO57 result to a bounded automatic SeaBIOS handoff, and
# B06WH preserves B06WG while lowering only the SeaBIOS debug level to 6;
# B06WI adds the exact-gated vendor-correlated IRQ/ACPI experiment; B06WJ
# preserves that hardware path and extends the native CPU/LPC/HPET ACPI tables.
# B06WK adds native IOH/legacy-memory ACPI repairs and fixed-button policy;
# existing releases are immutable, including WJ built before the shared
# resource-consumer flag correction that accompanies WK.

set -euo pipefail

if [[ $# != 1 ]]; then
	echo "usage: $0 {b06vv|b06vw|b06vx|b06vy|b06vz|b06wa|b06wb|b06wc|b06wd|b06we|b06wf|b06wg|b06wh|b06wi|b06wj|b06wk}" >&2
	exit 2
fi

variant="$1"
development=0
if [[ "${variant}" == development ]]; then
	# Reuse WK's feature checks, never its released identity or artifact names.
	variant=b06wk
	development=1
fi
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
source "${repo_root}/scripts/x58_layout.sh"
coreboot_dir="${x58_coreboot_dir}"
config="${repo_root}/configs/x58-pro-e-${variant}.config"
public_rom="${repo_root}/builds/experimental/msi-x58-pro-e-${variant}-coreboot-base-4MiB.rom"
local_dir="${repo_root}/blobs-local/msi-x58-pro-e/${variant}"
vendor_rom="${X58_SATA_SUCCESSOR_VENDOR_ROM:-${repo_root}/blobs-local/msi-x58-pro-e/7522v8F/A7522IMS.8F0}"
jobs="${X58_SATA_SUCCESSOR_JOBS:-8}"
epoch="${X58_SATA_SUCCESSOR_SOURCE_DATE_EPOCH:-1788703200}"
ipxe_commit="7c39c04a537ce29dccc6f2bae9749d1d371429c1"
ipxe_sha256="8b21a133a1a795bcd19f7945c23306d37b348fedc8299527e3ce3d66c906ee80"
seabios_base="b52ca86e094d19b58e2304417787e96b940e39c6"
seabios_trace="5497f43189374647b3b0f282aa497e71c891f3c5"
seabios_patch="${repo_root}/patches/seabios-b52ca86-usb-deferred-trace.patch"
seabios_patch_sha256="f222796ea8029444106ca009045b1528b9a325fa5b84706ca252739d353c232f"
seabios_dir="${coreboot_dir}/payloads/external/SeaBIOS/seabios"
trace=0

case "${variant}" in
	b06vv)
		build_id="X58PROE-B06VV-ICH10-PCS-SCLK-20260906"
		;;
	b06vw)
		build_id="X58PROE-B06VW-ICH10-AHCI-MMIO-20260906"
		;;
	b06vx)
		build_id="X58PROE-B06VX-AHCI-USBTRACE-20260906"
		trace=1
		;;
	b06vy)
		build_id="X58PROE-B06VY-ICH10-IOAPIC-MASK-20260906"
		trace=1
		;;
	b06vz)
		build_id="X58PROE-B06VZ-FIXED-RESOURCES-20260906"
		trace=1
		;;
	b06wa)
		build_id="X58PROE-B06WA-ICH10-HPET-DECODE-20260906"
		trace=1
		;;
	b06wb)
		build_id="X58PROE-B06WB-ICH10-TCO-HALT-20260906"
		trace=1
		;;
	b06wc)
		build_id="X58PROE-B06WC-INTEGRATED-PLATFORM-20260906"
		trace=1
		;;
	b06wd)
		build_id="X58PROE-B06WD-SEABIOS-INPUT-20260906"
		trace=1
		;;
	b06we)
		build_id="X58PROE-B06WE-ACPI-SAD-BDF-FIX-20260907"
		trace=1
		;;
	b06wf)
		build_id="X58PROE-B06WF-LATE-USB-LAB-20260907"
		trace=1
		;;
	b06wg)
		build_id="X58PROE-B06WG-AUTO-USB-SEABIOS-20260907"
		trace=1
		;;
	b06wh)
		build_id="X58PROE-B06WH-AUTO-USB-SEABIOS-20260907"
		trace=1
		;;
	b06wi)
		build_id="X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907"
		trace=1
		;;
	b06wj)
		build_id="X58PROE-B06WJ-ACPI-PLATFORM-20260907"
		trace=1
		;;
	b06wk)
		build_id="X58PROE-B06WK-ACPI-REPAIR-20260908"
		trace=1
		;;
	*)
		echo "unsupported corrected-route successor: ${variant}" >&2
		exit 2
		;;
esac

if [[ ${development} == 1 ]]; then
	config="${repo_root}/configs/x58-pro-e-development.config"
	build_id="X58PROE-DEVELOPMENT-SPD10"
	local_dir="${X58_OUTPUT_DIR:-${repo_root}/blobs-local/msi-x58-pro-e/development-spd10}"
	public_rom="${local_dir}/msi-x58-pro-e-development-spd10-coreboot-base-4MiB.rom"
	vendor_rom="${X58_VENDOR_ROM:-}"
	[[ -n "${vendor_rom}" ]] || {
		echo "set X58_VENDOR_ROM to your own pinned A7522IMS.8F0; no vendor firmware is downloaded" >&2;
		exit 2;
	}
elif grep -q '^CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT=y$' "${config}"; then
	echo "refusing a changed SPD-v10 image under a historical release identity; use build_x58_development.sh" >&2
	exit 2
else
	echo "Historical experiment configuration: this source checkout is not an archived release reproduction." >&2
fi

if [[ "${variant}" == b06vz ]]; then
	echo "B06VZ is BUILD-ONLY: do not flash before a recorded B06VY hardware PASS." >&2
elif [[ "${variant}" == b06wa ]]; then
	echo "B06WA is BUILD-ONLY: do not flash before a recorded B06VZ hardware PASS." >&2
elif [[ "${variant}" == b06wb ]]; then
	echo "B06WB is BUILD-ONLY: do not flash before recorded B06VY, B06VZ and B06WA hardware PASSes." >&2
elif [[ "${variant}" == b06wc ]]; then
	echo "B06WC is an integrated EXPERIMENTAL image: predecessor qualification is intentionally not required; socketed-flash recovery is required." >&2
elif [[ "${variant}" == b06wd ]]; then
	echo "B06WD is an EXPERIMENTAL SeaBIOS input image: use a preattached USB or PS/2 keyboard and retain socketed-flash recovery." >&2
elif [[ "${variant}" == b06we ]]; then
	echo "B06WE is an EXPERIMENTAL one-delta ACPI SAD-BDF correction: retain COM1 and socketed-flash recovery." >&2
elif [[ "${variant}" == b06wf ]]; then
	echo "B06WF is an EXPERIMENTAL late USB diagnostic lab: retain COM1 and socketed-flash recovery; do not invoke writes without a recorded hypothesis." >&2
elif [[ "${variant}" == b06wg ]]; then
	echo "B06WG is an EXPERIMENTAL automatic GPIO57 USB release: preattach one known keyboard, capture COM1, and retain one-second AC/socketed-flash recovery." >&2
elif [[ "${variant}" == b06wh ]]; then
	echo "B06WH is the EXPERIMENTAL B06WG-equivalent path with only SeaBIOS debug level 6: preattach known USB media/input, capture COM1, and retain one-second AC/socketed-flash recovery." >&2
elif [[ "${variant}" == b06wi ]]; then
	echo "B06WI is an EXPERIMENTAL exact-gated vendor-correlated IRQ/ACPI path: capture COM1 through ACPI and OS handoff, and retain one-second AC/socketed-flash recovery." >&2
elif [[ "${variant}" == b06wj ]]; then
	echo "B06WJ is an EXPERIMENTAL native CPU/LPC/HPET ACPI platform successor: RAM/QPI, IRQ and quiet USB behavior are inherited; capture COM1 and the Windows stop screen, and retain one-second AC/socketed-flash recovery." >&2
elif [[ "${variant}" == b06wk ]]; then
	echo "B06WK is an EXPERIMENTAL native ACPI repair image: inherited RAM/QPI, IRQ and quiet USB hardware paths; IOH routes, legacy memory resources, fixed-button policy and common resource-consumer flags repaired. Capture COM1 and the Windows stop screen; retain one-second AC/socketed-flash recovery. No A5 fix is claimed." >&2
fi

temp_dir="$(mktemp -d "/tmp/x58-${variant}-build.XXXXXX")"
cleanup()
{
	rm -rf -- "${temp_dir}"
}
trap cleanup EXIT

cd -- "${repo_root}"
unset PYTHONOPTIMIZE

install_exact()
{
	local source="$1"
	local target="$2"

	if [[ -e "${target}" ]]; then
		if ! cmp -s -- "${source}" "${target}"; then
			echo "refusing to replace differing artifact: ${target}" >&2
			return 1
		fi
		return 0
	fi
	mkdir -p -- "$(dirname -- "${target}")"
	install -m 0644 -- "${source}" "${target}"
}

require_line()
{
	local file="$1"
	local line="$2"

	grep -Fqx -- "${line}" "${file}" || {
		echo "required configuration missing from ${file}: ${line}" >&2
		return 1
	}
}

require_clean_payload_sources()
{
	local directory
	local status

	for directory in \
		"${coreboot_dir}/payloads/external/iPXE/ipxe" \
		"${seabios_dir}"; do
		git -C "${directory}" diff --quiet
		git -C "${directory}" diff --cached --quiet
		status="$(git -C "${directory}" status --porcelain --untracked-files=normal)"
		case "${directory}:${status}" in
			*payloads/external/iPXE/ipxe:""|*payloads/external/iPXE/ipxe:"?? ipxe.rom"|*payloads/external/SeaBIOS/seabios:"") ;;
			*) echo "payload source tree is not clean: ${directory}: ${status}" >&2; return 1 ;;
		esac
	done
}

prepare_seabios_trace_revision()
{
	local trace_clone="${temp_dir}/seabios-trace"
	local created

	[[ "$(sha256sum "${seabios_patch}" | awk '{print $1}')" == \
		"${seabios_patch_sha256}" ]] || {
		echo "unexpected deferred USB trace patch hash" >&2; return 1;
	}
	[[ "$(git -C "${seabios_dir}" rev-parse "${seabios_base}^{commit}")" == \
		"${seabios_base}" ]] || { echo "missing exact SeaBIOS base revision" >&2; return 1; }
	git clone --quiet --no-hardlinks "${seabios_dir}" "${trace_clone}"
	git -C "${trace_clone}" checkout --quiet --detach "${seabios_base}"
	git -C "${trace_clone}" apply --check "${seabios_patch}"
	git -C "${trace_clone}" apply "${seabios_patch}"
	git -C "${trace_clone}" diff --check
	git -C "${trace_clone}" add src/Kconfig src/hw/usb.h src/hw/usb.c \
		src/hw/usb-ehci.c src/hw/usb-uhci.c src/hw/usb-hid.c
	env GIT_AUTHOR_DATE=2026-09-06T00:00:00Z \
		GIT_COMMITTER_DATE=2026-09-06T00:00:00Z \
		git -C "${trace_clone}" -c user.name='X58 bring-up' \
		-c user.email='x58-local@example.invalid' \
		commit --quiet -m 'usb: add deferred bring-up trace'
	created="$(git -C "${trace_clone}" rev-parse HEAD)"
	[[ "${created}" == "${seabios_trace}" ]] || {
		echo "non-deterministic SeaBIOS trace commit: ${created}" >&2; return 1;
	}
	git -C "${seabios_dir}" fetch --quiet "${trace_clone}" "${seabios_trace}"
}

build_once()
{
	local output="$1"
	local cache_dir="${temp_dir}/cache"

	mkdir -p -- "${cache_dir}/ccache"
	env -u DEBUG make -C "${coreboot_dir}" clean
	cp -- "${config}" "${coreboot_dir}/.config"
	env -u DEBUG CCACHE_DISABLE=1 CCACHE_DIR="${cache_dir}/ccache" \
		XDG_CACHE_HOME="${cache_dir}" SOURCE_DATE_EPOCH="${epoch}" \
		make -C "${coreboot_dir}" SOURCE_DATE_EPOCH="${epoch}" olddefconfig
	env -u DEBUG CCACHE_DISABLE=1 CCACHE_DIR="${cache_dir}/ccache" \
		XDG_CACHE_HOME="${cache_dir}" SOURCE_DATE_EPOCH="${epoch}" \
		make -C "${coreboot_dir}" SOURCE_DATE_EPOCH="${epoch}" -j"${jobs}"
	cp -- "${coreboot_dir}/build/coreboot.rom" "${output}"
}

verify_payloads()
{
	local ipxe_dir="${coreboot_dir}/payloads/external/iPXE/ipxe"
	local ipxe_rom="${ipxe_dir}/ipxe.rom"
	local expected_seabios="${seabios_base}"
	local status

	[[ ${trace} == 0 ]] || expected_seabios="${seabios_trace}"
	[[ "$(git -C "${seabios_dir}" rev-parse HEAD)" == "${expected_seabios}" ]]
	git -C "${seabios_dir}" diff --quiet
	git -C "${seabios_dir}" diff --cached --quiet
	[[ -z "$(git -C "${seabios_dir}" status --porcelain --untracked-files=normal)" ]]
	[[ "$(git -C "${ipxe_dir}" rev-parse HEAD)" == "${ipxe_commit}" ]]
	git -C "${ipxe_dir}" diff --quiet
	git -C "${ipxe_dir}" diff --cached --quiet
	status="$(git -C "${ipxe_dir}" status --porcelain --untracked-files=normal)"
	[[ -z "${status}" || "${status}" == "?? ipxe.rom" ]]
	if [[ ${development} == 1 ]]; then
		python3 "${repo_root}/scripts/x58_validate_ipxe.py" "${ipxe_rom}"
	else
		[[ "$(sha256sum "${ipxe_rom}" | awk '{print $1}')" == "${ipxe_sha256}" ]]
		[[ "$(stat -c '%s' "${ipxe_rom}")" == 95232 ]]
	fi
}

[[ -f "${vendor_rom}" ]] || { echo "missing user-supplied vendor ROM: ${vendor_rom}" >&2; exit 1; }
[[ "${jobs}" =~ ^[1-9][0-9]*$ ]] || { echo "X58_SATA_SUCCESSOR_JOBS must be positive" >&2; exit 1; }
[[ "${epoch}" =~ ^[0-9]+$ ]] || { echo "SOURCE_DATE_EPOCH must be a nonnegative integer" >&2; exit 1; }
if [[ ${development} == 1 ]]; then
	# Explicit source preparation is separate; this preflight never fetches.
	bash "${repo_root}/scripts/prepare_x58_payloads.sh" --check
	[[ -x "${coreboot_dir}/util/crossgcc/xgcc/bin/i386-elf-gcc" &&
		-x "${coreboot_dir}/util/crossgcc/xgcc/bin/iasl" ]] || {
		echo "build the pinned coreboot crossgcc-i386 toolchain and IASL first" >&2; exit 1;
	}
	[[ -f "${coreboot_dir}/3rdparty/intel-microcode/intel-ucode/06-2c-02" ]] || {
		echo "explicitly initialize the pinned 3rdparty/intel-microcode submodule first" >&2; exit 1;
	}
	PYTHONPATH="${repo_root}/scripts" python3 -B -c \
		'import sys; from pathlib import Path; import msi_vendor_blobs; msi_vendor_blobs.load_vendor_rom(Path(sys.argv[1]))' "${vendor_rom}"
	python3 -B -m unittest discover -s tests -v
	PYTHONPATH="${repo_root}/scripts" python3 -B -m unittest discover -s scripts -p 'test_*.py' -v
fi
require_clean_payload_sources
if [[ ${trace} == 1 && ${development} == 0 ]]; then
	prepare_seabios_trace_revision
fi

first_rom="${temp_dir}/coreboot-first.rom"
second_rom="${temp_dir}/coreboot-second.rom"
build_once "${first_rom}"

for line in \
	'CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT=y' \
	'CONFIG_X58_PRO_E_B06VQ_ICHBASE1=y' \
	'CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE=y' \
	'CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK=y' \
	'# CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 is not set' \
	'# CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP is not set' \
	'CONFIG_PAYLOAD_SEABIOS=y' \
	'CONFIG_SEABIOS_HARDWARE_IRQ=y' \
	'CONFIG_PXE=y' 'CONFIG_BUILD_IPXE=y' 'CONFIG_PXE_ROM_ID="10ec,8168"' \
	'# CONFIG_IPXE_NO_PROMPT is not set' \
	'# CONFIG_PCI_ALLOW_BUS_MASTER_ANY_DEVICE is not set'; do
	require_line "${coreboot_dir}/.config" "${line}"
done
if [[ "${variant}" == b06wd || "${variant}" == b06we || \
	"${variant}" == b06wf || "${variant}" == b06wg || \
	"${variant}" == b06wh || "${variant}" == b06wi || \
	"${variant}" == b06wj || "${variant}" == b06wk ]]; then
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT=y'
	require_line "${coreboot_dir}/.config" 'CONFIG_DRIVERS_PS2_KEYBOARD=y'
	require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_PS2_TIMEOUT=1000'
fi
if [[ "${variant}" == b06we || "${variant}" == b06wf || \
	"${variant}" == b06wg || "${variant}" == b06wh || \
	"${variant}" == b06wi || "${variant}" == b06wj || \
	"${variant}" == b06wk ]]; then
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX=y'
fi
if [[ "${variant}" == b06wf ]]; then
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WF_USB_LAB=y'
fi
if [[ "${variant}" == b06wg ]]; then
	require_line "${coreboot_dir}/.config" '# CONFIG_X58_PRO_E_B06WF_USB_LAB is not set'
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS=y'
	require_line "${coreboot_dir}/.config" 'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/$(MAINBOARDDIR)/config_seabios_b06wg_usbtrace8"'
	require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_DEBUG_LEVEL=8'
fi
if [[ "${variant}" == b06wh ]]; then
	require_line "${coreboot_dir}/.config" '# CONFIG_X58_PRO_E_B06WF_USB_LAB is not set'
	require_line "${coreboot_dir}/.config" '# CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS is not set'
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=y'
	require_line "${coreboot_dir}/.config" 'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/$(MAINBOARDDIR)/config_seabios_b06wh_usbtrace6"'
	require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_DEBUG_LEVEL=6'
fi
if [[ "${variant}" == b06wi || "${variant}" == b06wj || \
	"${variant}" == b06wk ]]; then
	require_line "${coreboot_dir}/.config" '# CONFIG_X58_PRO_E_B06WF_USB_LAB is not set'
	require_line "${coreboot_dir}/.config" '# CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS is not set'
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=y'
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI=y'
	require_line "${coreboot_dir}/.config" 'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/$(MAINBOARDDIR)/config_seabios_b06wh_usbtrace6"'
	require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_DEBUG_LEVEL=6'
fi
if [[ "${variant}" == b06wj ]]; then
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM=y'
	require_line "${coreboot_dir}/.config" 'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06WJ ACPI platform experiment"'
	require_line "${coreboot_dir}/.config" 'CONFIG_LOCALVERSION="x58-pro-e-b06wj"'
fi
if [[ "${variant}" == b06wk ]]; then
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM=y'
	require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR=y'
	if [[ ${development} == 1 ]]; then
		require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT=y'
		require_line "${coreboot_dir}/.config" 'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E development SPD10"'
		require_line "${coreboot_dir}/.config" 'CONFIG_LOCALVERSION="x58-pro-e-development-spd10"'
	else
	require_line "${coreboot_dir}/.config" 'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06WK ACPI repair experiment"'
	require_line "${coreboot_dir}/.config" 'CONFIG_LOCALVERSION="x58-pro-e-b06wk"'
	fi
fi

auto_config="${coreboot_dir}/build/auto.conf"
for line in \
	'CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n' \
	'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_REQUIRED_FIELDS=y' \
	'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP=y' \
	'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA2_DISABLE=y' \
	'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE=y' \
	'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD=y' \
	'CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS=n' \
	'CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK=n' \
	'CONFIG_SMP=n' 'CONFIG_XAPIC_ONLY=y'; do
	require_line "${auto_config}" "${line}"
done

case "${variant}" in
	b06vv)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=n'
		;;
	b06vw)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=n'
		;;
	b06vx)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=n'
		;;
	b06vy)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=n'
		;;
	b06vz)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=n'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=n'
		;;
	b06wa)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=n'
		require_line "${auto_config}" 'CONFIG_IOAPIC=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n'
		;;
	b06wb)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=y'
		require_line "${auto_config}" 'CONFIG_IOAPIC=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_WATCHDOG=n'
		require_line "${auto_config}" 'CONFIG_USE_WATCHDOG_ON_BOOT=n'
		;;
	b06wc)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT=y'
		require_line "${auto_config}" 'CONFIG_IOAPIC=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_WATCHDOG=n'
		require_line "${auto_config}" 'CONFIG_USE_WATCHDOG_ON_BOOT=n'
		require_line "${auto_config}" 'CONFIG_HAVE_ACPI_TABLES=y'
		require_line "${auto_config}" 'CONFIG_ACPI_CUSTOM_MADT=y'
		require_line "${auto_config}" 'CONFIG_ACPI_CUSTOM_MCFG=y'
		require_line "${auto_config}" 'CONFIG_NO_ECAM_MMCONF_SUPPORT=y'
		require_line "${auto_config}" 'CONFIG_NO_SMM=y'
		;;
	b06wd)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT=y'
		require_line "${auto_config}" 'CONFIG_DRIVERS_PS2_KEYBOARD=y'
		require_line "${auto_config}" 'CONFIG_IOAPIC=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_WATCHDOG=n'
		require_line "${auto_config}" 'CONFIG_USE_WATCHDOG_ON_BOOT=n'
		require_line "${auto_config}" 'CONFIG_HAVE_ACPI_TABLES=y'
		require_line "${auto_config}" 'CONFIG_ACPI_CUSTOM_MADT=y'
		require_line "${auto_config}" 'CONFIG_ACPI_CUSTOM_MCFG=y'
		require_line "${auto_config}" 'CONFIG_NO_ECAM_MMCONF_SUPPORT=y'
		require_line "${auto_config}" 'CONFIG_NO_SMM=y'
		;;
	b06we)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT=y'
		require_line "${auto_config}" 'CONFIG_DRIVERS_PS2_KEYBOARD=y'
		require_line "${auto_config}" 'CONFIG_IOAPIC=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_WATCHDOG=n'
		require_line "${auto_config}" 'CONFIG_USE_WATCHDOG_ON_BOOT=n'
		require_line "${auto_config}" 'CONFIG_HAVE_ACPI_TABLES=y'
		require_line "${auto_config}" 'CONFIG_ACPI_CUSTOM_MADT=y'
		require_line "${auto_config}" 'CONFIG_ACPI_CUSTOM_MCFG=y'
		require_line "${auto_config}" 'CONFIG_NO_ECAM_MMCONF_SUPPORT=y'
		require_line "${auto_config}" 'CONFIG_NO_SMM=y'
		;;
	b06wf|b06wg|b06wh|b06wi|b06wj|b06wk)
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WB_TCO_HALT=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT=y'
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX=y'
		if [[ "${variant}" == b06wf ]]; then
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WF_USB_LAB=y'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS=n'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=n'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI=n'
		elif [[ "${variant}" == b06wg ]]; then
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WF_USB_LAB=n'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS=y'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=n'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI=n'
		elif [[ "${variant}" == b06wh ]]; then
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WF_USB_LAB=n'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS=n'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=y'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI=n'
		else
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WF_USB_LAB=n'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS=n'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=y'
			require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI=y'
		fi
		require_line "${auto_config}" 'CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT=y'
		require_line "${auto_config}" 'CONFIG_DRIVERS_PS2_KEYBOARD=y'
		require_line "${auto_config}" 'CONFIG_IOAPIC=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n'
		require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_COMMON_WATCHDOG=n'
		require_line "${auto_config}" 'CONFIG_USE_WATCHDOG_ON_BOOT=n'
		require_line "${auto_config}" 'CONFIG_HAVE_ACPI_TABLES=y'
		require_line "${auto_config}" 'CONFIG_ACPI_CUSTOM_MADT=y'
		require_line "${auto_config}" 'CONFIG_ACPI_CUSTOM_MCFG=y'
		require_line "${auto_config}" 'CONFIG_NO_ECAM_MMCONF_SUPPORT=y'
		require_line "${auto_config}" 'CONFIG_NO_SMM=y'
		;;
esac
if [[ "${variant}" == b06wj || "${variant}" == b06wk ]]; then
	require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM=y'
	require_line "${auto_config}" 'CONFIG_HPET_MIN_TICKS=0x80'
else
	require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM=n'
fi
if [[ "${variant}" == b06wk ]]; then
	require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR=y'
else
	require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR=n'
fi

if [[ "${variant}" != b06wc && "${variant}" != b06wd && \
	"${variant}" != b06we && "${variant}" != b06wf && \
	"${variant}" != b06wg && "${variant}" != b06wh && \
	"${variant}" != b06wi && "${variant}" != b06wj && \
	"${variant}" != b06wk ]]; then
	require_line "${auto_config}" 'CONFIG_HAVE_ACPI_TABLES=n'
fi

seabios_config="${seabios_dir}/.config"
for line in 'CONFIG_BOOT=y' 'CONFIG_DRIVES=y' 'CONFIG_ATA=y' 'CONFIG_AHCI=y' \
	'CONFIG_USB=y' 'CONFIG_USB_UHCI=y' 'CONFIG_USB_EHCI=y' \
	'CONFIG_USB_MSC=y' 'CONFIG_USB_HUB=y' 'CONFIG_USB_KEYBOARD=y' \
	'CONFIG_KEYBOARD=y' 'CONFIG_PS2PORT=y' \
	'CONFIG_OPTIONROMS=y' 'CONFIG_SERCON=y' 'CONFIG_HARDWARE_IRQ=y'; do
	require_line "${seabios_config}" "${line}"
done
if [[ ${trace} == 1 ]]; then
	require_line "${coreboot_dir}/.config" '# CONFIG_SEABIOS_STABLE is not set'
	require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_REVISION=y'
	require_line "${seabios_config}" 'CONFIG_DEBUG_USB_TRACE=y'
	require_line "${seabios_config}" '# CONFIG_THREADS is not set'
	if [[ "${variant}" == b06wg ]]; then
		require_line "${seabios_config}" 'CONFIG_DEBUG_LEVEL=8'
	elif [[ "${variant}" == b06wh || "${variant}" == b06wi || \
		"${variant}" == b06wj || "${variant}" == b06wk ]]; then
		require_line "${seabios_config}" 'CONFIG_DEBUG_LEVEL=6'
	else
		require_line "${seabios_config}" 'CONFIG_DEBUG_LEVEL=9'
	fi
else
	require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_STABLE=y'
fi
verify_payloads

grep -aFq -- "${build_id}" "${first_rom}" || { echo "build identity missing" >&2; exit 1; }
cbfs_listing="${temp_dir}/cbfs.txt"
"${coreboot_dir}/build/cbfstool" "${first_rom}" print > "${cbfs_listing}"
for entry in fallback/payload pci10ec,8168.rom etc/sercon-port \
	etc/optionroms-checksum etc/pci-optionrom-exec; do
	grep -Fq "${entry}" "${cbfs_listing}"
done
if [[ ${trace} == 1 ]]; then
	usb_attach="${temp_dir}/usb-time-sigatt.bin"
	"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
		-n etc/usb-time-sigatt -f "${usb_attach}"
	python3 -c 'import pathlib,sys; d=pathlib.Path(sys.argv[1]).read_bytes(); assert len(d)==8 and int.from_bytes(d,"little")==1000' "${usb_attach}"
else
	! grep -Fq 'etc/usb-time-sigatt' "${cbfs_listing}"
fi
if [[ "${variant}" == b06wd || "${variant}" == b06we || \
	"${variant}" == b06wf || "${variant}" == b06wg || \
	"${variant}" == b06wh || "${variant}" == b06wi || \
	"${variant}" == b06wj || "${variant}" == b06wk ]]; then
	ps2_spinup="${temp_dir}/ps2-keyboard-spinup.bin"
	"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
		-n etc/ps2-keyboard-spinup -f "${ps2_spinup}"
	python3 -c 'import pathlib,sys; d=pathlib.Path(sys.argv[1]).read_bytes(); assert len(d)==8 and int.from_bytes(d,"little")==1000' "${ps2_spinup}"
else
	! grep -Fq 'etc/ps2-keyboard-spinup' "${cbfs_listing}"
fi

python3 -m unittest discover -s tests -v

first_ipxe="${temp_dir}/ipxe-first.rom"
cp -- "${coreboot_dir}/payloads/external/iPXE/ipxe/ipxe.rom" "${first_ipxe}"
build_once "${second_rom}"
verify_payloads
cmp -s -- "${first_ipxe}" "${coreboot_dir}/payloads/external/iPXE/ipxe/ipxe.rom" || {
	echo "${variant} iPXE ROM differs between clean builds" >&2; exit 1;
}
cmp -s -- "${first_rom}" "${second_rom}" || {
	echo "${variant} clean builds are not byte-identical" >&2; exit 1;
}

install_exact "${second_rom}" "${public_rom}"
if [[ ${development} == 1 ]]; then
	variant=development-spd10
fi
unpatched="${temp_dir}/msi-x58-pro-e-${variant}-unpatched-4MiB.rom"
deterministic="${temp_dir}/msi-x58-pro-e-${variant}-deterministic-4MiB.rom"
full_chip="${temp_dir}/msi-x58-pro-e-${variant}-deterministic-w25q128-16MiB.rom"
python3 "${repo_root}/scripts/msi_vendor_blobs.py" compose --vendor-rom "${vendor_rom}" \
	--coreboot-rom "${public_rom}" --output-rom "${unpatched}" --include-csi-wrapper-support
python3 "${repo_root}/scripts/msi_vendor_blobs.py" verify-composite --vendor-rom "${vendor_rom}" \
	--image "${unpatched}" --include-csi-wrapper-support
unpatched_hash="$(sha256sum "${unpatched}" | awk '{print $1}')"
python3 "${repo_root}/scripts/x58_b06v9_wrapper_patch.py" patch "${unpatched}" \
	"${deterministic}" --expected-input-sha256 "${unpatched_hash}"
python3 "${repo_root}/scripts/x58_b06v9_wrapper_patch.py" verify "${deterministic}"
deterministic_hash="$(sha256sum "${deterministic}" | awk '{print $1}')"
python3 "${repo_root}/scripts/place_firmware_at_flash_top.py" "${deterministic}" \
	"${full_chip}" --flash-size 0x1000000 --expected-sha256 "${deterministic_hash}"
install_exact "${unpatched}" "${local_dir}/msi-x58-pro-e-${variant}-unpatched-4MiB.rom"
install_exact "${deterministic}" "${local_dir}/msi-x58-pro-e-${variant}-deterministic-4MiB.rom"
install_exact "${full_chip}" "${local_dir}/msi-x58-pro-e-${variant}-deterministic-w25q128-16MiB.rom"
if [[ ${development} == 1 ]]; then
	install_exact "${coreboot_dir}/.config" "${local_dir}/coreboot.config"
	install_exact "${seabios_dir}/.config" "${local_dir}/seabios.config"
	python3 "${repo_root}/scripts/x58_build_manifest.py" \
		--coreboot-root "${coreboot_dir}" --output-dir "${local_dir}" \
		--epoch "${epoch}" --build-id "${build_id}"
fi
sha256sum "${public_rom}" "${local_dir}"/*.rom \
	"${coreboot_dir}/payloads/external/iPXE/ipxe/ipxe.rom"
