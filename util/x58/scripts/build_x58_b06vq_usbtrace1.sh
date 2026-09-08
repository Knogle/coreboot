#!/usr/bin/env bash
# Build the isolated MSI X58 Pro-E B06VQ SeaBIOS USB-trace image.

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
source "${repo_root}/scripts/x58_layout.sh"
coreboot_dir="${x58_coreboot_dir}"
config="${repo_root}/configs/x58-pro-e-b06vq-usbtrace1.config"
public_rom="${repo_root}/builds/experimental/msi-x58-pro-e-b06vq-usbtrace1-coreboot-base-4MiB.rom"
local_dir="${repo_root}/blobs-local/msi-x58-pro-e/b06vq-usbtrace1"
vendor_rom="${X58_B06VQ_USBTRACE1_VENDOR_ROM:-${repo_root}/blobs-local/msi-x58-pro-e/7522v8F/A7522IMS.8F0}"
jobs="${X58_B06VQ_USBTRACE1_JOBS:-8}"
epoch="${X58_B06VQ_USBTRACE1_SOURCE_DATE_EPOCH:-1788688800}"
seabios_base="b52ca86e094d19b58e2304417787e96b940e39c6"
seabios_trace="5497f43189374647b3b0f282aa497e71c891f3c5"
seabios_patch="${repo_root}/patches/seabios-b52ca86-usb-deferred-trace.patch"
seabios_patch_sha256="f222796ea8029444106ca009045b1528b9a325fa5b84706ca252739d353c232f"
seabios_dir="${coreboot_dir}/payloads/external/SeaBIOS/seabios"
ipxe_commit="7c39c04a537ce29dccc6f2bae9749d1d371429c1"
ipxe_sha256="8b21a133a1a795bcd19f7945c23306d37b348fedc8299527e3ce3d66c906ee80"
temp_dir="$(mktemp -d /tmp/x58-b06vq-usbtrace1-build.XXXXXX)"

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

	if ! grep -Fqx -- "${line}" "${file}"; then
		echo "required configuration missing from ${file}: ${line}" >&2
		return 1
	fi
}

prepare_seabios_trace_revision()
{
	local trace_clone="${temp_dir}/seabios-trace"
	local created

	if [[ "$(sha256sum "${seabios_patch}" | awk '{print $1}')" != \
	      "${seabios_patch_sha256}" ]]; then
		echo "unexpected deferred USB trace patch hash" >&2
		return 1
	fi
	if [[ -n "$(git -C "${seabios_dir}" status --porcelain --untracked-files=no)" ]]; then
		echo "refusing to replace tracked changes in the SeaBIOS checkout" >&2
		return 1
	fi
	if [[ "$(git -C "${seabios_dir}" rev-parse "${seabios_base}^{commit}")" != \
	      "${seabios_base}" ]]; then
		echo "missing exact SeaBIOS base revision" >&2
		return 1
	fi

	git clone --quiet --no-hardlinks "${seabios_dir}" "${trace_clone}"
	git -C "${trace_clone}" checkout --quiet --detach "${seabios_base}"
	git -C "${trace_clone}" apply --check "${seabios_patch}"
	git -C "${trace_clone}" apply "${seabios_patch}"
	git -C "${trace_clone}" diff --check
	git -C "${trace_clone}" add \
		src/Kconfig src/hw/usb.h src/hw/usb.c src/hw/usb-ehci.c \
		src/hw/usb-uhci.c src/hw/usb-hid.c
	env GIT_AUTHOR_DATE=2026-09-06T00:00:00Z \
		GIT_COMMITTER_DATE=2026-09-06T00:00:00Z \
		git -C "${trace_clone}" \
		-c user.name='X58 bring-up' \
		-c user.email='x58-local@example.invalid' \
		commit --quiet -m 'usb: add deferred bring-up trace'
	created="$(git -C "${trace_clone}" rev-parse HEAD)"
	if [[ "${created}" != "${seabios_trace}" ]]; then
		echo "non-deterministic SeaBIOS trace commit: ${created}" >&2
		return 1
	fi

	# The coreboot wrapper can now check out the exact local commit without a
	# network fetch.  No branch or tracked source in the working clone changes.
	git -C "${seabios_dir}" fetch --quiet "${trace_clone}" "${seabios_trace}"
	git -C "${seabios_dir}" cat-file -e "${seabios_trace}^{commit}"
}

verify_ipxe_rom()
{
	local ipxe_dir="${coreboot_dir}/payloads/external/iPXE/ipxe"
	local ipxe_rom="${ipxe_dir}/ipxe.rom"

	if [[ "$(git -C "${ipxe_dir}" rev-parse HEAD)" != "${ipxe_commit}" ]]; then
		echo "unexpected iPXE revision" >&2
		return 1
	fi
	if [[ "$(sha256sum "${ipxe_rom}" | awk '{print $1}')" != "${ipxe_sha256}" ]]; then
		echo "unexpected iPXE ROM hash" >&2
		return 1
	fi
	if [[ "$(stat -c '%s' "${ipxe_rom}")" != 95232 ]]; then
		echo "unexpected iPXE ROM size" >&2
		return 1
	fi
	python3 -c \
		'import pathlib, struct, sys; data = pathlib.Path(sys.argv[1]).read_bytes(); assert data[:2] == b"\x55\xaa"; off = struct.unpack_from("<H", data, 0x18)[0]; assert data[off:off + 4] == b"PCIR"; assert struct.unpack_from("<HH", data, off + 4) == (0x10ec, 0x8168); assert sum(data) & 0xff == 0' \
		"${ipxe_rom}"
}

verify_prompted_ipxe_source()
{
	local general="${coreboot_dir}/payloads/external/iPXE/ipxe/src/config/general.h"

	grep -Eq '^#define[[:space:]]+BANNER_TIMEOUT[[:space:]]+20$' "${general}"
	grep -Eq '^#define[[:space:]]+ROM_BANNER_TIMEOUT[[:space:]]+\([[:space:]]*2 \* BANNER_TIMEOUT[[:space:]]*\)$' "${general}"
}

build_once()
{
	local output="$1"
	local cache_dir="${temp_dir}/cache"

	mkdir -p -- "${cache_dir}/ccache"

	make -C "${coreboot_dir}" clean
	cp -- "${config}" "${coreboot_dir}/.config"
	env -u DEBUG CCACHE_DISABLE=1 CCACHE_DIR="${cache_dir}/ccache" \
		XDG_CACHE_HOME="${cache_dir}" SOURCE_DATE_EPOCH="${epoch}" \
		make -C "${coreboot_dir}" olddefconfig
	env -u DEBUG CCACHE_DISABLE=1 CCACHE_DIR="${cache_dir}/ccache" \
		XDG_CACHE_HOME="${cache_dir}" SOURCE_DATE_EPOCH="${epoch}" \
		make -C "${coreboot_dir}" -j"${jobs}"
	cp -- "${coreboot_dir}/build/coreboot.rom" "${output}"
}

if [[ ! -f "${vendor_rom}" ]]; then
	echo "missing user-supplied vendor ROM: ${vendor_rom}" >&2
	exit 1
fi
if [[ ! "${jobs}" =~ ^[1-9][0-9]*$ ]]; then
	echo "X58_B06VQ_USBTRACE1_JOBS must be a positive integer" >&2
	exit 1
fi

prepare_seabios_trace_revision

first_rom="${temp_dir}/coreboot-first.rom"
second_rom="${temp_dir}/coreboot-second.rom"
build_once "${first_rom}"

require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS=y'
require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT=y'
require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT=y'
require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06VQ_USB_TRACE1=y'
require_line "${coreboot_dir}/.config" '# CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP is not set'
require_line "${coreboot_dir}/.config" 'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_EHCI_INIT=y'
require_line "${coreboot_dir}/.config" 'CONFIG_PAYLOAD_SEABIOS=y'
require_line "${coreboot_dir}/.config" '# CONFIG_SEABIOS_STABLE is not set'
require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_REVISION=y'
require_line "${coreboot_dir}/.config" \
	'CONFIG_SEABIOS_REVISION_ID="5497f43189374647b3b0f282aa497e71c891f3c5"'
require_line "${coreboot_dir}/.config" \
	'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/$(MAINBOARDDIR)/config_seabios_b06vq_usbtrace1"'
require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_HARDWARE_IRQ=y'
require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_ADD_SERCON_PORT_FILE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_PXE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_BUILD_IPXE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_PXE_ROM_ID="10ec,8168"'
require_line "${coreboot_dir}/.config" 'CONFIG_IPXE_HAS_HTTPS=y'
require_line "${coreboot_dir}/.config" 'CONFIG_IPXE_TRUST_CMD=y'
require_line "${coreboot_dir}/.config" '# CONFIG_IPXE_SERIAL_CONSOLE is not set'
require_line "${coreboot_dir}/.config" '# CONFIG_IPXE_NO_PROMPT is not set'
require_line "${coreboot_dir}/.config" '# CONFIG_VGA_ROM_RUN is not set'
require_line "${coreboot_dir}/.config" '# CONFIG_VGA_BIOS is not set'
require_line "${coreboot_dir}/.config" 'CONFIG_NO_GFX_INIT=y'
require_line "${coreboot_dir}/.config" '# CONFIG_PCI_ALLOW_BUS_MASTER_ANY_DEVICE is not set'
require_line "${coreboot_dir}/.config" 'CONFIG_XAPIC_ONLY=y'

auto_config="${coreboot_dir}/build/auto.conf"
require_line "${auto_config}" 'CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n'
require_line "${auto_config}" 'CONFIG_SMP=n'
require_line "${auto_config}" 'CONFIG_NO_PCAT_8259=n'
require_line "${auto_config}" 'CONFIG_XAPIC_ONLY=y'
require_line "${auto_config}" 'CONFIG_SEABIOS_HARDWARE_IRQ=y'
require_line "${auto_config}" 'CONFIG_IPXE_NO_PROMPT=n'
require_line "${auto_config}" 'CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP=n'

seabios_config="${coreboot_dir}/payloads/external/SeaBIOS/seabios/.config"
for line in \
	'CONFIG_BOOT=y' \
	'CONFIG_BOOTMENU=y' \
	'CONFIG_DRIVES=y' \
	'CONFIG_ATA=y' \
	'CONFIG_AHCI=y' \
	'CONFIG_USB=y' \
	'CONFIG_USB_UHCI=y' \
	'CONFIG_USB_EHCI=y' \
	'CONFIG_USB_MSC=y' \
	'CONFIG_USB_HUB=y' \
	'CONFIG_USB_KEYBOARD=y' \
	'CONFIG_DEBUG_USB_TRACE=y' \
	'# CONFIG_THREADS is not set' \
	'CONFIG_OPTIONROMS=y' \
	'CONFIG_PMM=y' \
	'CONFIG_PCIBIOS=y' \
	'CONFIG_PNPBIOS=y' \
	'CONFIG_KEYBOARD=y' \
	'CONFIG_SERCON=y' \
	'CONFIG_HARDWARE_IRQ=y'; do
	require_line "${seabios_config}" "${line}"
done
if [[ "$(git -C "${seabios_dir}" rev-parse HEAD)" != "${seabios_trace}" ]]; then
	echo "SeaBIOS did not build from the pinned trace revision" >&2
	exit 1
fi
if [[ -n "$(git -C "${seabios_dir}" status --porcelain --untracked-files=no)" ]]; then
	echo "SeaBIOS trace checkout is unexpectedly dirty" >&2
	exit 1
fi

ipxe_dir="${coreboot_dir}/payloads/external/iPXE/ipxe"
ipxe_rom="${ipxe_dir}/ipxe.rom"
first_ipxe_rom="${temp_dir}/ipxe-first.rom"
verify_prompted_ipxe_source
verify_ipxe_rom
cp -- "${ipxe_rom}" "${first_ipxe_rom}"

cbfs_listing="${temp_dir}/cbfs.txt"
"${coreboot_dir}/build/cbfstool" "${first_rom}" print > "${cbfs_listing}"
grep -Fq 'fallback/payload' "${cbfs_listing}"
grep -Fq 'pci10ec,8168.rom' "${cbfs_listing}"
grep -Fq 'etc/sercon-port' "${cbfs_listing}"
grep -Fq 'etc/optionroms-checksum' "${cbfs_listing}"
grep -Fq 'etc/pci-optionrom-exec' "${cbfs_listing}"
if grep -Eq 'pci1002,[[:xdigit:]]{4}\.rom' "${cbfs_listing}"; then
	echo "B06VQ-USBTRACE1 must not embed an AMD VGA option ROM" >&2
	exit 1
fi

usb_attach_time="${temp_dir}/usb-time-sigatt.bin"
"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
	-n etc/usb-time-sigatt -f "${usb_attach_time}"
python3 -c \
	'import pathlib, sys; data = pathlib.Path(sys.argv[1]).read_bytes(); assert len(data) == 8 and int.from_bytes(data, "little") == 1000' \
	"${usb_attach_time}"

payload_config="${temp_dir}/payload-config"
"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
	-n payload_config -f "${payload_config}"
require_line "${payload_config}" 'CONFIG_DEBUG_USB_TRACE=y'
require_line "${payload_config}" '# CONFIG_THREADS is not set'

payload_revision="${temp_dir}/payload-revision"
"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
	-n payload_revision -f "${payload_revision}"
grep -Fq '5497f431' "${payload_revision}"

optionrom_checksum="${temp_dir}/optionroms-checksum.bin"
"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
	-n etc/optionroms-checksum -f "${optionrom_checksum}"
python3 -c \
	'import pathlib, sys; data = pathlib.Path(sys.argv[1]).read_bytes(); assert len(data) == 8 and int.from_bytes(data, "little") == 1' \
	"${optionrom_checksum}"

pci_optionrom_exec="${temp_dir}/pci-optionrom-exec.bin"
"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
	-n etc/pci-optionrom-exec -f "${pci_optionrom_exec}"
python3 -c \
	'import pathlib, sys; data = pathlib.Path(sys.argv[1]).read_bytes(); assert len(data) == 8 and int.from_bytes(data, "little") == 1' \
	"${pci_optionrom_exec}"

python3 -m unittest discover -s tests -v

# A second clean build is part of the release command, not a later informal
# check.  This catches timestamps and state leaked by either payload build.
build_once "${second_rom}"
verify_prompted_ipxe_source
verify_ipxe_rom
if ! cmp -s -- "${first_ipxe_rom}" "${ipxe_rom}"; then
	echo "B06VQ-USBTRACE1 iPXE ROM differs between clean builds" >&2
	exit 1
fi
if ! cmp -s -- "${first_rom}" "${second_rom}"; then
	echo "B06VQ-USBTRACE1 clean builds are not byte-identical" >&2
	exit 1
fi

install_exact "${second_rom}" "${public_rom}"

unpatched="${temp_dir}/msi-x58-pro-e-b06vq-usbtrace1-unpatched-4MiB.rom"
deterministic="${temp_dir}/msi-x58-pro-e-b06vq-usbtrace1-deterministic-4MiB.rom"
full_chip="${temp_dir}/msi-x58-pro-e-b06vq-usbtrace1-deterministic-w25q128-16MiB.rom"

python3 "${repo_root}/scripts/msi_vendor_blobs.py" compose \
	--vendor-rom "${vendor_rom}" \
	--coreboot-rom "${public_rom}" \
	--output-rom "${unpatched}" \
	--include-csi-wrapper-support
python3 "${repo_root}/scripts/msi_vendor_blobs.py" verify-composite \
	--vendor-rom "${vendor_rom}" \
	--image "${unpatched}" \
	--include-csi-wrapper-support

unpatched_hash="$(sha256sum "${unpatched}" | awk '{print $1}')"
python3 "${repo_root}/scripts/x58_b06v9_wrapper_patch.py" patch \
	"${unpatched}" "${deterministic}" \
	--expected-input-sha256 "${unpatched_hash}"
python3 "${repo_root}/scripts/x58_b06v9_wrapper_patch.py" verify \
	"${deterministic}"

deterministic_hash="$(sha256sum "${deterministic}" | awk '{print $1}')"
python3 "${repo_root}/scripts/place_firmware_at_flash_top.py" \
	"${deterministic}" "${full_chip}" \
	--flash-size 0x1000000 \
	--expected-sha256 "${deterministic_hash}"

install_exact "${unpatched}" \
	"${local_dir}/msi-x58-pro-e-b06vq-usbtrace1-unpatched-4MiB.rom"
install_exact "${deterministic}" \
	"${local_dir}/msi-x58-pro-e-b06vq-usbtrace1-deterministic-4MiB.rom"
install_exact "${full_chip}" \
	"${local_dir}/msi-x58-pro-e-b06vq-usbtrace1-deterministic-w25q128-16MiB.rom"

sha256sum \
	"${public_rom}" \
	"${local_dir}/msi-x58-pro-e-b06vq-usbtrace1-unpatched-4MiB.rom" \
	"${local_dir}/msi-x58-pro-e-b06vq-usbtrace1-deterministic-4MiB.rom" \
	"${local_dir}/msi-x58-pro-e-b06vq-usbtrace1-deterministic-w25q128-16MiB.rom" \
	"${ipxe_rom}"
