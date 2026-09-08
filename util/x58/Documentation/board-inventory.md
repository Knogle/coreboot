> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Board inventory

This file separates published/MSI-firmware evidence from facts that must be
measured on the target board.

## Established reference facts

| Item | Value | Source/status |
|---|---|---|
| Target | MSI X58 Pro-E, MS-7522 family; DMI revision 3.0 | verified on running vendor firmware; physical silkscreen still to confirm |
| CPU socket | LGA1366 | documented by MSI |
| IOH / southbridge | Intel X58 / ICH10R | documented by MSI |
| Memory | six DDR3 slots, triple-channel | documented by MSI |
| Serial interface | one board header listed | documented by MSI; routing/electrical level hardware-unverified |
| Super I/O | Fintek F71882F/F71882FG family; configuration port `0x4e` | verified-static MSI Setup and `RUN_CSEG`; exact live ID/package and UART route hardware-unverified |
| Main LAN | Realtek RTL8111/8168/8411 PCIe, `10ec:8168` revision 02 | verified by live PCI enumeration |
| Secondary storage | populated JMicron JMB363, `197b:2363` revision 03 | verified by live PCI enumeration |
| Firmware family | AMIBIOS8; live ID `V8.14B8` | verified-static and verified on running board |

MSI reference: [X58 Pro-E specifications](https://mx.msi.com/Motherboard/X58_ProE/Specification)
and [product brief](https://storage-asset.msi.com/datasheet/original/mb/global/X58_ProE.pdf).

## Image-to-board identity

The archive contains `A7522IMS.8F0`, which is in the MS-7522 family, but its
bundled short release note labels the product `X58 Pro SLI (MS-7522)`.  The
user reports that the 4 MiB file was compared with an earlier complete EEPROM
capture and was byte-identical.  The directly read 2026-08-04 target instead
has SHA-256 `75232f915d44f2180d35c6b293d21694fae8c217c8f54bbd84a9a7c8819a2db1`
and identifies itself as `V8.14B8`; it is not whole-image-identical to `8F0`.
Both `MINITDLL` code sections are nevertheless byte-identical.  Keep the two
firmware states distinct; see [MSI image hashes](../research/msi/image-hashes.md).

Still record the physical PCB model/revision, current BIOS ID, flash-chip
marking, and all component variants.  Byte identity establishes the firmware
image; it does not prove that every MS-7522 PCB revision uses identical
GPIO/clock/power policy or the same flash part.

## Required physical inventory

Fill these fields from markings or measurements; do not infer them from setup
strings:

```text
board silkscreen model:
board revision:
current boot-screen BIOS ID:
current BIOS setup defaults changed:
flash chip manufacturer/model:
flash package:
flash voltage:
flash capacity:
socket type/orientation mark:
Super I/O full marking:
Super I/O configuration port: 0x4e (verified-static; confirm live ID)
UART header designator/pinout/electrical level:
POST-code connection/device:
clock generator marking:
voltage-controller marking(s):
CPU model/S-spec/CPUID/stepping:
DIMM model/SPD hash/slot silkscreen:
GPU model/VBIOS hash:
PSU:
SATA/USB test device:
jumper positions:
```

## Initial fixed test configuration

After the items above are known, freeze one configuration:

```text
the verified Xeon E5645 / CPUID `0x206c2` stepping 2
one non-XMP JEDEC DDR3 DIMM in the verified first slot
one simple discrete GPU
one SATA device or USB diagnostic device
AHCI, no RAID
all overclocking and XMP disabled
no optional controllers unless needed for diagnostics
```
