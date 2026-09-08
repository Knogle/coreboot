#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Explicit local firmware build, never flash or contact hardware.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
case "${1:-}" in
	-h|--help)
		echo "usage: X58_VENDOR_ROM=/path/to/A7522IMS.8F0 $0"
		echo "First explicitly prepare public payload sources with prepare_x58_payloads.sh."
		echo "Builds DEVELOPMENT-SPD10 twice; never represents archived WK or flashes hardware."
		exit 0 ;;
	"") ;;
	*) echo "unexpected argument: $1" >&2; exit 2 ;;
esac
[[ $# -eq 0 ]] || { echo "unexpected arguments" >&2; exit 2; }
exec "${script_dir}/build_x58_b06_sata_successor.sh" development
