> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Post-B06VY minimal ACPI, SMBIOS and resource audit

Date: 2026-09-06

## Scope and point-in-time status

This is an independent source and evidence audit for the first platform
description stages after B06VY.  It supplements
native-ioapic-hpet-resource-acpi-design-2026-09-06.md and resolves two changes
since that earlier design:

- complete read-only censuses of all 24 IOAPIC redirection entries now exist,
  and an isolated entry-23 write/readback/restore test passed; and
- B06VY now contains an automatic, exact-gated canonicalization of all 24
  entries to a masked state.

At the time of this audit B06VY is built and statically verified but has not
yet been run on the target.  Consequently this note does not treat the
automatic B06VY path as hardware-proved.  The earlier ROMMON captures prove
the primitives, not the new complete firmware control flow.

No platform source, Kconfig, devicetree or image was changed by this audit.
Only this research note was added.

Subsequent implementation note: the resource-only recommendation was
implemented as the separately named B06VZ build.  Its two deterministic
builds and source contracts pass, but it remains build-only and must not be
flashed until B06VY has a recorded hardware PASS.  See
[`Documentation/b06vz-fixed-platform-resources.md`](../../Documentation/b06vz-fixed-platform-resources.md)
and the corresponding experimental manifest.

## Executive decision

The next safe release after a successful B06VY hardware run is a
resource-only release.  It may reserve the already-decoded fixed PM, GPIO,
SMBus, RCBA, IOAPIC, LAPIC and flash ranges.  It must not enable ACPI,
interrupt routes, SCI, HPET, 8042, or the full historical ICH10 driver.

Native ACPI should then be introduced as an explicitly non-OS table-validation
stage.  SeaBIOS in coreboot mode copies coreboot tables; it does not synthesize
an ACPI set for this board.  A Linux-ready ACPI image requires later proof of
interrupt delivery, not merely checksums and correct-looking AML.

| Item | Immediate resource successor | Table-validation successor | Linux-ready gate |
|---|---|---|---|
| Fixed PM/GPIO/SMBus/RCBA/LAPIC/flash resources | yes, exact gates | retain | retain |
| Fixed IOAPIC resource | yes, only after B06VY READY | retain | retain |
| HPET resource | no while HPTC is off | after automatic exact HPTC gate | retain |
| SMBIOS base tables | already active | validate/correct strings | retain |
| SMBIOS memory records | no fabricated records | keep absent until all physical-array fields are verified | validate SPD/topology/map |
| MADT BSP and IOAPIC descriptors | no ACPI yet | yes, inspection only | yes |
| IRQ0 to GSI2 override | no | omit, or never boot an OS from the test image | only after delivery proof |
| SCI9 to GSI9 override and FADT SCI | no | structural inspection only | only after SCI proof |
| PM timer in FADT | no ACPI yet | yes, after live PMBASE/timer gate | yes |
| MCFG e0000000, bus 00..ff | no new table | yes only with access/publication separation | yes |
| HPET table | no | after decode, identity and minimum-tick policy | yes |
| FADT 8042 flag / PS2 AML | no | no | only after AB returns 00 and real input works |
| USB keyboard | SeaBIOS owns current preboot path | no AML needed | OS PCI USB drivers; ownership audit |
| _PRT / PIRQ routing | no | no | after one device interrupt route is proved |
| reset register, S states, wake methods | no | no | separate proven milestones |

## Current source contract

The stable audit snapshot is the checked-in B06VY source configuration, rather
than the mutable generated `coreboot/.config`:

- configs/x58-pro-e-b06vy.config:37 identifies B06VY;
- :45 and :87 retain its explicit SMBIOS product and firmware-version inputs;
- :78 enables SMBIOS generation; and
- :86 retains NO_SMM.

The board Kconfig additionally selects MISSING_BOARD_RESET,
NO_ECAM_MMCONF_SUPPORT, NO_MONOTONIC_TIMER and NO_SMM at
coreboot/src/mainboard/msi/x58_pro_e/Kconfig:5-12.  Generic defaults keep the
uniprocessor model at one CPU and one socket at coreboot/src/Kconfig:829-831
and coreboot/src/lib/Kconfig:52-54.  These source locations remain valid even
when a later experimental build is the last generated configuration.

The reduced devicetree has a CPU-cluster node and selected PCI functions, but
no DEVICE_PATH_APIC processors and no D31:F0 LPC device:

- coreboot/src/mainboard/msi/x58_pro_e/devicetree_b06vy.cb:1-25.

The domain currently supplies exactly eight resources:

- coreboot/src/mainboard/msi/x58_pro_e/b06vn_pci.c:2517-2541; and
- their exact post-allocation gates are at :3435-3458.

They are:

| index | range | meaning |
|---:|---|---|
| 0 | 00000000..0009ffff | low RAM |
| 1 | 000a0000..000bffff | VGA aperture, reserved |
| 2 | 000c0000..000fffff | option-ROM/firmware area, reserved |
| 3 | 00100000..bfffffff | low RAM |
| 4 | 100000000..13fffffff | remapped RAM |
| 5 | I/O 1000..ffff | PCI producer window |
| 6 | c0000000..dfffffff | PCI MMIO producer window |
| 7 | e0000000..efffffff | PCIEXBAR/ECAM, reserved |

The B06VQ platform census knows the additional fixed ranges but still prints
PLATFORM_RESOURCES_MODELLED=0 at
coreboot/src/mainboard/msi/x58_pro_e/b06vn_pci.c:1582-1688.

## Hardware evidence boundary

### IOAPIC

The ROMMON captures prove:

- RCBA OIC 00 to 03 exposes fec00000;
- ID register 0 is 00000000;
- version register 1 is 00170020, so the version is 20 and there are 24
  redirection entries;
- every low dword in two complete cold-state censuses had mask bit 16 set;
- reset contents outside that bit varied and must not be interpreted as valid
  routes; and
- entry 23 accepted high-then-low writes and was restored exactly.

B06VY encodes these constants at
coreboot/src/mainboard/msi/x58_pro_e/b06vn_pci.c:361-380 and executes the
automatic gate/census/canonicalization at :1326-1429.  Its domain-scan call
site is :2473-2475.

This proves a decoded, identifiable, fully masked IOAPIC after B06VY succeeds.
It does not prove a delivered interrupt, an ExtINT path, a PIRQ route, an SCI
route, polarity, trigger mode, or destination APIC behavior.

### LAPIC

The BSP LAPIC and virtual-wire state are proved sufficiently for SeaBIOS
timer progress.  The only CPU truth currently publishable is one enabled BSP,
processor UID 0, APIC ID 0.  Physical package core/thread capabilities visible
through CPUID are not evidence that application processors have been started.

### PM and SCI

The current path repeatedly gates PMBASE to 0501 and ACPI decode to 80.
The decoded I/O base is therefore 0500.  The 24-bit PM timer at offset 08 has
been observed advancing.  The current PM1_CNT is zero, so SCI_EN is clear.
PIRQ A through H remain 80, disabled.

The static MSI tables and coreboot's ICH10 common helper both describe SCI as
IRQ/GSI 9 with level/high flags 000d.  This is correlated topology evidence,
not a delivery test.

### HPET

ROMMON proved exact HPTC 00 to 80 decode, live capability
0429b17f:8086a301, four timers, a 64-bit main counter and 69841279-fs period.
A separate run/stop test changed only ENABLE_CNF and left comparator and
interrupt state untouched.  B06VY itself does not reproduce HPTC enable, so
HPET remains unavailable in the immediate resource-only successor.

### PCIEXBAR

The current platform gate reads the full SAD PCIEXBAR tuple:

- high dword 00000000;
- low dword e0000001; and
- host identity 34058086 through the decoded aperture.

See coreboot/src/mainboard/msi/x58_pro_e/b06vn_pci.c:78-82 and :770-805.
The reserved ECAM range is already resource index 7.  This is enough evidence
for an MCFG allocation at e0000000, segment 0, buses 00..ff, but only after
table publication is decoupled from coreboot's PCI configuration access mode.

## Immediate fixed-resource stage

After B06VY passes on target, append resource indexes 8 through 14 without
changing indexes 0 through 7:

| index | helper | base | size | exact admission gate |
|---:|---|---:|---:|---|
| 8 | fixed_io_range_reserved | 0400 | 20 | D31:F3 8086:3a30, BAR4 0401, CMD.IO and HOSTC.HST_EN |
| 9 | fixed_io_range_reserved | 0500 | 80 | D31:F0 8086:3a16, PMBASE 0501, ACPI_CNTL 80 |
| 10 | fixed_io_range_reserved | 0580 | 40 | GPIOBASE 0581, GPIO_CNTL 10 |
| 11 | mmio_range | fec00000 | 1000 | B06VY READY, OIC 03, ID 0, version/count 20/24 |
| 12 | mmio_range | fed1c000 | 4000 | RCBA fed1c001 |
| 13 | mmio_range | fee00000 | 1000 | IA32_APIC_BASE and BSP LAPIC identity gate |
| 14 | mmio_range | ff000000 | 1000000 | fixed W25Q128 16-MiB top mapping |

Do not add HPET in this release.  A later automatic HPET release may append
index 15 at fed00000 size 400 only after it sets HPTC to exactly 80 and repeats
the complete live identity gate.

The helper semantics are important:

- mmio_range adds FIXED, MEM, RESERVE and STORED
  (coreboot/src/include/device/device.h:355-369);
- fixed_io_range_reserved adds FIXED, IO and RESERVE
  (:378-402); and
- resource_range_idx adds ASSIGNED and exact size for fixed resources
  (coreboot/src/device/device_util.c:808-829).

Reserved memory resources enter coreboot's bootmem and therefore SeaBIOS's
E820 reserved map through coreboot/src/lib/bootmem.c:97-128 and :161-180.
The collector deliberately filters for MEM resources at
coreboot/src/lib/memrange.c:247-279, so fixed I/O reservations do not create
meaningless E820 records.

These new ranges cannot overlap the present allocator windows:

- all fixed I/O ends below the PCI I/O producer start at 1000;
- IOAPIC, RCBA and LAPIC lie above the PCI MMIO producer end at dfffffff;
- they do not overlap ECAM e0000000..efffffff; and
- flash begins at ff000000.

Nevertheless acceptance must compare every PCI BAR before and after and extend
the exact resource audit.  Metadata is not automatically harmless if a range
or flag is wrong.

Do not add subtractive LPC resources to the domain.  Subtractive ownership
belongs to a real D31:F0 LPC device model, which is intentionally absent.
Do not duplicate COM1 while the Fintek/Super-I/O resource owner is not
modelled.  Do not duplicate ECAM index 7.

## Why coreboot must generate the tables

SeaBIOS's coreboot path probes PCI, scans CB_MEM_TABLE regions and copies
standard tables, then calls find_acpi_features:

- coreboot/payloads/external/SeaBIOS/seabios/src/fw/coreboot.c:241-274.

copy_table recognizes PIRQ, MP, ACPI RSDP and both SMBIOS entry points:

- coreboot/payloads/external/SeaBIOS/seabios/src/fw/biostables.c:642-648.

Only the QEMU path calls ACPI table synthesis:

- coreboot/payloads/external/SeaBIOS/seabios/src/fw/paravirt.c:269-324.

Thus the current coreboot-mode SeaBIOS does not manufacture missing X58 ACPI
tables.  If coreboot publishes no RSDP, Linux receives no usable native ACPI
description from SeaBIOS.

The existing B06VN capture confirms that this copying path is real for
SMBIOS: SeaBIOS copied both entry points from coreboot in
research/msi/captures/2026-09-06-b06vn-hw-02-seabios-through-ipxe.raw:275-277.

## Minimal native ACPI design

### Build structure

HAVE_ACPI_TABLES compiles the generic ACPI writers and automatically picks up
a board acpi_tables.c and dsdt.asl when present:

- coreboot/src/acpi/Kconfig:82-86; and
- coreboot/src/acpi/Makefile.mk:3-34.

A valid DSDT is mandatory.  If dsdt.aml is missing or invalid, generic
coreboot skips all ACPI output:

- coreboot/src/acpi/acpi.c:1613-1624.

Do not select SOUTHBRIDGE_INTEL_I82801JX as an ACPI shortcut.  Its Kconfig
brings broad LPC, SMI, watchdog and interrupt policy, while its FADT
unconditionally advertises legacy devices and an 8042.  Only narrowly reusable
common PMBASE helpers may be selected, or the board may provide the small
required callbacks locally.

### MADT

Use ACPI_CUSTOM_MADT.  Do not select ACPI_COMMON_MADT_LAPIC:
acpi_create_madt_lapics walks enabled DEVICE_PATH_APIC objects at
coreboot/src/acpi/acpi_apic.c:63-84, and the current tree has none.  That path
would emit zero processors.

The first structural MADT should contain exactly:

1. LAPIC address fee00000 and PCAT-compatible flag;
2. one enabled local APIC record, processor UID 0, APIC ID 0; and
3. one IOAPIC record, ID 0, address fec00000, GSI base 0.

acpi_create_madt_one_lapic is available at
coreboot/src/acpi/acpi_apic.c:35-45.  The generic architecture layer sets the
LAPIC address and PCAT flag at :230-243.

If acpi_create_madt_ioapic_from_hw is used, remember that it reads ID then
version/count at :101-111 and consequently leaves IOREGSEL selecting register
1.  The board must explicitly restore selector 0 and exact-gate its readback.
It is equally valid to fill the descriptor from already-gated B06VY values,
without any table-time MMIO access.

Do not emit any additional enabled or disabled CPU records.  The vendor
template's disabled CPU slots are patch scaffolding, not the current runtime
topology.

The generic common IOAPIC MADT path also emits IRQ0 to GSI2 and SCI overrides
at coreboot/src/acpi/acpi_apic.c:140-149.  Do not select it before delivery
proof.

A MADT with no source overrides is useful only for table transport, checksum
and parser validation.  It is not Linux-ready: default ISA IRQ0 to GSI0 is
unlikely to match this PC/AT topology.  Conversely, adding plausible overrides
before proof can steer an OS onto an untested route.  Therefore the safest
inspection image must not boot an OS.

After the bounded delivery tests pass, add:

- ISA IRQ0 to GSI2, edge/high; and
- ISA IRQ9 to GSI9, level/high flags 000d.

### FADT, PM timer and SCI

The existing ICH10 FADT is useful for offsets but is not safe to reuse
unchanged.  It derives these blocks at
coreboot/src/southbridge/intel/i82801jx/fadt.c:7-29:

| block | live candidate | length | current evidence |
|---|---:|---:|---|
| PM1_EVT | 0500 | 4 | offset correlated; status/enable audit still required |
| PM1_CNT | 0504 | 2 | live PM1_CNT read, SCI_EN currently zero |
| PM_TMR | 0508 | 4 | advancing 24-bit timer proved |
| GPE0 | 0520 | 16 | address correlated; semantics/enables not yet admitted |
| PM2_CNT | 0550 | 1 | defer until a live semantic gate |

fill_fadt_extended_pm_io validates and mirrors legacy addresses into GAS
fields at coreboot/src/acpi/fadt_filler.c:24-71.

For a table-validation image:

- derive and exact-gate PMBASE at runtime; never copy vendor base 0800;
- publish the proved PM timer;
- retain SCI number 9 only as a structural value;
- leave smi_cmd, acpi_enable and acpi_disable zero because NO_SMM remains;
- omit the reset register while MISSING_BOARD_RESET remains;
- set LEGACY_DEVICES for the retained PC/AT/legacy platform contract, but keep
  the independent 8042 bit clear;
- do not advertise S1/S3/S4, wake or unproved power features; and
- do not write PM1, GPE, SMI, PIRQ or IOAPIC route state merely to generate a
  table.

The generic x86 FADT layer gets SCI through ioapic_get_sci_pin, adds CF9 reset
only with HAVE_CF9_RESET, and adds the SMI command only with a permanent SMI
handler:

- coreboot/src/arch/x86/acpi.c:9-46.

The narrow common PMBASE helper reports IRQ/GSI 9 and level/high at
coreboot/src/southbridge/intel/common/pmbase.c:108-115.

The Linux-ready gate is stronger.  With SMI_CMD zero, firmware tells the OS
that ACPI mode is already active.  PM1_CNT.SCI_EN must therefore be set by a
native, exact-gated pre-payload operation before Linux uses the FADT.  Before
that write:

1. read and log all PM1/GPE status and enable fields;
2. clear only understood pending status with write-one-to-clear semantics;
3. ensure no enabled pending source can create a storm;
4. program and prove IOAPIC entry 9 as level/high to BSP APIC ID 0;
5. enable one bounded test SCI source;
6. observe exactly one delivered interrupt; and
7. restore the test source and retain the final intended SCI state.

Until this passes, an FADT may be inspected but must not be described as
OS-ready.

### MCFG

Generic coreboot always places an MCFG creator in its table list
(coreboot/src/acpi/acpi.c:1506-1520), but adds allocations only when
ECAM_MMCONF_SUPPORT is enabled (:161-189).  B06VY deliberately has
NO_ECAM_MMCONF_SUPPORT, so simply enabling ACPI produces an empty MCFG rather
than the required allocation.

Do not remove NO_ECAM_MMCONF_SUPPORT merely to populate MCFG.  That changes
coreboot's PCI configuration-access implementation and is not a metadata-only
operation.

Implement a narrow publication/access separation:

- a new MCFG-publication option defaults to ECAM_MMCONF_SUPPORT;
- the MSI board enables publication using the already-gated base e0000000,
  segment 0, start bus 00, end bus ff;
- coreboot itself retains CF8/CFC for ordinary config access; and
- dsdt_top's ECAM reservation may follow the publication option, while its
  PCFG OperationRegion must remain tied to actual access support.

Relevant current coupling is visible in
coreboot/src/acpi/dsdt_top.asl:43-93 and coreboot/src/device/Kconfig:570-635.
Do not emit a second board MCFG beside the generic empty one.

MCFG does not require interrupt-delivery proof.  It does require this software
separation, the existing exact PCIEXBAR gate and one non-duplicated ECAM
resource.

### HPET

The HPET resource and ACPI table are distinct milestones.  First reproduce
HPTC 00/80 to exactly 80, validate 0429b17f:8086a301, leave the counter stopped
and all timer interrupt/legacy-replacement bits clear, and add the 400-byte
fixed resource.

Only then attach the generic HPET writer.  It reads the live ID without
checking decode and publishes CONFIG_HPET_MIN_TICKS:

- coreboot/src/acpi/acpi_hpet.c:12-63.

The full historical ICH10 Kconfig uses 0080, while the extracted MSI template
contains 37ee.  Neither is yet justified as native policy.  Resolve that
difference from a live vendor HPET table or a documented requirement before
publication.

No comparator or interrupt route needs to be enabled by firmware merely to
describe HPET.  However a Linux boot using HPET interrupts still belongs after
the general IOAPIC delivery milestone.

### Minimal namespace

The first DSDT should expose only:

- the common top-level _PIC selector;
- PCI0 as a PNP0A08/PNP0A03-compatible root, segment 0, bus 0;
- generated producer resources for I/O 1000..ffff and MMIO
  c0000000..dfffffff; and
- one PNP0C02 motherboard-resource object containing admitted fixed consumer
  ranges.

Give the domain ACPI name PCI0 and use pci_domain_fill_ssdt.  That helper skips
reserved and stored resources and emits only producer windows:

- coreboot/src/acpi/acpigen_pci_root_resource_producer.c:57-116.

Do not add _PRT, SATA/USB power methods, hotplug, vendor AML, SMI methods,
sleep methods or a broad ICH10 namespace yet.

## SMBIOS truthfulness

SMBIOS is already enabled independently of ACPI.  Generic x86 defaults it on
at coreboot/src/Kconfig:1001-1009, and the target capture proves coreboot
generated and SeaBIOS copied both entry points.

The generic writer always emits Types 0, 1, 2, 3, 4, 32 and 127.  Types 16,
17, 19 and 20 return zero when CBMEM_ID_MEMINFO is absent:

- coreboot/src/lib/smbios.c:722-891 and :1252-1307.

No current X58 board code creates that CBMEM object.  Omitting memory-device
records is incomplete but more truthful than inventing six sockets, ECC,
maximum capacity or channel topology.

The next SMBIOS-only improvements are safe and do not depend on interrupt
delivery:

1. replace default serial 123456789 and version 1.0 with Unknown or measured
   board data; defaults are visible at coreboot/src/Kconfig:1046-1057, while
   the explicit B06VY product and firmware-version inputs are checked in at
   configs/x58-pro-e-b06vy.config:45 and :87;
2. retain manufacturer MSI and the explicit experimental product/build
   identity;
3. validate the generated Type 4 against the admitted CPU.

Type 4 obtains physical core/thread capability from CPUID but caps enabled
cores at CONFIG_MAX_CPUS:

- coreboot/src/arch/x86/smbios.c:107-186.

With CONFIG_MAX_CPUS=1, enabled-core reporting remains consistent with the
one-BSP firmware state even if capability fields show the physical package.

Do not blindly enable generic Type 19/20 from installed DIMM size.  The
current generic implementation maps installed bytes contiguously from zero,
whereas this port reports 3 GiB below the PCI hole plus 1 GiB remapped above
4 GiB.  Either improve the mapping representation or omit those records until
their address semantics are correct.

Likewise, do not create only one Type 17 merely because the supported
bring-up configuration contains one DIMM.  SMBIOS describes the physical
array, not just firmware's current support envelope.  Before adding memory
records, verify all six connector designations and SPD-address mapping, set
the physical-array device count correctly, represent empty connectors
explicitly, and source the populated device from the admitted SPD block.
Maximum capacity and ECC capability must remain unknown until independently
established.

## USB legacy and 8042 implications

SeaBIOS USB keyboard support does not require a FADT 8042 flag, a PS/2 AML
device or USB ACPI methods.  SeaBIOS's UHCI/EHCI drivers can supply preboot
keyboard input directly after controller and board power/over-current policy
are correct.  The OS later enumerates the PCI USB controllers itself.

The selected payload already compiles USB, UHCI, EHCI, hubs, mass storage and
USB keyboard support at
coreboot/src/mainboard/msi/x58_pro_e/config_seabios_b06vq_usbtrace1:35-45.
It separately keeps PS2PORT enabled at :47-50.  That payload probe is not
evidence for, and does not require, an ACPI 8042 declaration.  Once USB input
works, disabling the unproductive PS/2 probe can be tested as a separate
payload-only change; it must not be conflated with southbridge initialization.

The Fintek KBC core passed command AA with response 55 after temporary decode,
but the primary keyboard-interface test AB returned 03 rather than 00.
Therefore:

- FADT iapc_boot_arch must keep ACPI_FADT_8042 clear;
- do not publish PNP0303 keyboard or PNP0F03 mouse objects;
- do not use the historical ICH10 FADT unchanged, because it sets both
  LEGACY_DEVICES and 8042 at
  coreboot/src/southbridge/intel/i82801jx/fadt.c:31-34; and
- USB keyboard failure must be debugged as USB controller, VBUS,
  over-current/GPIO or SeaBIOS ownership behavior, not hidden by a false PS/2
  declaration.

Because NO_SMM is active, both EHCI legacy-support ownership semaphores and
USB SMI-enable fields must be zero or otherwise neutral at OS handoff.
Current read-only ownership audits are useful evidence, but any future
handoff write needs exact before/after/rollback logging.
The current platform logger reads the relevant EHCI legacy and SMI fields at
coreboot/src/mainboard/msi/x58_pro_e/b06vn_pci.c:952-1007.

## Required target tests

### Resource-only successor

- B06VY first reaches READY with all 24 entries canonical and masked.
- Every resource index 0 through 14 matches exact base, size and flags.
- No overlap exists among domain resources or assigned endpoint BARs.
- PCI tree, all BARs, SeaBIOS video, SATA and iPXE match B06VY.
- SeaBIOS E820 contains each fixed MMIO reservation once.
- No new register write occurs in the resource function.

### Table-validation successor

- Build twice byte-identically.
- iasl succeeds without errors.
- RSDP, XSDT/RSDT, DSDT, FADT, MADT and MCFG checksums pass.
- SeaBIOS logs copying the RSDP rather than synthesizing tables.
- MADT has exactly one enabled LAPIC and one IOAPIC ID 0 at fec00000.
- IOREGSEL is zero after table generation.
- FADT uses PMBASE 0500-relative addresses, SMI_CMD zero, no reset register
  and no 8042 bit.
- MCFG contains one e0000000/segment-0/bus-00..ff allocation, not an empty or
  duplicate table.
- No OS is booted from this inspection image.

### Linux-ready successor

- bounded IRQ0 to GSI2 delivery and exact restoration pass;
- bounded SCI to GSI9 level/high delivery passes with no interrupt storm;
- SCI_EN is intentionally set before payload and read back;
- IRQ overrides exactly match the tested routes;
- at least one PCI device interrupt is proved before _PRT is added;
- EHCI legacy ownership/SMI state is neutral; and
- ten cold plus ten warm boots reach the same ACPI and interrupt state.

## Principal risks

1. A successful B06VY mask census is not interrupt initialization.
2. Publishing IRQ overrides can redirect an OS onto an untested route.
3. Publishing FADT with SMI_CMD zero and SCI_EN zero falsely claims ACPI mode
   is already established.
4. Selecting the full ICH10 driver would combine unrelated, unproved LPC,
   interrupt, power, SMI, watchdog and legacy-input policy.
5. Enabling ECAM_MMCONF_SUPPORT solely for MCFG can change coreboot PCI access
   and destabilize the deliberately bounded enumeration path.
6. An HPET table while HPTC is off can publish an all-ones identity.
7. Generic Type 19/20 can misdescribe the 3-GiB plus remapped-1-GiB physical
   address map.
8. The default SMBIOS serial and version strings are placeholders.
9. Advertising 8042 after only the controller self-test conflicts with the
   failed primary-interface result.
10. A table-validation image can accidentally become an OS test if the
    existing iPXE chain is left unrestricted; gate the payload or test network
    accordingly.

## Recommended release sequence

1. Run and archive B06VY on target.
2. Add only fixed resource indexes 8 through 14.
3. Add automatic, exact-gated HPET decode and resource index 15.
4. Correct and validate existing SMBIOS strings; keep memory-device records
   absent until the physical array and mapped-address semantics are complete.
5. Build a non-OS native ACPI transport/checksum image with one BSP, one
   IOAPIC, minimal FADT/DSDT and decoupled MCFG publication.
6. Prove IRQ0, then SCI/GSI9, then set SCI_EN deliberately.
7. Add tested overrides and permit the first Linux ACPI boot.
8. Add one tested PCI interrupt route and only then begin _PRT/PIRQ work.
9. Defer SMP, reset, sleep/wake, 8042 and broader namespace work to isolated
   milestones.
