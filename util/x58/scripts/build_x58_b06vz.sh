#!/usr/bin/env bash
set -euo pipefail
exec "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/build_x58_b06_sata_successor.sh" b06vz
