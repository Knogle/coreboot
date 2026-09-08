#!/usr/bin/env bash
# Build a standalone BIOS-PXE diagnostic in RAM, never a flash image.
set -euo pipefail

usage() {
	printf '%s\n' \
		"Usage: bash $0 --mkstandalone PATH --modules-dir DIR [options]" \
		"  --dsdt FILE       released B06WJ dsdt-from-rom.aml (hash pinned)" \
		"  --config FILE     this lab's grub.cfg" \
		"  --candidates-dir DIR  optionally embed the three hash-pinned native AML variants" \
		"  --legacy-windows FILE  separately embed the hash-pinned two-window native AML" \
		"  --ioh-prt FILE    separately embed the hash-pinned IOH PRT-only native AML" \
		"  --output-dir DIR  a NEW directory; default mktemp under /tmp" \
		"No remote access, firmware build, flash operation or target boot."
}

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "$script_dir/.." && pwd)
mkstandalone=
modules_dir=
dsdt="$repo_dir/builds/experimental/b06wj-release-20260907/dsdt-from-rom.aml"
config="$repo_dir/research/msi/acpi-ramlab/grub.cfg"
candidates_dir=
legacy_windows=
ioh_prt=
output_dir=
while (($#)); do
	case "$1" in
	--help|-h) usage; exit 0 ;;
	--mkstandalone|--modules-dir|--dsdt|--config|--candidates-dir|--legacy-windows|--ioh-prt|--output-dir)
		if (($# < 2)); then usage >&2; exit 2; fi
		case "$1" in
		--mkstandalone) mkstandalone=$2 ;;
		--modules-dir) modules_dir=$2 ;;
		--dsdt) dsdt=$2 ;;
		--config) config=$2 ;;
		--candidates-dir) candidates_dir=$2 ;;
		--legacy-windows) legacy_windows=$2 ;;
		--ioh-prt) ioh_prt=$2 ;;
		--output-dir) output_dir=$2 ;;
		esac
		shift 2 ;;
	*) usage >&2; exit 2 ;;
	esac
done

die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
[[ -n "$mkstandalone" && -n "$modules_dir" ]] || { usage >&2; exit 2; }
[[ -x "$mkstandalone" ]] || die "mkstandalone is not executable: $mkstandalone"
[[ -d "$modules_dir" ]] || die "missing BIOS i386-pc module directory: $modules_dir"
[[ -f "$modules_dir/kernel.img" && -f "$modules_dir/moddep.lst" ]] ||
	die 'module directory must contain kernel.img and moddep.lst'
[[ -f "$config" && -f "$dsdt" ]] || die 'config or released DSDT is absent'

expected_dsdt=63ba8a0d497cc6998bfffd35ea816d7bba5d85f811337ce0bfb21f7c854d7d03
actual_dsdt=$(sha256sum -- "$dsdt")
actual_dsdt=${actual_dsdt%% *}
[[ "$actual_dsdt" == "$expected_dsdt" ]] || die 'not the unchanged released B06WJ DSDT'

# Optional experiment files only; no variant is selected by grub.cfg.
# Pins: builds/experimental/acpi-ramlab/b06wj-20260908/manifest.json.
# Ignore all other directory entries, and reject a missing/mutated candidate
# before creating output or invoking the GRUB builder.
candidate_files=()
candidate_grafts=()
if [[ -n "$candidates_dir" ]]; then
	[[ -d "$candidates_dir" ]] || die "missing candidates directory: $candidates_dir"
	for variant in vga-only s0-s5-only s0-only; do
		case "$variant" in
		vga-only) expected_candidate=328181a9170b2432a56214aff05eb435cab3dadff2d2724145b8800cc76b55d1 ;;
		s0-s5-only) expected_candidate=f1886bfca9565ec47e6448e4f828b4484fc53c8121d037a605cfe44c640c0db9 ;;
		s0-only) expected_candidate=72a607e94006502ae1340b78bf77634ef691f2f1e7f570cbc0eb6c346bed5066 ;;
		esac
		candidate="$candidates_dir/$variant/dsdt.aml"
		[[ -f "$candidate" ]] || die "missing candidate: $variant"
		actual_candidate=$(sha256sum -- "$candidate")
		actual_candidate=${actual_candidate%% *}
		[[ "$actual_candidate" == "$expected_candidate" ]] ||
			die "candidate hash mismatch: $variant"
		candidate_files+=("$candidate")
		candidate_grafts+=("boot/grub/dsdt-$variant.aml=$candidate")
	done
fi

# Independent opt-in: preserve the original --candidates-dir set and pins.
# Pin: acpi-ramlab/b06wj-legacy-windows-20260908/manifest.json.
if [[ -n "$legacy_windows" ]]; then
	[[ -f "$legacy_windows" ]] || die 'missing legacy-windows candidate'
	expected_legacy_windows=d0808b3049696dcf329282f54eb1d4f438bb0d207c3df5377d53ff4db53890d2
	actual_legacy_windows=$(sha256sum -- "$legacy_windows")
	actual_legacy_windows=${actual_legacy_windows%% *}
	[[ "$actual_legacy_windows" == "$expected_legacy_windows" ]] ||
		die 'candidate hash mismatch: legacy-windows'
	candidate_files+=("$legacy_windows")
	candidate_grafts+=("boot/grub/dsdt-legacy-windows.aml=$legacy_windows")
fi

# Independent PRT-only experiment; no combination with other AML deltas.
# Pin: acpi-ramlab/b06wj-ioh-prt-20260908/manifest.json.
if [[ -n "$ioh_prt" ]]; then
	[[ -f "$ioh_prt" ]] || die 'missing IOH PRT candidate'
	expected_ioh_prt=ebb760c9558dca096b8b1bd21820416c1b04020375afd7512da6d3b421ea8bbf
	actual_ioh_prt=$(sha256sum -- "$ioh_prt")
	actual_ioh_prt=${actual_ioh_prt%% *}
	[[ "$actual_ioh_prt" == "$expected_ioh_prt" ]] ||
		die 'candidate hash mismatch: ioh-prt'
	candidate_files+=("$ioh_prt")
	candidate_grafts+=("boot/grub/dsdt-ioh-prt.aml=$ioh_prt")
fi

# BIOS disk access deliberately uses SeaBIOS rather than new AHCI/USB drivers.
# No network modules are needed: config and control AML are in the memdisk.
# The interactive write-capable commands do NOT run from the default config.
modules='normal serial terminal echo ls lsacpi acpi lsmmap hexdump memrw iorw lspci setpci biosdisk part_msdos part_gpt fat ntfs chain search search_fs_uuid probe cat configfile'
for module in $modules; do
	[[ -f "$modules_dir/$module.mod" ]] || die "required module missing: $module.mod"
done

if [[ -n "$output_dir" ]]; then
	[[ ! -e "$output_dir" ]] || die 'output directory already exists; refusing overwrite'
	mkdir -- "$output_dir"
else
	output_dir=$(mktemp -d /tmp/x58-acpi-ramlab.XXXXXX)
fi
output_dir=$(cd -- "$output_dir" && pwd)
image="$output_dir/x58-acpi-ramlab-01.pxe"

# Pin available timestamp/locale controls.  Record and compare two actual
# outputs before asserting byte reproducibility for a particular GRUB build.
export SOURCE_DATE_EPOCH=1788825600
export LC_ALL=C TZ=UTC
cmd=("$mkstandalone" -O i386-pc-pxe -d "$modules_dir"
	--install-modules="$modules" --modules="$modules"
	--locales= --fonts= --themes= --compress=no
	-o "$image"
	"boot/grub/grub.cfg=$config"
	"boot/grub/dsdt-wj.aml=$dsdt"
	"${candidate_grafts[@]}")
{
	printf 'X58-ACPI-RAMLAB-01 build inputs\n'
	"$mkstandalone" --version
	printf 'SOURCE_DATE_EPOCH=%s\n' "$SOURCE_DATE_EPOCH"
	printf 'Command:'
	printf ' %q' "${cmd[@]}"
	printf '\n'
	sha256sum -- "$config" "$dsdt" "$modules_dir/kernel.img" "$modules_dir/moddep.lst"
	for candidate in "${candidate_files[@]}"; do sha256sum -- "$candidate"; done
	for module in $modules; do sha256sum -- "$modules_dir/$module.mod"; done
} > "$output_dir/build-inputs.txt"

"${cmd[@]}" > "$output_dir/build.log" 2>&1 || {
	printf 'Build failed; retained log: %s\n' "$output_dir/build.log" >&2
	exit 1
}
[[ -s "$image" ]] || die 'GRUB did not produce a nonempty image'
sha256sum -- "$image" > "$output_dir/image.sha256"
printf 'Built network-only diagnostic: %s\n' "$image"
printf 'Inputs/log/hash retained in: %s\n' "$output_dir"
printf 'No firmware or remote system changed. Hardware execution not performed.\n'
