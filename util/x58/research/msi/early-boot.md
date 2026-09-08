> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI X58 Pro-E early-boot evidence

Status: **verified-static** unless marked otherwise.  Addresses below refer to
the 4 MiB full ROM with SHA-256
`ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8`.
No hardware execution is claimed.

## Full-ROM identity and reset vector

The file is 4,194,304 bytes.  The user reports that it was compared against
the complete EEPROM contents and is byte-identical.  Its first bytes are
erased `0xff`; the 16-byte record at raw offset `0x3ffff0` has SHA-256
`b6a4ae92c2f4ef8ab3529145bb8bb1299bebdbb218e02436ded85ef0aa089ffc`. The original instruction/data bytes
are omitted from this public source export.

The far jump is the reset-vector instruction and the adjacent ASCII date is
`03/19/11`.  No Intel Flash Descriptor signature was found at the beginning or
elsewhere as a valid descriptor, and no `$FPT`, `$MN2`, or identified ME
manifest was found.  Combined with the user's full-chip comparison, the
working layout is a descriptorless 4 MiB legacy BIOS ROM, not an update slice
of a larger IFD image.

This does not identify the physical flash part or voltage and does not prove
that every MS-7522 revision has the same storage layout.

## Bootblock mapping

The `AMIEBBLK` occupies raw `0x3c0000..0x3fffff`, corresponding to physical
`0xfffc0000..0xffffffff`.  It contains both early-init DLLs:

| Module | Raw offset | Image base | Entry | Image size |
|---|---:|---:|---:|---:|
| `MINITDLL` | `0x3c1dc0` | `0xfffc1dc0` | `0xfffc2000` | `0x18700` |
| `CSI_INITDLL` | `0x3e6de0` | `0xfffe6de0` | `0xfffe7000` | `0x75e0` |

The top-level call order is CSI wrapper at `0xfffc04e2`, then MINIT wrapper at
`0xfffc0d19`.  See
[candidate-mrc-interface.md](../comparisons/candidate-mrc-interface.md) for
the private parameter/workspace observations.

## MSI cache-as-RAM sequence

The bootblock sequence around physical `0xfffc0376..0xfffc048c` is a direct
architectural match for coreboot's Intel non-evict CAR mechanism:

1. send INIT IPI to other logical processors using APIC state from MSR `0x1b`
   and the ICR at APIC offset `0x300`;
2. clear/disable default MTRR state through `IA32_MTRR_DEF_TYPE` (`0x2ff`);
3. use CPUID leaf `0x80000008` to obtain the implemented physical-address
   width;
4. program the first variable MTRR pair:

   ```text
   IA32_MTRR_PHYSBASE0 low = 0xfff80006
   IA32_MTRR_PHYSMASK0 low = 0xffff0800
   effective range          = 0xfff80000..0xfff8ffff
   type                     = write-back
   size                     = 64 KiB
   ```

5. program the second pair:

   ```text
   IA32_MTRR_PHYSBASE1 low = 0xfffc0005
   IA32_MTRR_PHYSMASK1 low = 0xfffc0800
   effective range          = 0xfffc0000..0xffffffff
   type                     = write-protected
   size                     = 256 KiB
   ```

6. enable MTRRs and caches;
7. set bit 0 in MSR `0x2e0`;
8. touch `0x400` cache lines of 64 bytes from `0xfff80000`, covering exactly
   64 KiB;
9. set bit 1 in MSR `0x2e0`;
10. test `0x4000` dwords, again exactly 64 KiB, with a `0x5a` pattern;
11. use a stack high in the same region, near `0xfff8ffc0`.

The observed failure path emits POST `0x41` and retries/branches through the
vendor error handling.  CAR teardown later disables MTRRs, clears MSR `0x2e0`
bits 1 and 0, and executes `INVD`.

Therefore the first coreboot port should use:

```text
DCACHE_RAM_BASE       0xfff80000
DCACHE_RAM_SIZE       0x00010000
DCACHE_BSP_STACK_SIZE 0x00004000 initially
```

The base and size are established; the 16 KiB coreboot stack allocation is an
implementation choice that must be checked by stack high-water measurement.
The existing coreboot files to use are
`src/cpu/intel/car/non-evict/cache_as_ram.S` and `exit_car.S`, rather than a
new assembly implementation.

## Fintek configuration port

MSI Setup contains `Configure SuperIO Chipset F71882F.`  More importantly, two
paths in extracted `RUN_CSEG` at approximately `f000:cb97` and `f000:cc07`
perform the Fintek configuration protocol using DX=`0x4e`:

```text
write 0x87 to 0x4e
write 0x87 to 0x4e
select/read logical-device and configuration registers through 0x4e/0x4f
restore or finish configuration
write 0xaa to 0x4e
```

The associated 0x59-entry save/restore table includes logical-device 1
registers `0x30`, `0x60`, `0x61`, `0x70`, and `0xf0..0xf2`, which is consistent
with the F71882FG COM1 logical device rather than only a generic Super-I/O
probe.

Thus port `0x4e` is **verified-static** for this firmware.  There is no reason
for build 0 to probe or write `0x2e`.  Coreboot's common Fintek helper can
enable `PNP_DEV(0x4e, 0x01)` at COM1 base `0x3f8` after an ID check.

The expected F71882FG/F71883FG device ID `0x4105`, Fintek vendor ID `0x1934`,
JCOM1 routing, pinout, and voltage still need live confirmation.  The image
establishes the configuration port but not the safe external cable.

## QPI and memory-init boundary

The vendor's verified high-level order is:

```text
CAR established
  -> caller constructs 0x304-byte CSI state
  -> CSI_INITDLL; POST 0xa0..0xaf, errors 0xe8..0xec
  -> caller constructs about 0xe0-byte MINIT policy
  -> MINITDLL with 0x2bcc-byte workspace
  -> accepted completion copies 0x2bcc bytes to physical 0x100000
```

`AX == 0xe801` from MINIT enters a reset-request path.  This is important for
future reset-loop protection, but none of these proprietary calls belongs in
the first build.

The MSI and Intel modules share exact 404-byte and 381-byte memory-test
fragments, strongly supporting common source ancestry.  Intel authorship is an
inference from the wider corpus.  The outer ABI and board policy differ, so
those matches do not make either module a portable blob.

## What this evidence does and does not authorize

It supports a first ROM that:

- maps a complete descriptorless 4 MiB coreboot image;
- reuses Intel non-evict CAR at `0xfff80000`, size 64 KiB;
- enables the Fintek UART only at configuration port `0x4e`;
- preserves MSI's QPI POST ranges for later correlation.

It does not yet support:

- writing CPU-Uncore, PCIEXBAR, X58 QPI, clock, voltage, GPIO, or IMC
  registers;
- assuming every F71882 pin route or logical-device setting;
- treating `CSI_INITDLL`, `MINITDLL`, or the Intel PEIM as a documented ABI;
- claiming any cold-boot or warm-reset behavior without a hardware log.
