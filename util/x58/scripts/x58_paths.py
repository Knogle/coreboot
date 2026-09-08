"""Local source-tree lookup; no downloads, filesystem writes or target access."""

from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]


def coreboot_root():
    for candidate in (TOOLS_ROOT.parent.parent, TOOLS_ROOT / "coreboot"):
        if ((candidate / "src/mainboard/msi/x58_pro_e/Kconfig").is_file()
                and (candidate / "Makefile").is_file()):
            return candidate.resolve()
    raise RuntimeError("cannot locate MSI X58 coreboot source tree")
