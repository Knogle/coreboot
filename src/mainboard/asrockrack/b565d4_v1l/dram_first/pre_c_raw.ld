/* SPDX-License-Identifier: GPL-2.0-only */
/* Additional assertions for the ordinary coreboot bootblock linker layout. */
ASSERT(_earlyram_stack == 0x09c00000 && _eearlyram_stack == 0x09c01000,
       "C7 raw runtime requires the retained PSP-window stack")
ASSERT(_data >= 0x09fe0000 && _data <= _edata && _edata <= _bss &&
       _bss <= _ebss && _ebss <= _bootblock,
       "C7 raw runtime data/BSS overlaps code or escapes mutable reservation")
ASSERT(_edata - _data <= 0x10000 && _ebss - _bss <= 0x10000,
       "C7 raw runtime data/BSS exceeds finite initialization bounds")
ASSERT(_data_load >= _bootblock && _data_load + (_edata - _data) <= _ebootblock,
       "C7 raw initialized-data source escapes bootblock")
ASSERT(_ebootblock == 0x0a000000, "C7 raw runtime PSP reset end changed")
