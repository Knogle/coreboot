> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WC integrated experimental platform image

Date: 2026-09-06

Status: **integrated experimental successor; two clean builds are
byte-identical and all source, host, IASL/AML, CBFS, composite and W25Q128
release gates pass; no B06WC target execution is claimed until a complete
serial log is archived**.

## Purpose

B06WC deliberately changes the release strategy from one separately qualified
hardware delta per image to one aggressively integrated, observable image.  It
retains the socketed-flash recovery requirement and all exact fail-closed
gates, but does not require separate B06VY, B06VZ, B06WA and B06WB target
qualification before testing.

The image preserves the already successful vendor-assisted DDR3/QPI handoff,
selective X58 PCI enumeration, AMD Radeon HD 5450 physical VBIOS execution,
SeaBIOS and RTL8168 iPXE path.  It directly combines:

1. corrected ICH10R AHCI routing, all-six-port PCS/SCLK policy and minimal
   AHCI MMIO enablement;
2. deterministic masked IOAPIC state, fixed resource reporting, quiescent HPET
   decode and the exact TCO watchdog halt;
3. automatic standard 8259 setup at vectors `20h/28h`, conservative masks and
   level-triggered SCI/IRQ9 in the ELCR;
4. a quiet native ACPI-mode transition plus minimal FADT, MADT, MCFG and DSDT;
5. read-only EHCI/UHCI admission and port-state classification before payload;
6. the explicitly unlocked ROMMON PIT-to-PIC-to-LAPIC ExtINT diagnostic.

This is intentionally a faster experimental convergence image, not a claim
that every integrated facility is already stable or OS-ready.

## Ordered ramstage contract

The normal path runs the new work in this order:

```text
exact platform/static-topology gate
  -> TCO1_CNT halt
  -> 8259 vectors/masks + ELCR IRQ9 level
  -> six-field ICH10 baseline
  -> AHCI function route
  -> IOAPIC decode and mask all 24 entries
  -> quiescent HPET decode
  -> quiet ACPI-mode transition
  -> strict raw PCI preflight
  -> IOHBUSNO and EHCI/UHCI controller setup
  -> selective PCI scan/resource allocation
  -> PCS/SCLK and AHCI MMIO enablement
  -> read-only USB admission
  -> ACPI/SMBIOS/coreboot tables
  -> SeaBIOS -> Radeon VBIOS -> RTL8168 iPXE
```

Every state-changing step has a unique ready flag.  A later stage cannot run
merely because an earlier function returned; it requires the exact final
state again where that state is readable.

## PIC and quiet ACPI mode

The legacy interrupt prestate is admitted only as either the observed reset
tuple or B06WC's complete retained target:

```text
reset:   IMR 00/00, ELCR 00/00
target:  IMR fb/ff, ELCR 00/02
```

CPU `EFLAGS.IF` must be clear.  B06WC replays coreboot's standard 8259 ICW
sequence so vectors `20h/28h` are established, masks IRQ0 and every slave IRQ,
leaves only the master cascade open, and makes only IRQ9 level-triggered.  It
does not issue an EOI or enable CPU interrupts.

After IOAPIC and HPET setup, the ACPI-mode stage additionally requires:

```text
LPC ID / RCBA / PMBASE / ACPI_CNTL   exact known tuple
PM1_EN                               0000
GPE0_EN low/high                     00000000 / 00000000
SMI_EN                               00000000
ALT_GP_SMI_EN                        0000
GPIO_ROUT                            00000000
UPRWC                                0000
PIC IRQ9                             masked
IOAPIC entry 9                       low=00010000, high=00000000
PM1_CNT                              full dword 00000000 or 00000001
EFLAGS.IF                            clear
```

If `PM1_STS.PRBTNOR_STS` is pending, B06WC acknowledges exactly that W1C bit
with `outw(0800, 0500)` and no other status bit.  It then performs a 16-bit
RMW of `PM1_CNT`, setting only `SCI_EN`, and requires full-dword `00000001`
readback.  All event enables and both IRQ9 delivery paths must still be quiet.

This lets the FADT truthfully use `SMI_CMD=0`: ACPI mode has already been
entered by firmware, while no SMI handler or SCI source is enabled.  It does
not prove SCI delivery.

## Published ACPI scope

B06WC publishes only the measured or explicitly established minimum:

- one enabled BSP Local APIC, APIC ID 0;
- ICH10R IOAPIC ID 0 at `fec00000`, GSI base 0;
- ISA IRQ9 to GSI9, level/high SCI override;
- PM1 event/control and PM timer blocks at PMBASE `0500`;
- MCFG allocation `e0000000`, segment 0, buses `00-ff`;
- one PCI0 host bridge and the proven PCI I/O/MMIO apertures;
- reservations for SMBus, PM, GPIO, ECAM, IOAPIC, HPET aperture, RCBA, LAPIC
  and the top 16-MiB flash window.

Coreboot itself continues to use CF8/CFC.  Publishing MCFG does not enable the
generic coreboot ECAM access path.

The first table set intentionally omits `_PRT`, S3/S4/S5, `_PRW`, `_PTS`,
`_WAK`, an HPET table/device, an 8042 claim, ACPI reset register, GPE blocks,
SMM enable/disable commands and application processors.  Those omissions are
capability boundaries, not accidental gaps hidden from the log.

## USB admission

The USB admission pass does not write a GPIO, over-current mux, PPO, UPRWC,
EHCI/UHCI control register or PORTSC.  It requires the exact eight ICH10R USB
functions, valid assigned BARs, expected class/header/command state, native
over-current routing and the measured EHCI capability tuple.  It then records
EHCI and UHCI connect, port-enable, over-current-active and over-current-change
bitmaps and classifies the observation as one of:

```text
CONNECT_OC
CONNECT
OC_NO_CONNECT
IDLE_NO_CONNECT
```

This gives the next USB/VBUS experiment a deterministic input without turning
an electrical over-current indication into an inferred board GPIO policy.

## Diagnostic ROMMON path

The normal B06WC boot never runs the destructive interrupt probe.  If an early
payload interrupt failure must be isolated, enter ROMMON before handoff and
explicitly run:

```text
unlock WRITE
irqprobe pit
```

The probe installs a temporary CAR IDT, generates a bounded PIT channel-0
one-shot, opens only IRQ0 through the PIC/LAPIC ExtINT path and accepts exactly
one vector-20 handler entry.  Cleanup masks IRQ0 before PIC EOI and restores
the IDTR, PIC masks, LAPIC LVT0 and SVR.  PIT mode/count cannot be restored, so
a cold reset is required afterward even on success.

## POST contract

The new B06WC-specific codes are:

| Code | Meaning |
|---:|---|
| `7c` | standard 8259/ELCR transition begins |
| `7d` | exact PIC/ELCR target reached |
| `7e` | fail-closed PIC/ELCR rejection |
| `7f` | quiet ACPI-mode transition begins |
| `80` | PRBTNOR status is clear; all sources/routes still quiet |
| `81` | exact `PM1_CNT=00000001` and final quiet-state gate passed |
| `82` | fail-closed ACPI-mode rejection |
| `83` | read-only USB admission begins |
| `84` | USB topology/controller census accepted |
| `85` | fail-closed USB admission rejection |
| `86` | ACPI table-publication runtime gate rejected |

The inherited TCO codes immediately before these are `79` begin, `7a` ready
and `7b` failure.  Earlier SATA, IOAPIC, HPET, PCI and payload codes retain
their documented meanings.

## First hardware run

Capture COM1 `03f8` at 115200 8N1 before applying power and retain the complete
log.  The first test should remain the known E5645, sole SPD `0x54`, Radeon HD
5450 and RTL8168 configuration.  A successful experimental run should show:

1. inherited DDR3/QPI handoff and ramstage entry;
2. `[TCO] ... READY`, `[PIC] ... READY`, `[IOAPIC] ... READY`,
   `[HPET] ... READY` and `[ACPI-MODE] ... READY`;
3. exact PCI/resource/AHCI final audits;
4. `[USB-ADMIT] ... READY` with the real port classification;
5. ACPI FADT/MADT/MCFG log lines without POST `86`;
6. SeaBIOS video through the card's physical VBIOS; and
7. RTL8168 iPXE device initialization and DHCP/TFTP progress.

Any terminal `7e`, `82`, `85` or `86` is useful evidence.  Preserve the full
PRE line and the first rejection reason; do not broaden an allowlist from a
POST code alone.  The socketed known-good chip remains the recovery path.

## Reproducible build

```bash
./scripts/build_x58_b06wc.sh
```

The release wrapper performs two clean fixed-epoch builds, checks byte
identity, compiles the DSDT with IASL, runs the full repository test suite,
audits the pinned SeaBIOS/iPXE and CBFS contents, composes the local
vendor-assisted image from the user's hash-pinned MSI input, verifies the
deterministic wrapper and places the 4-MiB firmware at the top of a 16-MiB
W25Q128 image.  Final hashes are recorded in the separate B06WC manifest.

The released flash artifact is
`blobs-local/msi-x58-pro-e/b06wc/msi-x58-pro-e-b06wc-deterministic-w25q128-16MiB.rom`
with SHA-256
`c7482e03098f1e9bb8f845401d1bdc33e97a1b131ceffd9bd9fc27e089254831`.
