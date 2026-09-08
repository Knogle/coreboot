> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WB deterministic ICH10 TCO-halt stage

Date: 2026-09-06

Status: **build-only; no B06WB target execution is claimed; do not flash until
B06VY, B06VZ and B06WA each have a preserved target-hardware PASS**.

## Purpose and one-delta rule

B06WB is a separately selectable successor to B06WA.  It retains the complete
B06WA boot and diagnostic path and adds one hardware hypothesis: halt the
ICH10R TCO watchdog before the later ramstage platform mutations, without
acknowledging its timeout history or changing any reset policy.

The immediate evidence is the read-only census and reversible experiment in
[`b06vp-car-pic-tco-census-2026-09-06.md`](../research/msi/b06vp-car-pic-tco-census-2026-09-06.md)
and its immutable
[`metadata`](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index):

- LPC `00:1f.0` was `8086:3a16`, RCBA was `fed1c001`, PMBASE was `00000501`
  and ACPI decode control was `80`;
- `GCS=00200404`, `TCO1_STS=0008`, `TCO2_STS=0002`,
  `TCO1_CNT=0000`, `TCO2_CNT=0008` and `TCO_TMR=0004` were observed;
- the running `TCO_RLD` samples changed, proving the watchdog was not
  quiescent; and
- the exact reversible transition `TCO1_CNT 0000 -> 0800` froze all three
  bounded reload-register samples, after which rollback restored `0000`.

The register interpretation comes from Intel ICH10 Family Datasheet
319973-003 sections 10.1.75 and 13.9.  The existing broad
`src/southbridge/intel/i82801jx/early_init.c` and
`src/southbridge/intel/common/watchdog.c` paths are explicitly not used:
besides halting TCO, those paths set `GCS.NR`, alter LPC command policy, or
clear write-one-to-clear timeout status.

## Placement and fail-closed preflight

`b06wb_halt_tco_once()` runs in the B06VN domain pre-scan path after the
read-only platform-state and static-topology gates, but before:

1. the six-field ICH10 baseline;
2. SATA route, PCS/SCLK and AHCI MMIO setup;
3. deterministic IOAPIC decode/masking;
4. quiescent HPET decode; and
5. raw PCI preflight or enumeration.

Before its only possible TCO write, the routine requires:

```text
LPC identity             3a168086 exactly
RCBA                     fed1c001 exactly
PMBASE                   00000501 exactly
ACPI_CNTL                 80 exactly
TCO1_CNT                  0000 or 0800 exactly
prior B06WB attempt       none
later platform attempt    none
```

Every other complete `TCO1_CNT` value fails closed.  Thus B06WB does not
silently retain `TCO_LOCK`, NMI policy, reserved bits or any unknown control
state.  “Before the first write” refers to the first experimental chipset
write; the retained UART and port-80 diagnostics remain active.

## Sole hardware mutation and exact postcondition

When the admitted prestate is `0000`, B06WB constructs the 16-bit target with
a masked read-modify-write expression that clears and then sets only
`TCO_TMR_HLT` bit 11.  It verifies that the constructed complete word is
exactly `0800`, performs one `outw`, and requires complete-word `0800`
readback.  With retained prestate `0800`, it performs no TCO write but repeats
all gates.

Before and after the optional write, B06WB logs the complete values of:

```text
GCS TCO_RLD TCO1_STS TCO2_STS TCO1_CNT TCO2_CNT TCO_TMR
```

It requires `TCO1_STS` and `TCO2_STS` to be bit-for-bit unchanged, thereby
preserving all status evidence.  It also requires `GCS`, `TCO2_CNT` and
`TCO_TMR` unchanged.  `TCO_RLD` is logged but deliberately not compared: it
can count between the PRE read and the halt write.  Immediately before the
ready flag, B06WB rereads the identity and decode tuple, both status words,
`GCS`, the complete `TCO1_CNT`, `TCO2_CNT` and `TCO_TMR`.

There is no automatic rollback after an admitted `0000 -> 0800` write.  If a
post-write gate fails, the system halts at the first failure with the watchdog
potentially stopped.  This is the intended safe failure state; a second
unverified mutation would obscure the evidence.

## Expected diagnostics

The B06WB POST codes are:

| Code | Meaning |
|---:|---|
| `79` | exact preflight passed; optional halt transition begins |
| `7a` | complete final TCO gate passed |
| `7b` | fail-closed B06WB stop |

An admitted fresh path should contain:

```text
[TCO] B06WB DECODE LPC_ID=3a168086 RCBA=fed1c001 PMBASE=00000501 ACPI_CNTL=80 ALLOWLIST=0000,0800
[TCO] B06WB PRE GCS=... TCO_RLD=... TCO1_STS=... TCO2_STS=... TCO1_CNT=0000 TCO2_CNT=... TCO_TMR=... WRITE=0
[TCO] B06WB POST GCS=... TCO_RLD=... TCO1_STS=... TCO2_STS=... TCO1_CNT=0800 TCO2_CNT=... TCO_TMR=... WRITE=1
[TCO] B06WB READY TCO1_CNT=0800 STATUS_PRESERVED=1 GCS_WRITE=0 STATUS_WRITE=0 RELOAD_WRITE=0 TIMER_WRITE=0 TCO_LOCK_WRITE=0
```

An admitted retained path has PRE `TCO1_CNT=0800` and POST `WRITE=0`.

## Explicit omissions

B06WB does not:

- set `GCS.NR` or `TCO_LOCK`;
- write `TCO_RLD`, `TCO_TMR`, `TCO1_STS`, `TCO2_STS` or `TCO2_CNT`;
- reload the watchdog, select another timeout, or clear historical status;
- change PM1, SMI, GPE, SCI, 8259 PIC, ELCR, IOAPIC or LAPIC policy;
- add any HPET access beyond the inherited later B06WA gate;
- add USB, SATA, GPIO, ACPI or SMBIOS behavior; or
- enable the generic ICH10 device model or common watchdog helper.

The B06WA domain resources 0 through 15, allocator apertures, 13-function
devicetree, SeaBIOS deferred-USB trace, Radeon physical-VBIOS path and RTL8168
iPXE option ROM remain unchanged.  The B06WB resource callback only requires
the TCO ready flag and delegates to B06WA's exact resource construction and
audits.

The deterministic build gate explicitly requires
`CONFIG_SOUTHBRIDGE_INTEL_COMMON_WATCHDOG=n`,
`CONFIG_USE_WATCHDOG_ON_BOOT=n`, `CONFIG_IOAPIC=n`,
`CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n`,
`CONFIG_HAVE_ACPI_TABLES=n` and the broad
`CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n`.

## Hardware gate and recovery

A successful build is not a hardware PASS.  Do not use the B06WB W25Q128
image until separate preserved target logs establish all three prerequisites:

1. B06VY deterministic IOAPIC `READY` and continued payload execution;
2. B06VZ exact resource audits and continued payload execution; and
3. B06WA quiescent HPET `READY`, resource audit and continued payload
   execution.

Then retain known-good B06WA and vendor chips/images, capture COM1 at 115200
8N1 from power application, and perform one cold boot.  Preserve the full
PRE/POST/READY lines and all inherited payload milestones.  On POST `7b`,
`3c` or `3d`, restore B06WA and diagnose the first exact gate rather than
broadening the allowlist.  The socketed flash and externally verified vendor
backup remain the recovery mechanism.

## Build and validation

```bash
./scripts/build_x58_b06wb.sh
```

The wrapper performs two clean, fixed-epoch coreboot/SeaBIOS/iPXE builds, runs
the complete repository test suite, requires byte identity, audits the exact
payload/option-ROM configuration, and emits the public 4-MiB base plus local
user-firmware composites.  Artifact hashes and completed validation counts are
recorded separately in the B06WB manifest after the deterministic build.
