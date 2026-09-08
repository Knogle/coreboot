> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VT isolated ICH10 SATA clock-field experiment

## Scope

B06VT is a default-off successor to B06VS. It preserves the complete guarded
RAM/QPI, IOH, PCIe, LAPIC, EHCI, Radeon physical-VBIOS, SeaBIOS and iPXE path.
Its sole new hardware hypothesis is that ICH10R SATA D31:F2 configuration
register `SCLKCG` at offset `0x94` must contain the documented Field-1 value
`0x193` before SeaBIOS is allowed to operate an AHCI controller.

This is deliberately not AHCI-controller initialization. B06VT does not enable
PCI command decode or bus mastering, map or access ABAR, set `GHC.AE`, reset the
HBA, issue COMRESET/OOB, change `PI`, touch a disk, or alter MAP/PCS/ORM.

## Evidence and register model

Intel ICH10 Family Datasheet section 14.1.32 describes `SCLKCG` as a 32-bit
register:

```text
31:30  reserved; must remain zero
29:24  PCD5..PCD0; one disables the corresponding port backbone clock
23:9   reserved
8:0    Field 1; BIOS must program 0x193
```

The vendor-booted reference capture recorded:

```text
D31:F2 MAP     0060
D31:F2 PCS     a23f
D31:F2 SCLKCG  00000193
```

The local MSI AMIBIOS body independently contains the `0x193` target on the
ICH10R `8086:3a16` path. The historical broad coreboot driver also writes
`0x193`, but its separate use of bit 30 as a clock-request control conflicts
with the datasheet's reserved-field description. B06VT never selects bit 30.

## Admission contract

The call occurs at the exit of coreboot `BS_DEV_RESOURCES`, immediately after
B06VS, and therefore after enumeration, BAR sizing, allocation, storage and the
leaf-overlap audit but before normal device enablement.

The inherited B06VS contract must still pass:

- D31:F2 is exactly `8086:3a22`, class/progif `010601`;
- D31:F5 is absent;
- MAP is exactly `0060`;
- PCI I/O, memory and bus-master command bits are clear;
- BAR5 is an exact assigned/stored 2-KiB, 32-bit non-prefetchable resource in
  the existing PCI MMIO aperture, with raw BAR and resource base identical;
- PCS low byte is exactly `3f`, reserved bit 14 is zero, and ORM is only
  observed/preserved.

The new SCLKCG admission set is intentionally only:

```text
00000000  exact reset state
00000193  exact retained target
```

Both states imply `PCD5..0=0`, every reserved field zero and bit 30 zero. Any
partial Field-1 value or any nonzero higher field is rejected. A retained exact
target skips the write.

## Write and readback

The reusable ICH10 helper performs exactly one 32-bit read and, when needed,
one 32-bit write:

```c
value = (value & ~0x000001ff) | 0x00000193;
```

The linked B06VT machine code contains an AND with `0xfffffe00` and an OR with
`0x193`. The board wrapper then requires the complete SCLKCG dword to equal
`00000193`, re-runs the exact inherited identity/resource gate, and verifies
that PCS selected/policy fields did not change. There is no automatic retry or
reset, and a second call in the same ramstage is terminally rejected.

## POST and serial contract

```text
50  B06VT SCLKCG operation begins
52  exact complete-dword and inherited readback passed
53  terminal rejection or readback failure
```

Expected success lines are:

```text
[SATA] B06VT PRE SCLKCG=00000000 ...
[SATA] B06VT POST SCLKCG=00000193 FIELD1=193 PCD=00 ...
[SATA] B06VT SCLKCG=00000193 gate PASS (write=1); PCD=0, reserved=0; PCS/MAP/BAR/CMD and AHCI MMIO untouched
```

On a retained boot, `PRE` may already be `00000193` and success should report
`write=0`. POST `52` proves only the configuration-register contract. It does
not prove a SATA link, AHCI MMIO operation or disk detection.

## First hardware procedure

Use the same fixed E5645, sole SPD `0x54`, HD5450 and RTL8168 configuration as
the successful B06VP run. Capture COM1 at 115200 8N1 before applying power.

1. Validate B06VQ, B06VR and B06VS in order; do not infer their gates from a
   later POST code.
2. Remove SATA data cables for the first B06VT run.
3. Program only the hash-verified 16-MiB W25Q128 image from the manifest.
4. Cold-start once and preserve the complete serial log and POST trace.
5. Accept only `PRE=00000000` or `PRE=00000193`, exact `POST=00000193`, POST
   `52`, and continued SeaBIOS/iPXE execution.
6. If POST `53` appears, do not retry blindly. Restore B06VS or the vendor chip
   and inspect the recorded PRE tuple.

Failure is bounded to a terminal halt before PCI device enablement. Recovery is
the already-proven external programmer/socketed-chip procedure. The build must
not be described as hardware-validated until repeated cold and warm runs have
been logged.

## Deliberately deferred

- ORM retry policy;
- AHCI ABAR decode and bus mastering;
- `GHC.AE`, `CAP`, `PI`, per-port `PxCMD/PxSSTS/PxSERR`;
- HBA reset and COMRESET;
- device presence as an admission criterion;
- SATA disk reads or boot;
- correction or replacement of the broad ICH10 SATA driver.
