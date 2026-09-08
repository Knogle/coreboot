> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Native IOAPIC, HPET, resource and ACPI bring-up design

Date: 2026-09-06

## Scope and result

This note defines the smallest reviewable path from the current MSI X58 Pro-E
`B06VN`/`B06VW` platform code to native platform resources and ACPI.  It does
not enable an interrupt, change a PIRQ route, enable SCI/SMI, advertise an
8042, or incorporate any vendor AML or table bytes.

The principal result is that the historical `i82801jx` LPC driver cannot be
selected as a shortcut.  Its IOAPIC helper is not a registration-only
operation, its LPC init performs broad interrupt and power-policy writes, and
its ACPI namespace advertises hardware which the current MSI path has not
proved.  The safe design is staged:

1. model fixed resources without changing hardware;
2. enable only the already-proved `OIC=03` IOAPIC decode, with exact gates;
3. integrate the now-proved HPET decode and counter behavior behind exact
   gates;
4. add minimal native, table-only ACPI for the one-BSP configuration;
5. prove one interrupt route and SCI delivery before calling the result
   OS-ready; and
6. add device routing, SMP, sleep and legacy-input declarations only after
   their independent hardware milestones.

No coreboot source is changed by this analysis.  In particular,
`src/mainboard/msi/x58_pro_e/b06vn_pci.c` is intentionally untouched while the
concurrent SATA branch is being developed.  Consequently there is no claimed
build or hardware result for the design below.

## Inputs

Open source inspected locally:

- `coreboot/src/mainboard/msi/x58_pro_e/b06vn_pci.c` and its current Kconfig;
- `coreboot/src/southbridge/intel/i82801jx/lpc.c`, `fadt.c`, and `acpi/*.asl`;
- `coreboot/src/arch/x86/ioapic.c` and `arch/x86/acpi.c`;
- `coreboot/src/acpi/acpi.c`, `acpi_apic.c`, `acpi_hpet.c`,
  `acpigen_pci_root_resource_producer.c`, and `dsdt_top.asl`;
- `coreboot/src/southbridge/intel/common/pmbase.c`; and
- the current coreboot Kconfig and Makefile rules.

Board evidence:

- `captures/2026-09-06-b06vp-car-ioapic-decode-trace.metadata.json`;
- `captures/2026-09-06-b06vp-car-ioapic-id-version-trace.metadata.json`;
- `captures/2026-09-06-b06vp-car-apic-hpet-readonly-trace.metadata.json`;
- `captures/2026-09-06-b06vp-car-hpet-decode-census-trace.metadata.json`;
- `captures/2026-09-06-b06vp-car-hpet-counter-runstop-trace.metadata.json`;
- `captures/2026-09-06-b06vp-fintek-kbc-selftest-live.metadata.json`;
- `captures/2026-09-06-b06vp-fintek-kbc-keyboard-interface-live.metadata.json`;
- `platform-init-audit-2026-09-06.md`;
- `acpitbl-static-analysis-2026-09-06.md`; and
- `Documentation/b06vq-platro1.md`.

The proprietary MSI artifacts remain local.  Only their hashes, table
structure and independently reconstructable semantics are referenced here.

## Evidence boundary

| Item | Current evidence | Classification |
|---|---|---|
| BSP LAPIC | `IA32_APIC_BASE=fee00900`, ID 0, version `01060015`; the B06VP virtual-wire gate reached `SVR=10f`, `LVT0=700`, `LVT1=400` and SeaBIOS timer progress was observed | proved for the BSP |
| ICH10 IOAPIC decode | exact byte transition RCBA `OIC 00 -> 03`; `fec00000` changed from open bus to a responding selector/window | proved decode only |
| IOAPIC identity | selector 0 returned ID 0; selector 1 returned `00170020`, hence version `20`, maximum-redirection index `17`, 24 inputs | proved read-only identity/count |
| IOAPIC redirections | not read as a complete set and never written | not proved |
| IRQ delivery through IOAPIC | no vector, polarity, trigger or delivery test | not proved |
| PIRQ routing | route bytes A-H are all `80` in the current path; vendor values and DxxIR topology are observations only | deliberately disabled/not proved |
| HPET decode/capability | exact `HPTC 0 -> 80` exposed `GCAP_ID=0429b17f:8086a301`: Intel vendor `8086`, rev 1, four timers, 64-bit counter, legacy capability, period 69841279 fs | hardware-proved in one CAR session |
| HPET counter | with all comparator/interrupt state zero, setting only `ENABLE_CNF` advanced `0013c26a -> 00286cf5 -> 003d1792`; after clearing it, two samples stayed `006690fe`; interrupt status remained zero | run/stop semantics proved; interrupt delivery not proved |
| PM timer | current `PMBASE=0501`, decode enabled and B06VQ-PLATRO code can measure the 24-bit timer | decoded; target run still determines the exact release state |
| SCI | vendor and static ACPI use IRQ/GSI 9, but current `PM1_CNT=0`, so `SCI_EN=0` | topology evidence only; delivery disabled |
| ECAM | an identity read at `e0000000` returned X58 host ID `34058086`; vendor MCFG covers bus `00..ff` | strong base/identity evidence, but coreboot intentionally uses CF8/CFC |
| 8042 controller | command `AA` returned `55` after temporary LPC KBC decode | controller core proved |
| primary PS/2 interface | command `AB` returned `03`, not success `00` | negative diagnostic; do not advertise |
| SMM | board selects `NO_SMM`; no native ACPI-enable SMI contract exists | intentionally absent |

The IOAPIC result must not be described as interrupt initialization.  `OIC=03`
only established MMIO decode.  Likewise, the KBC controller self-test does not
override the failed primary-interface test.

## Why the historical ICH10 LPC path is not a safe shortcut

### IOAPIC helper has large write side effects

`i82801jx_enable_apic()` does three operations:

1. writes byte `03` to RCBA `OIC` and reads it back;
2. calls `ioapic_lock_max_vectors()`; and
3. calls `register_new_ioapic_gsi0()`.

Only the first operation matches the completed MSI live experiment.
`ioapic_lock_max_vectors()` reads and rewrites IOAPIC register 1; coreboot
documents the maximum-redirection field as write-once on some chipsets.
`register_new_ioapic_gsi0()` then:

- rewrites the IOAPIC ID;
- writes both dwords of every one of the 24 redirection entries to mask them;
  and
- on a PC/AT build, replaces redirection entry 0 with an ExtINT route to the
  BSP LAPIC.

That is at least 48 redirection-window data writes plus identity/policy writes.
It also changes the interrupt path which already let B06VP/SeaBIOS make timer
progress.  It therefore cannot be used merely to make the IOAPIC visible to
ACPI.

`ioapic_create_dev()` is also not a side-effect-free metadata helper: for GSI
base zero it calls the same `register_new_ioapic_gsi0()` path before allocating
the device object.

### LPC init changes unrelated policy

The historical `lpc_init()` additionally:

- rewrites SERIRQ mode;
- routes all eight PIRQs and every enumerated PCI `INT_LINE` to IRQ 11;
- rewrites power/GPI/NMI state and clears PM status;
- initializes RTC, DMA, PIC and IRQ 9 trigger mode;
- enables HPET without validating its live capabilities;
- changes clock-gating fields; and
- requests ACPI enable/disable through port `b2`.

The all-IRQ11 policy conflicts with MSI's measured DxxIR/PIRQ topology and the
current all-disabled PIRQ state.  Port-`b2` ACPI control assumes a working SMI
handler, while this port deliberately selects `NO_SMM`.  The complete driver
must remain unselected.

### Historical FADT and ASL over-advertise the current port

The upstream ICH10 FADT unconditionally sets both `LEGACY_DEVICES` and `8042`.
The upstream `lpc.asl` declares PS/2 keyboard and mouse devices permanently
present.  It also declares broad DMA, PIC, RTC, timer, firmware and Super-I/O
resources.  `ich10.asl` further pulls in USB, PCIe, SATA, GPIO and SMI-facing
objects using fixed build-time bases.

Those files are useful register-map references, not a safe namespace for this
port.  The current `AB=03` keyboard-interface result specifically forbids the
8042 flag and PS/2 devices.

## Resource ownership

### Current domain resources

`b06vn_read_resources()` currently creates exactly eight resources:

| index | range | current role |
|---:|---|---|
| 0 | `00000000..0009ffff` | RAM |
| 1 | `000a0000..000bffff` | fixed VGA MMIO hole |
| 2 | `000c0000..000fffff` | reserved RAM/option-ROM area |
| 3 | `00100000..bfffffff` | low RAM |
| 4 | `100000000..13fffffff` | remapped RAM |
| 5 | `1000..ffff` | PCI I/O producer window |
| 6 | `c0000000..dfffffff` | PCI MMIO producer window |
| 7 | `e0000000..efffffff` | fixed reserved PCIEXBAR/ECAM window |

B06VQ-PLATRO only logs the fixed southbridge ranges and explicitly reports
`PLATFORM_RESOURCES_MODELLED=0`.  It does not create them.

### First resource-only patch

The next resource patch should create only fixed, assigned, reserved resources
whose decode is already proved or architecturally fixed.  It must perform no
hardware write and must leave resource indexes 0 through 7 unchanged.

| range | size | initial disposition | gate |
|---|---:|---|---|
| SMBus I/O `0400..041f` | `20` | fixed I/O reserve | D31:F3 ID `8086:3a30`, BAR4 `0401`, `CMD.IO=1`, `HOSTC.HST_EN=1` |
| PM I/O `0500..057f` | `80` | fixed I/O reserve | D31:F0 ID, PMBASE `0501`, ACPI control `80` |
| GPIO I/O `0580..05bf` | `40` | fixed I/O reserve | GPIOBASE `0581`, GPIO control `10` |
| IOAPIC `fec00000..fec00fff` | `1000` | fixed MMIO reserve | only when automatic `OIC=03` exact gate is enabled |
| RCBA `fed1c000..fed1ffff` | `4000` | fixed MMIO reserve | RCBA register `fed1c001` |
| BSP LAPIC `fee00000..fee00fff` | `1000` | fixed MMIO reserve | `IA32_APIC_BASE` and LAPIC identity gate |
| firmware `ff000000..ffffffff` | `1000000` | fixed MMIO reserve | fixed 16-MiB flash layout |
| HPET `fed00000..fed003ff` | `400` | add only with exact automatic HPTC enable/gate | `HPTC=80`, live ID `8086a301`, period dword `0429b17f` |

The existing ECAM resource at index 7 already reserves the full 256-MiB
window and must not be duplicated.  COM1 should not be duplicated until the
Fintek/LPC endpoint resource owner is modelled.

The fixed I/O ranges lie below the current PCI allocator window at `1000`;
the fixed MMIO ranges lie above the PCI MMIO window ending at `dfffffff` and
outside ECAM.  Thus the intended patch should not move a PCI BAR.  Acceptance
still requires an exact before/after allocation comparison and a global
overlap audit.

Do not attach subtractive I/O or firmware resources to the PCI domain merely
to imitate `i82801jx_lpc_read_resources()`.  Subtractive decode belongs to an
actual D31:F0 LPC device.  The current reduced devicetree omits that device;
fixed reservations are the correct first model, and a real LPC device model is
a later isolated change.

For ACPI, reserved resources must be consumers under a `PNP0C02` motherboard
resource object, not producer ranges in `PCI0._CRS`.  Coreboot's
`pci_domain_fill_ssdt()` intentionally skips `IORESOURCE_RESERVE` and stored
RAM, so it can generate the PCI producer windows while a separate `PNP0C02`
object exposes fixed reserved ranges.

## Minimal IOAPIC path

### Decode-only automatic step

An initial build may automate exactly the already-proved transition:

```text
preconditions:
  D31:F0 identity       3a168086
  RCBA register         fed1c001
  OIC byte              00 or already 03
  HPTC                  independently logged, not changed

mutation:
  if OIC == 00: write8(OIC, 03), then mandatory readback

postconditions:
  OIC                   03 exactly
  IOREGSEL initial read  not ffffffff
  ID                    00
  version/count         20 / 24 inputs
```

No sub-bit meaning is assigned to `OIC=03`; it is treated as the exact
datasheet/vendor/coreboot-observed target byte.  This step must not call
`ioapic_lock_max_vectors()`, `setup_ioapic()`, `register_new_ioapic_gsi0()` or
`ioapic_create_dev()`.

Merely reading an IOAPIC register writes `IOREGSEL`.  Every diagnostic/table
gate should restore selector zero at the end.  The generic
`acpi_create_madt_ioapic_from_hw()` reads ID followed by version/count and
therefore leaves selector 1 selected.  A board wrapper can restore selector 0
by making `get_ioapic_id()` the final access, or a later generic cleanup can
reverse the read order.  Neither path writes the data window.

### Safest next live IOAPIC experiment

Before any redirection data write, perform a complete read-only census under
CAR/ROMMON with CPU interrupts disabled:

1. assert the exact D31:F0, RCBA, `OIC=03`, ID 0 and `00170020` gates;
2. for selectors `10..3f`, write only `IOREGSEL` and read `IOWIN`;
3. log all 24 low/high redirection pairs without interpreting unknown bits;
4. restore selector 0; and
5. reconfirm `OIC=03`, ID and RCBA.

Because ROMMON permits 32 operations, the census is split into four validated
six-entry scripts.  The operation counts below are hexadecimal `20` (32
decimal):

| entries | script | `PROGRAM_FNV` | ops |
|---:|---|---:|---:|
| 0..5 | `ich10-ioapic-redir-ro-00-05-b06vp-oic3.xrs` | `9867da61` | `20` |
| 6..11 | `ich10-ioapic-redir-ro-06-11-b06vp-oic3.xrs` | `18fdd161` | `20` |
| 12..17 | `ich10-ioapic-redir-ro-12-17-b06vp-oic3.xrs` | `27f7b6a1` | `20` |
| 18..23 | `ich10-ioapic-redir-ro-18-23-b06vp-oic3.xrs` | `e0e98bc1` | `20` |

All selector writes are marked reversible and each script explicitly restores
selector zero.  The host validator reports `AUTO_ROLLBACK=YES`; execution is
still a separate, operator-reviewed action and was not performed as part of
this audit.

This establishes whether any input is initially unmasked and provides the
rollback baseline for later one-entry tests.  It does not route an interrupt.
The one-second AC cycle remains recovery.

The subsequent interrupt experiment should modify only one selected
redirection entry, preserve its complete original pair, keep every other entry
byte-identical, use a bounded PIT/event counter, and restore the pair.  Its
exact vector/delivery mode should be chosen only after the census and the
existing BSP LVT/PIC path are reviewed together.  A bulk `clear_vectors()` or
all-PIRQ route is specifically excluded.

## Minimal HPET path

The prepared script `research/scripts/ich10-hpet-decode-b06vp-oic3.xrs` was
executed successfully with the intended boundary:

- exact LPC/RCBA/OIC/HPTC/open-aperture assertions;
- one 32-bit masked HPTC update with mask `00000083`, value `00000080`;
- address-select bits `1:0` held at zero, selecting `fed00000`;
- no main-counter, comparator, legacy-replacement or IRQ write; and
- reads of capability, general configuration, counter and timer-0 registers.

The archived result is:

```text
HPTC                     00000000 -> 00000080
GCAP_ID                  0429b17f:8086a301
vendor / revision        8086 / 01
timers / counter width   4 / 64 bit
counter period           69841279 fs
GEN_CFG / ISR / counter  all zero before the functional test
```

The follow-up `ich10-hpet-counter-runstop-b06vp-hptc80.xrs` also passed.  It
set only `ENABLE_CNF`, observed three increasing low-counter samples, cleared
only that bit, and then observed an unchanged counter around the same delay.
The high counter dword and interrupt status stayed zero; comparators, timer
configuration, interrupt enables and legacy replacement were untouched.

These results justify an automatic, exact-gated `HPTC=80` decode, the
`fed00000..fed003ff` resource and live capability validation.  They do not
justify any comparator, IRQ, FSB-delivery or legacy-replacement configuration.

`acpi_write_hpet()` blindly reads the ID at `HPET_BASE_ADDRESS`; attaching it
while decode is off would publish `ffffffff`.  It also uses
`CONFIG_HPET_MIN_TICKS`.  The full ICH10 Kconfig defaults that value to `80`,
whereas the MSI static template records decimal 14318 (`37ee`).  This is an
unresolved policy input.  Do not invent a value: capture the live vendor HPET
table and/or establish the required periodic-mode minimum before publication.

## Native ACPI table design

### Build dependencies and separation

The first ACPI build should select only narrowly reusable components:

- `HAVE_ACPI_TABLES`;
- `ACPI_CUSTOM_MADT`;
- `IOAPIC`, solely to compile the read/table helpers, not to invoke setup; and
- `SOUTHBRIDGE_INTEL_COMMON_PMBASE` if its PM access and SCI topology helpers
  are used.

It must continue to select `NO_SMM` and `NO_ECAM_MMCONF_SUPPORT`, and must not
select `SOUTHBRIDGE_INTEL_I82801JX`.

With `HAVE_ACPI_TABLES`, coreboot automatically compiles a board `dsdt.asl`
and board `acpi_tables.c` when present.  A minimal DSDT must be supplied;
enabling tables without valid AML is not a useful milestone.

### FADT

The first board-native `acpi_fill_fadt()` should derive PMBASE at runtime and
fail closed unless it is the currently admitted `0501` decode.  Candidate
legacy block values are:

```text
PM1_EVT   0500, length 4
PM1_CNT   0504, length 2
PM_TMR    0508, length 4
GPE0      0520, length 16
PM2_CNT   0550, length 1 only after a read/semantic gate
SCI       IRQ 9
```

The generic extended-PM helper can mirror only the populated legacy fields.
Start with `iapc_boot_arch = LEGACY_DEVICES`; the `8042` bit must be clear.
Conservative initial feature flags are `WBINVD`, `C1_SUPPORTED` and the
vendor-correlated sleep-button bit.  Defer `S4_RTC_WAKE`, `PLATFORM_CLOCK` and
`C2_MP_SUPPORTED` until the corresponding sleep/timer/SMP behavior exists.

Because `NO_SMM` is retained, `smi_cmd`, `acpi_enable` and `acpi_disable` must
all remain zero.  `arch_fill_fadt()` already follows that rule when
`permanent_smi_handler()` is false.  Do not copy the vendor `b2/e1/1e`
contract: without its SMI handler it is nonfunctional and unsafe.

The vendor FADT advertises CF9 reset value `06`, and full CF9 reset has been
useful in ROMMON, but the current board still selects a missing board-reset
path.  The first table-only build should omit `RESET_REGISTER`.  Add
`SOUTHBRIDGE_INTEL_COMMON_RESET`/`HAVE_CF9_RESET` only as a separate accepted
reset milestone.

### SCI caveat: table-only is not OS-ready

Live `PM1_CNT=00000000`, so `SCI_EN` bit 0 is clear.  ACPI convention with
`SMI_CMD=0` tells an OS that firmware is already in ACPI mode; publishing that
combination while SCI remains disabled is incomplete.

Therefore the first ACPI image is explicitly a SeaBIOS/table-validation image,
not an OS-boot-ready ACPI implementation.  Before an OS boot attempt, choose
and prove one of these paths:

1. a native, exact-gated, pre-payload RMW that sets only `PM1_CNT.SCI_EN`,
   after PM/GPE status and enables have been audited and an IRQ-9 storm test is
   available; or
2. a real SMM ACPI-enable/disable service, which is a later architecture change
   and removes the present `NO_SMM` assumption.

No SCI, SMI, PM status or enable register should be changed in the table-only
milestone.

### MADT

Do not use `ACPI_COMMON_MADT_LAPIC` yet.  Its CPU generator walks enabled
`DEVICE_PATH_APIC` objects.  The current reduced tree has only a CPU-cluster
node and `SMP=n`, so the common path would emit zero processor entries.

A custom MADT for the current narrow target should emit:

1. one enabled LAPIC entry for processor 0 using the live BSP `lapicid()`;
2. one IOAPIC entry at `fec00000`, GSI base 0, after exact ID/version/count
   gates;
3. ISA IRQ 0 to GSI 2, edge/high (vendor encodes conforming flags `0000`;
   coreboot common code uses explicit edge/high); and
4. SCI IRQ 9 to GSI 9 with flags `000d` (level/high), matching MSI and the
   common PMBASE helper.

The IOAPIC entry describes topology; it does not prove delivery.  Generation
must restore IOREGSEL to zero.  Advertise PCAT compatibility because the
current PIC/virtual-wire path is retained, but do not change either path while
constructing the table.

Additional LAPIC records are forbidden until application processors are
actually started and represented.  The MSI template's twelve disabled CPU
records are runtime-patched scaffolding, not a CPU topology to copy.

### MCFG without changing coreboot PCI access

This port deliberately selects `NO_ECAM_MMCONF_SUPPORT` because its bounded
enumeration uses CF8/CFC and has platform-specific bus-routing constraints.
Removing that option would globally change coreboot's PCI config-access
implementation, not merely publish a table.

Current generic ACPI emits an empty MCFG header when ECAM access is disabled;
it emits allocation records only under `ECAM_MMCONF_SUPPORT`.  That is not the
desired result: hardware and vendor evidence support one allocation at
`e0000000`, segment 0, buses `00..ff`, while coreboot should continue using
CF8/CFC.

The clean solution is a small generic separation, reviewed independently:

```text
ACPI_PUBLISH_MCFG
  default ECAM_MMCONF_SUPPORT
  permits ECAM base/bus/length configuration for table publication
  does not compile or select MMIO PCI-config access
```

`acpi_create_mcfg()` and the `PNP0C02` ECAM reservation in `dsdt_top.asl`
would use the publication option; `pci_ops.c` and the PCFG OperationRegion
would remain gated by actual ECAM access.  The MSI board would select
publication, set base `e0000000`, 256 buses and length `10000000`, while
retaining `NO_ECAM_MMCONF_SUPPORT`.

Do not add a second board-specific MCFG alongside the generic empty one.
Before hardcoding the allocation, prefer one direct read of the full X58
PCIEXBAR register in addition to the already-proved ECAM identity and static
vendor MCFG.

### Minimal DSDT and generated PCI root resources

The first native DSDT should contain only:

- the common top-level definitions and `_PIC` selector;
- `_S0` and, only if deliberately accepted, `_S5` with the ICH10/vendor type;
- `\_SB.PCI0` as `PNP0A08`/`PNP0A03`, segment 0 and bus 0; and
- a `PNP0C02` device for admitted fixed reserved resources.

It should not include the broad ICH10 `ich10.asl`/`lpc.asl`, vendor AML,
`_PRT`, PS/2 devices, USB wake, PCIe hotplug, SMI methods, NVS contracts,
S1/S3/S4, or devices not initialized by this port.

Give the domain ACPI name `PCI0` and attach `pci_domain_fill_ssdt()`.  That
helper will derive the bus range and emit CF8/CFC plus current non-reserved
producer windows (`1000..ffff` and `c0000000..dfffffff`).  It intentionally
skips ECAM index 7 because that resource is reserved, so the separate
motherboard resource remains necessary.

No `_PRT` belongs in the first table.  Vendor DxxIP/DxxIR and direct-GSI maps
are valuable routing inputs, but current PIRQ bytes remain disabled and no
device interrupt has been delivered through the IOAPIC.

### HPET table

The isolated decode and counter tests have passed, so HPET may be included in
the first ACPI build if that build first reproduces and revalidates the exact
automatic state:

- enable HPTC with the exact masked field operation before table generation;
- reserve the actual selected `400`-byte aperture;
- verify the same live capability ID again in ramstage; and
- only then attach `acpi_write_hpet()` with a justified minimum tick.

An AML HPET device, if added, must report `_STA=0f` only when the exact HPTC
decode remains enabled and must expose the selected address.  It must not
enable the counter or legacy replacement as an AML side effect.

## Staged implementation and acceptance plan

### R0: resource-only

Sole change: create gated fixed reservations listed above, excluding HPET
because this phase performs no decode write, and without subtractive LPC
resources.

Acceptance:

- same PCI tree and exact BAR allocation as the preceding release;
- no new PCI/MMIO/I/O writes;
- zero resource overlaps;
- ECAM remains reserved once, not duplicated; and
- SeaBIOS/iPXE behavior remains unchanged.

### R1: IOAPIC decode-only

Sole change: automate exact `OIC=00/03 -> 03`, read back ID 0/version 20/count
24 and restore selector zero.  Add the IOAPIC fixed resource only in this
configuration.

Acceptance:

- exact PRE/TARGET/POST log;
- no IOWIN data write, no MRE lock, no redirection/PIRQ/SCI change;
- existing SeaBIOS timer progress remains; and
- ten cold plus ten warm runs eventually pass before broadening.

### R2: HPET automatic decode

The separate live HPTC decode/capability and counter run/stop experiments have
passed.  The next release may automate HPTC decode, repeat the capability gate,
leave the counter stopped, and add the fixed resource.  No interrupts.

Acceptance:

- exact `HPTC=80`, `GCAP_ID=0429b17f:8086a301` readback;
- general configuration, interrupt status and counter remain zero at the
  payload handoff unless a separately labelled diagnostic reruns the test;
- all comparator interrupt enables remain clear; and
- the fixed resource is exactly `fed00000..fed003ff`.

### R3: native table-only ACPI

Sole change: minimal DSDT, runtime-gated FADT, one-BSP custom MADT, PCI-root
SSDT/resources, and correctly decoupled MCFG publication.  HPET is optional
only if R2 passed.

Acceptance:

- SeaBIOS reports a nonzero RSDP;
- `iasl`, `acpidump`/`acpixtract` and checksum validation pass;
- MADT has exactly one enabled LAPIC, IOAPIC ID 0/count 24, and two intended
  overrides;
- no 8042, `_PRT`, SMI command, false SMP record or duplicate MCFG/resource;
- PMBASE values are `0500`-relative, not copied vendor `0800` values; and
- image is labelled **table-validation only, not OS-ready**, because SCI_EN
  remains zero.

### R4: one-route interrupt proof

After the redirection census, modify and restore only one IOAPIC entry for a
bounded timer-delivery experiment.  Then prove SCI/GSI9 separately with status
and enable audit.  Only after these tests should `_PRT`, PIRQ routing or an OS
ACPI boot be attempted.

### R5: later completeness

Add, in separate milestones:

- native SCI enable or SMM handoff;
- board-correct `_PRT` and PIRQ routes;
- application-processor startup and CPU namespace/table entries;
- USB/PCIe wake and power methods;
- CF9 reset advertisement;
- tested sleep states; and
- 8042/PS2 declarations only after command `AB` returns success and real
  keyboard input works.

## Immediate recommendation

Add the IOAPIC redirection read-only census; HPET decode and run/stop are now
hardware-proved.  In source, the next safe patch is resource-only, followed by
exact-gated `OIC=03` and `HPTC=80` automation that leaves all routing and the
HPET counter untouched at handoff.  Native ACPI can then be introduced as a
deliberately table-only milestone while SCI, PIRQ and SMM remain untouched.
