"""Resolve real source paths in both the public fork and research workspace."""

from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[1]


def coreboot_root(tools_root=TOOLS_ROOT):
    for candidate in (tools_root.parent.parent, tools_root / "coreboot"):
        if ((candidate / "src/mainboard/msi/x58_pro_e/Kconfig").is_file()
                and (candidate / "Makefile").is_file()):
            return candidate.resolve()
    raise RuntimeError(f"cannot locate MSI X58 coreboot sources from {tools_root}")


COREBOOT_ROOT = coreboot_root()
