> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WJ Windows A5 static/runtime ACPI audit

Status: source, immutable-release AML and read-only runtime-table audit. The
operator has observed `ACPI_BIOS_ERROR (0xA5)`, but the four bugcheck
arguments are still unavailable. A ranked cause below is therefore a testable
hypothesis unless explicitly called a proven table/topology mismatch.

No coreboot firmware source or ROM was changed by this audit. Proprietary MSI
tables remain below ignored `blobs-local/`.

## Audited inputs

- Immutable B06WJ DSDT extracted from the release ROM: 1,076 bytes, SHA-256
  `63ba8a0d497cc6998bfffd35ea816d7bba5d85f811337ce0bfb21f7c854d7d03`.
- Current native sources:
  `coreboot/src/mainboard/msi/x58_pro_e/acpi_tables.c`, `dsdt.asl`,
  `b06wi_prt.asl`, `b06wj_platform.asl` and `b06wc_acpi.h`.
- Actual pre-override B06WJ runtime inventory and bytes:
  `research/msi/captures/b06wj-grub-{census,rootdump,fadt,tables}-20260908-01.raw`.
- Deterministically reconstructed runtime tables and ACPICA round trips:
  `builds/experimental/b06wj-acpi-runtime-20260908/`.
- MSI vendor references:
  `blobs-local/msi-x58-pro-e/x58-acpi-a5-reference-20260908-{01,02}/` and the
  separately bounded BIOS-NVS capture.
- Microsoft bugcheck categories:
  [Bug Check 0xA5: ACPI_BIOS_ERROR](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0xa5--acpi-bios-error).

## Decisive finding 1: the runtime CTBL SSDT has an invalid resource flag

The 113-byte runtime `SSDT` contains this DWORD Address Space descriptor for
`CTBL._CRS` (descriptor bytes start with `87`):

```text
87 17 00 00 1c 02 ...
            ^^
            General Flags = 0x1c
```

For a DWORD Address Space descriptor, ACPI 6.6 Table 6.46 requires General
Flags bits 7:4 to be zero and labels bit 0 ignored. Older ACPI 2.0 and IASL
use bit 0 for Consumer/Producer. The fixed, positive-decode historical
consumer encoding is therefore `0x0d`, not `0x1c`; the reserved-bit violation
is independent of that version distinction.
[ACPI 6.6 DWORD descriptor](https://uefi.org/specs/ACPI/6.6/06_Device_Configuration.html#dword-address-space-descriptor),
[ACPI 2.0](https://uefi.org/sites/default/files/resources/ACPI_2.pdf)

The defect is directly traceable to native code:

- `coreboot/src/include/acpi/acpigen.h:276` defines
  `ADDR_SPACE_GENERAL_FLAG_CONSUMER` as `0x10`, a reserved bit rather than
  bit 0.
- `coreboot/src/acpi/acpigen.c:2402-2408` combines that value with
  `MaxFixed|MinFixed`, producing `0x1c`.
- `coreboot/src/acpi/acpi.c:323-347` explicitly calls
  `acpigen_resource_consumer_mmio()` for the coreboot-table device.

This is an **actual specification violation in the published RAM bytes**,
not merely a difference from MSI. ACPICA makes the consequence visible: it
disassembles the original as `DWordMemory (ResourceProducer, ...)`, silently
drops reserved bit 4, and recompiles General Flags as `0x0c`. Thus even a
permissive parser receives the opposite Consumer/Producer semantic. The
original table checksum is valid, but a checksum cannot make the descriptor
valid. Windows-side selection/consumption of a specific table has not been
captured by this static and pre-handoff audit.

Microsoft documents A5 parameter 1 `0x0f` for a resource descriptor that ACPI
could not parse. This is now the strongest single static A5 hypothesis. It is
still not the proven stop subtype until the four bugcheck arguments or an
isolated RAM-table A/B result are captured. The minimal byte correction is
`1c -> 0d` plus SSDT checksum adjustment `aa -> b9` for these exact runtime
bytes; relocation still requires rebuilding pointer tables/checksums.

## Decisive finding 2: root interrupt routes are incomplete

The released WJ `RPRT` has only 20 entries: IOH D3 and ICH D26/D27/D28/D29/D31
(`b06wi_prt.asl:4-27`). The fresh target census shows these additional
nonzero PCI `Interrupt Pin` values:

```text
00:00.0, 00:01.0..00:0a.0: A
00:16.0..00:16.7:          A/B/C/D/A/B/C/D
```

The live MSI APIC-mode root package supplies D0, D1, D2, D4-D10 and D22,
each with wildcard function and pins A/B/C/D mapped to GSI 16/17/18/19. WJ
omits every one of those routes. This is a **proven namespace-versus-hardware
mismatch**, and it directly matches the class of Microsoft A5 routing failures
(`0x10001`/`0x10003`). Without the bugcheck arguments it is not yet proven to
be this particular boot's fatal subtype.

Two important exclusions follow from the same census:

- `00:1f.6` (`8086:3a32`) reports INTC, not INTD. WJ's wildcard D31/C route to
  GSI18 already covers it; adding D31/D is not justified.
- `00:0d.7` (`8086:341b`, class 0600) reports INTA, but the vendor APIC package
  also deliberately omits D13. Do not invent a D13 route from the pin byte.

The isolated `ioh-prt-only` RAM candidate changes only the package count
20→64 and adds the 44 vendor-correlated tuples. It does not add sleep objects
or PCI memory windows:

```text
path:   builds/experimental/acpi-ramlab/b06wj-ioh-prt-20260908/ioh-prt-only/dsdt.aml
size:   1618 bytes
SHA256: ebb760c9558dca096b8b1bd21820416c1b04020375afd7512da6d3b421ea8bbf
```

IASL 20251212 reports zero errors and warnings; compile/disassemble/recompile
is byte-identical, the checksum is valid, the unchanged control reproduces the
released AML, and a second build reproduced the same candidate bytes. This is
not a hardware result.

## Table-by-table result

| Interface | Audit result | Evidence and interpretation |
|---|---|---|
| RSDP/RSDT/XSDT | structurally valid | Runtime RSDP revision 2 has valid legacy and extended checksums, nonzero RSDT/XSDT, and every listed table checksum validates. A separate revision-0 RSDP is not required. |
| FADT header/pointers | valid | Actual revision 6/length 276; FACS/DSDT and X_FACS/X_DSDT pairs agree. This does not match Microsoft's length-versus-revision A5 class. Source is `acpi_tables.c:124-196`; generic full-length creation is `coreboot/src/acpi/acpi.c:1174-1213`. |
| FADT PM GAS | internally consistent | Legacy and extended PM1 event (`500`, width 32), PM1 control (`504`, 16), PM timer (`508`, 32) and GPE0 (`520`, 128) agree; access sizes are appropriate. PM2 is optional and consistently absent. |
| ACPI mode/timer | live pass | `PM1_CNT@504=0001` proves SCI_EN. PM timer samples `55cf77→6645a7` prove progress. A5 ACPI-enable timeout and non-running PM-timer hypotheses are therefore low priority. |
| FADT fixed buttons | **semantic defect** | Flags `0x75` set `POWER_BUTTON`, meaning the power button is a control-method device, but the DSDT defines no `PNP0C0C`. ACPI requires a power-button mechanism. Vendor flags clear this bit and use the fixed event. This is definite contract incompleteness at `acpi_tables.c:169-177`, but no captured A5 argument links it to the early boot stop. |
| MADT | coherent | Revision 3/length 84; LAPIC UID/APIC 0, unique IOAPIC ID1 at `fec00000`, GSI base0, IRQ0→GSI2 high/edge and SCI9→GSI9 high/level. All published PCI GSIs 16-23 lie in the hardware's 0-23 range. `acpi_tables.c:217-255`. |
| CPU namespace | valid narrow model | `ACPI0007` `CP00`, UID0 (`b06wj_platform.asl:20-24`) matches MADT processor UID0. Legacy vendor `Processor()` objects and twelve LAPICs describe a different SMP runtime and are not evidence that WJ's BSP object is invalid. Missing P/C-state methods reduce functionality, not table validity. |
| PCI root `_CRS` | valid descriptors, incomplete platform view | Bus0-255 contains current bus0; CF8-CFF is a consumer; I/O producer ranges are arithmetically consistent; C0000000-DFFFFFFF covers all logged WJ-assigned BARs and does not overlap RAM. However, `dsdt.asl:38-74` omits vendor/open-Intel legacy VGA producers A0000-BFFFF and C0000-DFFFF. The target uses VGA forwarding and a physical HD5450 VBIOS, so this is the second isolated test, not a proven malformed descriptor. |
| Root/child `_PRT` syntax | valid where present | All WJ entries use function wildcard `ffff`, pin 0-3, direct-GSI source 0 and GSIs covered by IOAPIC. NPE3 and P0P8 child mappings correctly cover the present GPU/NIC paths (`b06wi_prt.asl:34-58`). The defect is missing root coverage described above, not entry encoding. |
| MCFG | valid | Base `e0000000`, segment0, buses00-ff; runtime ECAM responds and matches MSI. `acpi_tables.c:294-312`. |
| HPET | coherent | Table and `PNP0103` object agree on `fed00000`; measured block ID is `8086a301`; length/reservation is `0x400`. Minimum tick `0x80` differs from vendor policy but is not a structural error. `acpi_tables.c:315-338`, `b06wj_platform.asl:26-43`. |
| CTBL SSDT | **invalid resource descriptor flags** | Runtime checksum/length validate, and the claimed memory lies inside firmware-reserved memory, but `CTBL._CRS` encodes General Flags `0x1c`: reserved bit 4 set and Consumer bit 0 clear. Native code intended a consumer. Correct encoding is `0x0d`; see decisive finding 1. |
| SPCR | structurally coherent | SPCR is extra versus MSI but describes the working 16550 COM1; removing it remains only a low-priority isolation experiment. |
| Sleep objects | omitted, low priority | `_S0`/`_S5` are present in MSI and both Intel desktop X58 references but absent in WJ. OSPM assumes S0 and can operate without an advertised soft-off method; their absence is not tied to a known captured subtype. Test separately after routing/resources. |

The immutable WJ DSDT compiles and disassembles with no IASL errors or warnings
and has a valid table checksum. This rules out many byte-level DSDT errors, but
IASL cannot detect missing routes for live PCI functions or an incorrect
fixed-feature policy. Conversely, the runtime SSDT disassembles without a
diagnostic but does **not** round-trip byte-identically: ACPICA normalizes its
invalid `0x1c` flag to producer `0x0c`. A clean IASL log is therefore not proof
that reserved descriptor bits are correct.

## Ranked one-change tests

1. **Repack control, then correct only the CTBL SSDT flag.** Change General
   Flags `1c -> 0d` and recompute all affected checksums/pointers. This tests
   the only presently proven malformed ACPI resource byte and maps to
   Microsoft A5 `0x0f`. If an urgent run combines it with `ioh-prt-only`, label
   that result a two-defect compound test: success cannot identify which delta
   was causal.
2. **`ioh-prt-only`.** This has the strongest live topology evidence and maps
   directly to documented A5 routing categories. Do not mix it with VGA or
   sleep deltas in the isolation sequence.
3. **Legacy PCI root windows only.** Test A0000-BFFFF and C0000-DFFFF together
   after the routing result. Both MSI runtime resources and Intel desktop X58
   AML contain them, and they do not overlap WJ E820 RAM.
4. **FADT fixed power-button correction.** Clear `POWER_BUTTON` only if the
   existing ICH10 fixed PWRBTN status/enable semantics are first admitted, or
   provide a real control-method device backed by a known GPE. Do not merely
   clone vendor flags `0xa5`.
5. **S0/S5 only**, then optional SPCR removal, only if the above do not change
   the failure.
6. Capture all four bugcheck arguments or a crash dump/KDCOM trace. That can
   distinguish PCI root resources (`0x02`), routing (`0x10001/0x10003`), FADT
   length (`0x1000b`), SCI/ACPI mode (`0x11`) and timer (`0x20000`) without
   accumulating speculative changes.

## Do not transplant from MSI

The live MSI DSDT patches its BIOS `OperationRegion` to
`BF78E064 = FACS + 0x64`. Its NVS supplies dynamic RAM/PCI windows and
sleep/SMI globals. The decoded `MG` values happen to reproduce the reference
machine's resource map, but the reference has different RAM and PCI devices.
Detailed named offsets are documented in
`vendor-live-acpi-nvs-decoded-2026-09-08.md`.

Do not copy the patched DSDT, FACS-adjacent NVS, SMI command interface,
`_PTS/_WAK`, vendor LAPIC list/IOAPIC ID, GPIO levels, PIRQ low nibbles or
complete runtime register images. Transplantable **semantics** are limited to
static interfaces corroborated on the target: current direct-GSI mappings,
SCI polarity/trigger, and resource windows derived from the target's own E820
and PCI allocation.
