> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VZ fixed platform-resource stage

Date: 2026-09-06

Status: **build-only; built twice byte-identically and statically verified; do
not flash until B06VY has a recorded target-hardware PASS**.

## Purpose and one-delta rule

B06VZ is a separately selectable successor to B06VY.  Its only functional
delta is resource bookkeeping: it describes seven already established fixed
platform ranges on the PCI domain.  It does not program a decode, device,
interrupt route, timer, USB controller or Super I/O function.

The stage deliberately retains all of B06VY's hardware behavior and payload
inputs:

- the deterministic, initially masked ICH10R IOAPIC state;
- the exact B06VX/B06VW SATA and USB path;
- the same 13-function devicetree topology;
- the same fixed PCI allocation apertures;
- SeaBIOS revision `5497f43189374647b3b0f282aa497e71c891f3c5` with
  the deferred USB journal; and
- the same RTL8168 iPXE ROM.

To keep "no SMBIOS delta" literal rather than merely architectural, B06VZ
also retains B06VY's `CONFIG_MAINBOARD_PART_NUMBER` and `CONFIG_LOCALVERSION`.
Those values feed generic SMBIOS product and BIOS-version strings.  The new
build is still unambiguously identified by its B06VZ bootblock, romstage and
ramstage banners, config filename and artifact filename.

This stage is intentionally not an ACPI or interrupt-delivery experiment.  It
must remain attributable to resource metadata alone.

## Exact domain-resource contract

Resources 0 through 7 are inherited and are now checked for exact bounds and
flags as part of the same contract.  Resources 8 through 14 are the only new
entries.

| Index | Kind | Half-open range | Exact semantic flags |
|---:|---|---|---|
| 0 | low RAM | `00000000-000a0000` | assigned, fixed, memory, cacheable, stored |
| 1 | VGA MMIO hole | `000a0000-000c0000` | assigned, fixed, memory, reserve, stored |
| 2 | legacy reserved RAM | `000c0000-00100000` | assigned, fixed, memory, cacheable, reserve, stored |
| 3 | low RAM | `00100000-c0000000` | assigned, fixed, memory, cacheable, stored |
| 4 | remapped RAM | `100000000-140000000` | assigned, fixed, memory, cacheable, stored |
| 5 | PCI I/O allocator window | `1000-10000` | assigned, I/O, bridge |
| 6 | PCI MMIO allocator window | `c0000000-e0000000` | assigned, memory, bridge |
| 7 | PCIEXBAR/ECAM | `e0000000-f0000000` | assigned, fixed, memory, reserve, stored |
| 8 | SMBus I/O | `0400-0420` | assigned, fixed, I/O, reserve |
| 9 | ACPI PM I/O | `0500-0580` | assigned, fixed, I/O, reserve |
| 10 | GPIO I/O | `0580-05c0` | assigned, fixed, I/O, reserve |
| 11 | ICH10R IOAPIC | `fec00000-fec01000` | assigned, fixed, memory, reserve, stored |
| 12 | ICH10R RCBA | `fed1c000-fed20000` | assigned, fixed, memory, reserve, stored |
| 13 | BSP LAPIC | `fee00000-fee01000` | assigned, fixed, memory, reserve, stored |
| 14 | top-of-address-space flash | `ff000000-100000000` | assigned, fixed, memory, reserve, stored |

The non-fixed allocator windows use coreboot bridge-resource semantics: their
available aperture is represented by exact `base` and `limit`, while `size`
remains zero.  Fixed entries use exact `base` and `size`.  B06VZ checks these
different representations explicitly instead of treating a producer window as
a fixed reservation.

## Runtime audit

The resource audit runs at the existing `BS_DEV_RESOURCES` exit hook, before
the inherited SATA MMIO handoff.  It requires:

1. the B06VY IOAPIC stage to have executed and returned successfully before
   the new resource reader is entered;
2. exactly 15 domain-resource objects, with no additional or missing index;
3. exact index, base, fixed-size or bridge-limit representation, and complete
   flag equality for every entry 0 through 14;
4. pairwise non-overlap among every domain range of the same resource type;
5. the inherited exact endpoint-BAR aperture checks;
6. the inherited pairwise endpoint-BAR non-overlap check; and
7. no overlap between any new fixed entry 8 through 14 and an assigned leaf
   BAR.

The compile-time assertions independently require the three fixed I/O ranges
to end below the unchanged PCI I/O aperture, ECAM to end before IOAPIC, each
high fixed MMIO range to remain disjoint, and the 16-MiB ROM window to end
exactly where remapped RAM begins.

An exact-resource mismatch stops at the inherited resource-failure POST code
`3c`; an overlap stops at `3d`.  A passing run prints:

```text
[RESOURCE] B06VZ fixed reservations 8..14 installed; allocator apertures unchanged
[RESOURCE] B06VZ exact resources 0..14 and non-overlap audit PASS
```

These messages prove only the in-memory coreboot resource model and its audit.
They do not prove ACPI correctness, interrupt delivery, USB input, SATA media
access or an operating-system boot.

## Deliberate omissions

B06VZ adds none of the following:

- HPET resource, HPTC decode change or HPET table;
- ACPI table generation, including MADT, FADT, DSDT or MCFG;
- a new SMBIOS producer or any change to B06VY's generic SMBIOS inputs;
- IOAPIC redirection, ExtINT, PIRQ, SCI or PIC policy;
- PM1/SCI enablement or power-management event clearing;
- USB legacy ownership, USB routing, over-current GPIO or 8042 policy;
- another CPU, AP startup or SMP declaration;
- a D31:F0 LPC device model or the broad historical ICH10 driver; or
- any additional PCI-configuration, port-I/O, MMIO or MSR write.

The resource helper functions update coreboot's in-memory device/resource
objects and can emit normal serial diagnostics.  They are not hardware decode
writes.

The reason for excluding HPET is especially important: reserving or publishing
an HPET before its selected base, decode and counter behavior are proved would
turn a truthful resource-only stage into an unverified timer claim.  The ACPI
work similarly waits for a demonstrated interrupt path.  The detailed staging
decision is recorded in
[`research/msi/post-b06vy-minimal-acpi-smbios-resource-audit-2026-09-06.md`](../research/msi/post-b06vy-minimal-acpi-smbios-resource-audit-2026-09-06.md).

## Flash gate and target procedure

Do not use the B06VZ W25Q128 image until the immediately preceding B06VY image
has a preserved target log showing its complete IOAPIC census, final selector
restore, `READY` line and continued SeaBIOS/iPXE path.  A build PASS is not a
B06VY hardware PASS.

Once that prerequisite exists:

1. retain the already known-good B06VY chip/image and verified vendor backup;
2. start COM1 capture at 115200 8N1 before applying power;
3. use the documented socketed W25Q128 recovery configuration;
4. cold boot B06VZ once and preserve the complete log;
5. require both B06VZ resource lines and the inherited B06VY, PCI, SeaBIOS and
   iPXE milestones; and
6. if POST `3c` or `3d` appears, restore B06VY and analyze the resource audit
   rather than bypassing it.

No hardware run is recorded by this document.  The project validation policy
still requires repeated cold boots before treating the stage as established.

## Build

```bash
./scripts/build_x58_b06vz.sh
```

The wrapper calls the shared deterministic successor builder.  It performs two
clean fixed-epoch builds, runs the complete repository test suite between the
builds, verifies byte identity, checks the inherited payload contracts, and
creates the public 4-MiB base plus locally composed 4-MiB and W25Q128 16-MiB
images.  The local composites continue to require the user's own hash-pinned
MSI image and are not distributable repository inputs.
