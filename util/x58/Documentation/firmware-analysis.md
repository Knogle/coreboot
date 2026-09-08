> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Firmware analysis: MSI X58 Pro-E and comparison corpus

Date: 2026-08-04.  This is a static-analysis report, not a hardware-test
report.

## Executive conclusion

A coreboot port is technically plausible, but it is a new silicon port rather
than a mainboard-only port.  Contemporary coreboot has reusable ICH10 code and
payload support, but no X58 northbridge, Bloomfield/Gulftown CPU, LGA1366, QPI,
or integrated-memory-controller implementation.

The supplied images do not reveal a technically standalone MRC/FSP:

- MSI embeds direct-call `CSI_INITDLL` and `MINITDLL` PE images in its
  bootblock.  They depend on caller-built private structures, fixed early-boot
  context, callbacks/state, and reset behavior.
- Intel desktop/server firmware embeds a large PEIM under GUID `63C0690C...`;
  S55xx UI/PDB metadata names its server form `UnCoreInitPlatform`.  The late
  desktop form needs six PPIs plus Intel-board variables and policy.  No ABI
  compatibility with the MSI DLLs or coreboot has been established.

Separately, no permission to redistribute any extracted initialization module
has been established.  The repository therefore keeps every vendor byte in
private ignored storage; this handling policy is not a legal determination of
the vendors' terms.

The strongest release route is a staged native implementation guided by public
Intel register documentation, MSI board-policy observations, cross-firmware
disassembly, and state captured from a vendor-booted board.  Later interface
reconstruction established that the paired MSI DLLs are technically
isolatable enough for a local, explicitly experimental wrapper: both are
relocatable XIP PE32 images without imports or external callbacks.  They still
require reconstructed CSI/MINIT input structures and output validation, so
this does not turn either module into a standalone or redistributable
`mrc.bin`.  See
[vendor-assisted-memory-init.md](vendor-assisted-memory-init.md).

## Input identity

Exact hashes are recorded in
[MSI image-hashes](../research/msi/image-hashes.md),
[Intel image-hashes](../research/intel/image-hashes.md), and the
[machine-readable corpus manifest](../research/firmware-corpus.json).  The MSI
input is a complete 4 MiB AMIBIOS8 EEPROM image; the user reports that it was
compared with the chip contents and is byte-identical.  Static inspection finds
no Intel Flash Descriptor or identified ME/GbE region.

The previously supplied Intel `SO0920P.bio` was initially misclassified.  Its
internal ID `SOX5820J.86A.0920.2013.0729.0042` identifies the common
DX58SO2/DX58OG family, not the original DX58SO (`SOX5810J`).  A verified
DX58SO 5600 image now provides the correct desktop diff partner.  Both are
Intel `.BIO` update capsules, not raw flash dumps.

The extended corpus serves complementary purposes:

- The MSI image is authoritative for bytes and static behavior of the exact
  user-compared EEPROM.  It supplies MS-7522 GPIO, DIMM/SPD, Super-I/O,
  clock/reset, option-topology, and board-table evidence, but those details
  must not be generalized to every MS-7522 PCB revision.
- Intel desktop firmware exposes close X58 PEI structure and variables.
- Intel S55xx R0033/R0069 provides named early/late
  `UnCoreInitPlatform` implementations for 5500/5520 systems.
- Apple MacPro4,1/5,1 provides an independent Nehalem/Westmere `D71C8BA4...`
  candidate line; the role label comes from UEFITool's GUID database.
- ASUS Rampage III Formula provides a structurally close AMIBIOS8 revision
  series with targeted memory-related release notes.

The MSI input establishes a descriptorless 4 MiB legacy-ROM layout with the
reset vector at raw `0x3ffff0`.  It does not identify the physical chip
part/voltage or prove that every MS-7522 revision has the same storage
topology.  None of the update capsules establishes MSI policy.

## MSI AMIBIOS8 structure

The reproducible [module map](../research/msi/module-map.csv) includes the AMI
body, setup, ACPI, bootblock, microcodes, and option ROMs.  Significant facts:

- `AMIBIOSC0800` occurs immediately before the `AMIEBBLK` bootblock.
- The 256 KiB bootblock occupies raw `0x3c0000..0x3fffff` and maps to
  `0xfffc0000..0xffffffff`.
- Embedded PDB paths name `MINITDLL` and `CSI_INITDLL` in an
  `MSI_X58_RC_Module` build tree.
- The bootblock directly calls CSI initialization before memory
  initialization.  Detailed interface observations are in
  [candidate-mrc-interface.md](../research/comparisons/candidate-mrc-interface.md).
- Before those calls, the bootblock establishes a 64 KiB non-evict CAR window
  at `0xfff80000`, including the same MSR `0x2e0` mechanism used by coreboot's
  generic Intel non-evict implementation.  The exact sequence is recorded in
  [early-boot.md](../research/msi/early-boot.md).
- The AMI SLAB decompresses into 61 legacy segments including `RUN_CSEG`,
  `POST_CSEG`, `DIM_CSEG`, SMI code, SMBIOS, and ACPI support.
- Setup text says `Configure SuperIO Chipset F71882F.`  Extracted `RUN_CSEG`
  uses the Fintek `0x87, 0x87` entry and `0xaa` exit sequence at configuration
  port `0x4e`, making that port verified-static.  The exact chip ID, UART pin
  route, header pinout, and electrical level still need live confirmation.

The image also contains Intel RAID `8086:2822`, Realtek PXE `10ec:8168`, and
JMicron `197b:2363` option ROMs.  None is required for the first native AHCI,
serial-console, single-GPU bring-up.

The setup module lists several supported 4 MiB SPI parts.  Those strings show
flash-driver coverage only; they do not identify the chip physically installed
on this board.

## Intel and Apple UEFI structure

UEFIExtract finds base SEC/PEI services and duplicated platform PEIM volumes.
The corrected [FV map](../research/intel/fv-map.csv) now distinguishes the
common FFSv2 filesystem GUID from the surrounding volume-image file GUIDs.
[PEIM notes](../research/intel/pei-candidates.md) record offsets, dependencies,
and canonical hashes.

The strongest Intel candidate is FFS
`63C0690C-5D9E-4EE3-840E-FAE0E76E291A`.  It occurs in DX58SO,
DX58SO2/DX58OG, and both S55xx releases.  S55xx UI sections and embedded PDB
paths explicitly name it `UnCoreInitPlatform`; this role name is now
**verified-static**, though its internal division of memory/QPI work remains
under analysis.  The late desktop body combines real CF8/CFC, MSR, CPUID,
SMBus/variable, POST, and memory-pattern code.  Its dependency expression and
PEI-service calls rule out treating it as an isolated `mrc.bin`.

Apple MacPro4,1/5,1 instead contains PEIM GUID `D71C8BA4...`.  Its images have
no UI name for this file; `UncoreInitPeim` is an external label from the pinned
UEFITool GUID database.  It evolves from PE32 to TE while retaining long code
matches with MSI `MINITDLL`.  This is independent corroboration, not an ABI
bridge and not evidence that the Apple and Intel GUID families are one module.

## Public documentation changes the feasibility assessment

The public [Intel X58 Express Chipset Datasheet, document
320838](https://www.intel.de/content/dam/doc/datasheet/x58-express-chipset-datasheet.pdf)
documents the IOH device model, DMI/DMIBAR, system-address routing, QPI physical
initialization start (`PHYINITBEGIN`), link status, failures, timeouts, and
retraining controls.

The public [Intel Xeon Processor 5500 Series Datasheet Volume 2, document
321322](https://www.intel.de/content/dam/www/public/us/en/documents/datasheets/xeon-5500-vol-2-datasheet.pdf)
documents the closely related Nehalem Uncore PCI functions and triple-channel
IMC registers.  It includes rank/bank/refresh/CKE/ZQ timing registers, MRS
values, DDR command issue, the physical-init FSM, selectable training states,
timeouts, and completion/pass status for read DQ/DQS, receive enable, write
leveling, and write DQ/DQS.

The Xeon reference is not automatically identical to every Bloomfield or
Gulftown stepping.  Each device ID, field, and sequencing assumption must be
checked against the desktop CPU and both vendor binaries.  Still, these public
register definitions make a clean-room native training state machine realistic;
the remaining hard work is deriving safe ordering, calibration algorithms,
board policy, and reset handling.

## Cross-firmware evidence

MSI `MINITDLL` has exact 888-byte matches with both S55xx
`UnCoreInitPlatform` versions and 890-byte matches with both Apple Uncore
PEIMs.  The original 404/381-byte desktop-Intel matches remain valid.  The
regions contain non-temporal memory-pattern/test code and strongly support
common source ancestry; Intel authorship remains an inference.  They are
semantic anchors, not interchangeable wrappers.  The full matrix and ASUS
revision diff are in
[shared-sequences.md](../research/comparisons/shared-sequences.md).

The observed MSI order also gives an implementation hypothesis to test:

```text
minimal CPU/temporary environment
  -> construct platform/QPI policy
  -> CSI/QPI initialization
  -> construct DIMM/memory policy
  -> DDR3/IMC initialization and training
  -> persistent handoff state
```

This order is **verified-static** at the top-level calls, while the exact
division of QPI work between the two DLLs remains an **inference**.

## Required and optional binary objects

| Object | First bring-up | Long-term disposition | Reason |
|---|---|---|---|
| CPU microcode for selected CPUID | required | use coreboot's normal externally supplied Intel microcode mechanism, subject to license | Correct errata level before complicated initialization |
| `CSI_INITDLL` | no for native path; optional local research only | replace with open QPI/X58 code | Private ABI and MSI/AMI context; redistribution unestablished |
| `MINITDLL` | no for native path; optional local research only | replace with open IMC/DDR3 code | Private ABI, callbacks/reset/state, fixed early environment |
| Intel `63C0690C...` PEIM | no | semantic reference only | Platform PEIM; late desktop form has six PPI dependencies and Intel-board policy |
| Apple `D71C8BA4...` PEIM | no | semantic reference only | Private platform context and no documented coreboot ABI |
| Intel RAID option ROM | no | omit initially | Native AHCI is sufficient; RAID is a non-goal |
| JMicron option ROM | no | omit initially | Disable secondary controller during minimal bring-up |
| Realtek PXE ROM | no | omit | Network boot is not required |
| Discrete-GPU VGA ROM | conditional for SeaBIOS VGA | obtain from the installed GPU/card or user-supplied local file | X58 has no integrated graphics; not a platform-init blob |
| SeaBIOS | payload choice | build from source | Open-source legacy BIOS payload |
| EDK2 Universal Payload | payload choice | build from source | Open-source UEFI payload; runs only after platform init |
| Intel ME/descriptor/GbE blob | no | absent from this descriptorless full-EEPROM layout | Full-chip image has no IFD or identified ME/GbE region |

No vendor ROM or extracted module should be committed.  Redistributability is
not implied by successful extraction.

## Coreboot reuse assessment

Upstream coreboot inspected at commit
`78d314b7c3abd383c661f4f9d1533eb0fb4e6031`, with relevant paths unchanged at
remote main `c6c871909a1200f83ed5e324e6c1ad0f8c1591f9`, has:

- `src/southbridge/intel/i82801jx`: useful ICH10 bootblock, SMBus, LPC, SATA,
  USB, PCIe, ACPI, and SMI foundations;
- common Fintek early-serial helpers and `superiotool` knowledge for F71882;
- no `src/northbridge/intel/x58` implementation;
- no Bloomfield `106Ax` or Gulftown `206Cx` CPU family implementation and no
  LGA1366 socket target;
- X58 recognition in `inteltool`, which is diagnostic support only;
- EDK2 and SeaBIOS payload integrations.

The [public prior-art survey](../research/prior-art.md) found no published
Bloomfield/Gulftown X58 port.  Historical X58 IDs and DMIBAR support are
`inteltool` diagnostics only; EDK2's X58 package targets Simics and does not
train physical DDR3 or QPI.

The existing `x4x` DDR3 raminit is not portable to X58: x4x uses an external
MCH and FSB, while X58 platforms use the CPU-integrated Nehalem IMC and QPI.
Its code organization and timeout patterns may be reused, not its register
sequence.

## Confidence and remaining unknowns

High confidence:

- the MSI bootblock contains the identified direct-call DLLs;
- CSI wrapper precedes MINIT wrapper;
- Intel's named `UnCoreInitPlatform` candidate is PEI/platform dependent;
- current upstream lacks native X58/Nehalem desktop support;
- payload selection cannot solve pre-RAM initialization.

Must be verified on hardware:

- exact flash chip, voltage, and completed external recovery cycle (the 4 MiB
  contents/layout are known for the user-compared EEPROM);
- exact board revision and CPU CPUID/stepping;
- live F71882 ID and JCOM1 UART path/level (configuration port `0x4e` is
  verified-static);
- POST-code electrical path;
- clock generator, voltage controller, straps, GPIOs, and reset topology;
- SPD addresses and physical slot-to-channel mapping;
- cold-, warm-, and CPU-only-reset behavior;
- vendor-initialized PCI/MSR/MMIO state and memory-training results.
