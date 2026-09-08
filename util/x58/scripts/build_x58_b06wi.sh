#!/usr/bin/env bash
# Build the B06WI exact-gated vendor-correlated IRQ/ACPI successor.

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
exec "${repo_root}/scripts/build_x58_b06_sata_successor.sh" b06wi
