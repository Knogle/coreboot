#!/usr/bin/env bash
# Build the B06WK native ACPI resource/fixed-button repair experiment.

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
exec "${repo_root}/scripts/build_x58_b06_sata_successor.sh" b06wk
