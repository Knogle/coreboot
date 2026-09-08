#!/usr/bin/env bash
# Build the B06WG automatic GPIO57 USB/SeaBIOS successor.

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
exec "${repo_root}/scripts/build_x58_b06_sata_successor.sh" b06wg
