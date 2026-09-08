#!/usr/bin/env bash
# Build and locally compose the MSI X58 Pro-E B06VK Q-canonical experiment.

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
source "${repo_root}/scripts/x58_layout.sh"
coreboot_dir="${x58_coreboot_dir}"
config="${repo_root}/configs/x58-pro-e-b06vk.config"
public_rom="${repo_root}/builds/experimental/msi-x58-pro-e-b06vk-coreboot-base-4MiB.rom"
local_dir="${repo_root}/blobs-local/msi-x58-pro-e/b06vk"
vendor_rom="${X58_B06VK_VENDOR_ROM:-${repo_root}/blobs-local/msi-x58-pro-e/7522v8F/A7522IMS.8F0}"
jobs="${X58_B06VK_JOBS:-8}"
epoch="${X58_B06VK_SOURCE_DATE_EPOCH:-1788602400}"
temp_dir="$(mktemp -d /tmp/x58-b06vk-build.XXXXXX)"

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

if [[ ! -f "${vendor_rom}" ]]; then
	echo "missing user-supplied vendor ROM: ${vendor_rom}" >&2
	exit 1
fi
if [[ ! "${jobs}" =~ ^[1-9][0-9]*$ ]]; then
	echo "X58_B06VK_JOBS must be a positive integer" >&2
	exit 1
fi

make -C "${coreboot_dir}" clean
cp -- "${config}" "${coreboot_dir}/.config"
env CCACHE_DISABLE=1 SOURCE_DATE_EPOCH="${epoch}" \
	make -C "${coreboot_dir}" olddefconfig
env CCACHE_DISABLE=1 SOURCE_DATE_EPOCH="${epoch}" \
	make -C "${coreboot_dir}" -j"${jobs}"

python3 -m unittest \
	tests.test_x58_b06vi_source_contract \
	tests.test_x58_b06vj_source_contract \
	tests.test_x58_b06vk_source_contract -v

install_exact "${coreboot_dir}/build/coreboot.rom" "${public_rom}"

unpatched="${temp_dir}/msi-x58-pro-e-b06vk-unpatched-4MiB.rom"
deterministic="${temp_dir}/msi-x58-pro-e-b06vk-deterministic-4MiB.rom"
full_chip="${temp_dir}/msi-x58-pro-e-b06vk-deterministic-w25q128-16MiB.rom"

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
	"${local_dir}/msi-x58-pro-e-b06vk-unpatched-4MiB.rom"
install_exact "${deterministic}" \
	"${local_dir}/msi-x58-pro-e-b06vk-deterministic-4MiB.rom"
install_exact "${full_chip}" \
	"${local_dir}/msi-x58-pro-e-b06vk-deterministic-w25q128-16MiB.rom"

sha256sum \
	"${public_rom}" \
	"${local_dir}/msi-x58-pro-e-b06vk-unpatched-4MiB.rom" \
	"${local_dir}/msi-x58-pro-e-b06vk-deterministic-4MiB.rom" \
	"${local_dir}/msi-x58-pro-e-b06vk-deterministic-w25q128-16MiB.rom"
