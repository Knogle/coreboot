#!/usr/bin/env bash
# Build and locally compose the MSI X58 Pro-E B06VM automatic first-boot image.

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
source "${repo_root}/scripts/x58_layout.sh"
coreboot_dir="${x58_coreboot_dir}"
config="${repo_root}/configs/x58-pro-e-b06vm.config"
public_rom="${repo_root}/builds/experimental/msi-x58-pro-e-b06vm-coreboot-base-4MiB.rom"
local_dir="${repo_root}/blobs-local/msi-x58-pro-e/b06vm"
vendor_rom="${X58_B06VM_VENDOR_ROM:-${repo_root}/blobs-local/msi-x58-pro-e/7522v8F/A7522IMS.8F0}"
jobs="${X58_B06VM_JOBS:-8}"
epoch="${X58_B06VM_SOURCE_DATE_EPOCH:-1788602400}"
ipxe_commit="7c39c04a537ce29dccc6f2bae9749d1d371429c1"
ipxe_sha256="8b21a133a1a795bcd19f7945c23306d37b348fedc8299527e3ce3d66c906ee80"
temp_dir="$(mktemp -d /tmp/x58-b06vm-build.XXXXXX)"

cleanup()
{
	rm -rf -- "${temp_dir}"
}
trap cleanup EXIT

cd -- "${repo_root}"

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

build_once()
{
	local output="$1"

	make -C "${coreboot_dir}" clean
	cp -- "${config}" "${coreboot_dir}/.config"
	env -u DEBUG CCACHE_DISABLE=1 SOURCE_DATE_EPOCH="${epoch}" \
		make -C "${coreboot_dir}" olddefconfig
	env -u DEBUG CCACHE_DISABLE=1 SOURCE_DATE_EPOCH="${epoch}" \
		make -C "${coreboot_dir}" -j"${jobs}"
	cp -- "${coreboot_dir}/build/coreboot.rom" "${output}"
}

if [[ ! -f "${vendor_rom}" ]]; then
	echo "missing user-supplied vendor ROM: ${vendor_rom}" >&2
	exit 1
fi
if [[ ! "${jobs}" =~ ^[1-9][0-9]*$ ]]; then
	echo "X58_B06VM_JOBS must be a positive integer" >&2
	exit 1
fi

first_rom="${temp_dir}/coreboot-first.rom"
second_rom="${temp_dir}/coreboot-second.rom"
build_once "${first_rom}"

require_line "${coreboot_dir}/.config" 'CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_PAYLOAD_SEABIOS=y'
require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_HARDWARE_IRQ=y'
require_line "${coreboot_dir}/.config" 'CONFIG_SEABIOS_ADD_SERCON_PORT_FILE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_PXE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_BUILD_IPXE=y'
require_line "${coreboot_dir}/.config" 'CONFIG_PXE_ROM_ID="10ec,8168"'
require_line "${coreboot_dir}/.config" 'CONFIG_IPXE_HAS_HTTPS=y'
require_line "${coreboot_dir}/.config" 'CONFIG_IPXE_TRUST_CMD=y'
require_line "${coreboot_dir}/.config" '# CONFIG_IPXE_SERIAL_CONSOLE is not set'
require_line "${coreboot_dir}/.config" '# CONFIG_VGA_ROM_RUN is not set'
require_line "${coreboot_dir}/.config" '# CONFIG_PCI_ALLOW_BUS_MASTER_ANY_DEVICE is not set'

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
	'CONFIG_OPTIONROMS=y' \
	'CONFIG_PMM=y' \
	'CONFIG_PCIBIOS=y' \
	'CONFIG_PNPBIOS=y' \
	'CONFIG_KEYBOARD=y' \
	'CONFIG_SERCON=y'; do
	require_line "${seabios_config}" "${line}"
done

ipxe_dir="${coreboot_dir}/payloads/external/iPXE/ipxe"
ipxe_rom="${ipxe_dir}/ipxe.rom"
first_ipxe_rom="${temp_dir}/ipxe-first.rom"
verify_ipxe_rom
cp -- "${ipxe_rom}" "${first_ipxe_rom}"

cbfs_listing="${temp_dir}/cbfs.txt"
"${coreboot_dir}/build/cbfstool" "${first_rom}" print > "${cbfs_listing}"
grep -Fq 'fallback/payload' "${cbfs_listing}"
grep -Fq 'pci10ec,8168.rom' "${cbfs_listing}"
grep -Fq 'etc/sercon-port' "${cbfs_listing}"
grep -Fq 'etc/optionroms-checksum' "${cbfs_listing}"
grep -Fq 'etc/pci-optionrom-exec' "${cbfs_listing}"

optionrom_checksum="${temp_dir}/optionroms-checksum.bin"
"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
	-n etc/optionroms-checksum -f "${optionrom_checksum}"
python3 -c \
	'import pathlib, sys; data = pathlib.Path(sys.argv[1]).read_bytes(); assert len(data) == 8 and int.from_bytes(data, "little") == 0' \
	"${optionrom_checksum}"

pci_optionrom_exec="${temp_dir}/pci-optionrom-exec.bin"
"${coreboot_dir}/build/cbfstool" "${first_rom}" extract \
	-n etc/pci-optionrom-exec -f "${pci_optionrom_exec}"
python3 -c \
	'import pathlib, sys; data = pathlib.Path(sys.argv[1]).read_bytes(); assert len(data) == 8 and int.from_bytes(data, "little") == 1' \
	"${pci_optionrom_exec}"

python3 -m unittest \
	tests.test_x58_b06vi_source_contract \
	tests.test_x58_b06vj_source_contract \
	tests.test_x58_b06vk_source_contract \
	tests.test_x58_b06vl_source_contract \
	tests.test_x58_b06vm_source_contract -v

# A second clean build is part of the release command, not a later informal
# check.  This catches timestamps and state leaked by either payload build.
build_once "${second_rom}"
verify_ipxe_rom
if ! cmp -s -- "${first_ipxe_rom}" "${ipxe_rom}"; then
	echo "B06VM iPXE ROM differs between clean builds" >&2
	exit 1
fi
if ! cmp -s -- "${first_rom}" "${second_rom}"; then
	echo "B06VM clean builds are not byte-identical" >&2
	exit 1
fi

install_exact "${second_rom}" "${public_rom}"

unpatched="${temp_dir}/msi-x58-pro-e-b06vm-unpatched-4MiB.rom"
deterministic="${temp_dir}/msi-x58-pro-e-b06vm-deterministic-4MiB.rom"
full_chip="${temp_dir}/msi-x58-pro-e-b06vm-deterministic-w25q128-16MiB.rom"

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
	"${local_dir}/msi-x58-pro-e-b06vm-unpatched-4MiB.rom"
install_exact "${deterministic}" \
	"${local_dir}/msi-x58-pro-e-b06vm-deterministic-4MiB.rom"
install_exact "${full_chip}" \
	"${local_dir}/msi-x58-pro-e-b06vm-deterministic-w25q128-16MiB.rom"

sha256sum \
	"${public_rom}" \
	"${local_dir}/msi-x58-pro-e-b06vm-unpatched-4MiB.rom" \
	"${local_dir}/msi-x58-pro-e-b06vm-deterministic-4MiB.rom" \
	"${local_dir}/msi-x58-pro-e-b06vm-deterministic-w25q128-16MiB.rom" \
	"${ipxe_rom}"
