#!/usr/bin/env python3
"""Read the exact MSI runtime-patched BIOS NVS region, never execute AML."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
if a.output.exists():
    p.error('output already exists')
tables = Path('/sys/firmware/acpi/tables')
dsdt = (tables / 'DSDT').read_bytes()
fadt = (tables / 'FACP').read_bytes()
if dsdt[:4] != b'DSDT' or sum(dsdt) % 256 or sum(fadt) % 256:
    p.error('table signature/checksum mismatch')
if dsdt[16:24] != b'A7522800':
    p.error('not the measured MSI DSDT')
# Exact AML encoding observed in the runtime disassembly, not an AML interpreter.
prefix = bytes.fromhex('5b8042494f53000c')
if dsdt.count(prefix) != 1:
    p.error('expected one DWORD-address BIOS SystemMemory OperationRegion')
offset = dsdt.index(prefix) + len(prefix)
base = struct.unpack_from('<I', dsdt, offset)[0]
if dsdt[offset+4:offset+6] != bytes.fromhex('0aff'):
    p.error('BIOS region length is not the measured 255-byte encoding')
facs = struct.unpack_from('<I', fadt, 36)[0]
if base != facs + 0x64 or not (0x100000 <= base < 0xc0000000):
    p.error('runtime BIOS region is not the measured FACS+0x64 NVS layout')
iomem = Path('/proc/iomem').read_text()
admitted = False
for line in iomem.splitlines():
    if line.strip().endswith(': ACPI Non-volatile Storage'):
        lo, hi = line.split(':', 1)[0].strip().split('-')
        if int(lo, 16) <= base and base + 254 <= int(hi, 16):
            admitted = True
if not admitted:
    p.error('BIOS region is not covered by kernel-reported ACPI NVS')
fd = os.open('/dev/mem', os.O_RDONLY | os.O_CLOEXEC | os.O_SYNC)
try:
    data = os.pread(fd, 255, base)
finally:
    os.close(fd)
if len(data) != 255:
    p.error('short NVS read')
a.output.mkdir(mode=0o700, parents=True)
(a.output / 'bios-nvs-ff.bin').write_bytes(data)
for name, argv in [('dmesg.txt', ['dmesg']), ('grub-version.txt', ['grub-mkimage', '--version'])]:
    result = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=10, check=False)
    (a.output / name).write_bytes(result.stdout)
metadata = {'capture_unix_ns': time.time_ns(), 'hardware_access': 'read-only',
            'bios_nvs_base': hex(base), 'bytes': len(data), 'facs_base': hex(facs),
            'dsdt_sha256': hashlib.sha256(dsdt).hexdigest(),
            'artifacts': {f.name: hashlib.sha256(f.read_bytes()).hexdigest()
                          for f in sorted(a.output.iterdir()) if f.is_file()}}
(a.output / 'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
print(json.dumps(metadata, indent=2))
