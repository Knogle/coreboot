> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VP CAR 8259 initialization and PIT IRQ0 proof

Date: 2026-09-06

## Result

Three isolated stages completed on the retained B06VP CAR ROMMON target:

1. the standard dual-8259 initialization programmed vectors `20h/28h`, left
   only the master cascade open (`IMR=fb/ff`), and selected level trigger for
   the board's SCI IRQ9 (`ELCR=00/02`);
2. a read-only census confirmed an enabled BSP xAPIC at `fee00000`, but a
   software-disabled LAPIC (`SVR=ff`) with masked LINT0/LINT1 and no sampled
   ISR/TMR/IRR bits; and
3. with IRQ0 still masked, PIT channel 0 mode 0/count `0800` changed master-PIC
   IRR0 from zero to one. The bounded poll succeeded on its first read.

The third stage is a real hardware delivery proof for:

```text
PIT channel 0 -> IRQ0 edge -> master 8259 IRR0
```

It is not a proof that the CPU accepted a virtual-wire ExtINT. The CAR monitor
keeps `EFLAGS.IF=0` and has no command for installing an IDT gate. Moreover,
ExtINT delivery bypasses LAPIC IRR and ISR, so polling `fee00210` for vector
`20h` would be the wrong success criterion.

## Exact observed state

Before PIT programming, after the separately proven 8259/ELCR sequence:

```text
PIC IMR             fb / ff
ELCR                00 / 02
IA32_APIC_BASE      00000000:fee00900
LAPIC TPR           00000000
LAPIC SVR           000000ff
LAPIC LVT0/LVT1     00010000 / 00010000
sampled ISR/TMR/IRR zero
master PIC IRR      00
slave PIC IRR       02
```

The pending slave IRR bit corresponds to the still-masked IRQ9 path; neither
SCI nor any PM event source was enabled by this experiment.

The PIT probe programmed only the stateful PIT command/count bytes and PIC
OCW3 IRR selection. It never unmasked IRQ0, executed `STI`, changed the LAPIC,
enabled SCI, or sent an EOI. The resulting master IRR value was exactly `01`.

## Cleanup and proof boundary

Both PICs were then reinitialized with the same ICW sequence. This cleared
master IRR0 and restored `IMR=fb/ff`; `ELCR=00/02` and the disabled/masked
LAPIC state remained exact. The slave's masked IRQ9 request remained visible
as IRR `02`, as expected because this test did not acknowledge any PM status.

The mode-0 PIT output is quiescent after terminal count, but PIT mode/count
cannot be read back and reconstructed exactly. A cold reset remains mandatory
before another test whose admission requires exact PIT reset state.

All scripts and transcripts are indexed with byte counts and SHA-256 values in
the [capture metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## Concrete implementation enabled by this proof

Two changes can now be kept separate:

- an automatic, exact-gated pre-payload 8259/ELCR baseline using the proven
  vectors, masks and IRQ9 trigger mode; and
- a default-off `irqprobe pit` ROMMON command for the remaining CPU-delivery
  proof.

The compiled probe needs a temporary CAR IDT with a vector-`20h` interrupt
gate, a very short bounded `IF=1` window, handler telemetry, PIC ISR0 capture
before the master-PIC EOI, and one unconditional cleanup path. It must not send
a LAPIC EOI for ExtINT. Success is one handler entry plus PIC ISR0 bit 0 set;
LAPIC IRR is explicitly not part of the acceptance condition.
