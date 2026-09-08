> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VP CAR 8259, ELCR and TCO census

Date: 2026-09-06

## Result

The retained B06VP CAR state has no safe legacy-interrupt baseline yet.  Both
8259 interrupt-mask bytes are `00`, so all sixteen legacy IRQ inputs are
unmasked, including SCI IRQ9.  Both ELCR bytes are also `00`, so IRQ9 has not
been selected as level-triggered.  This is consistent with the experimental
board path calling only `setup_lapic_interrupts()`: that establishes BSP LAPIC
virtual-wire mode but does not initialize the two 8259 controllers.

The ICH10 TCO watchdog is also not quiescent:

| Register | Observed | Established meaning |
|---|---:|---|
| `GCS` at RCBA `+3410h` | `00200404` | `NR` bit 5 is clear, so the no-reboot override is not selected |
| `TCO1_STS` | `0008` | `TIMEOUT` history is set |
| `TCO2_STS` | `0002` | `SECOND_TO_STS` history is set |
| `TCO1_CNT` | `0000` | `TCO_TMR_HLT` bit 11 and `TCO_LOCK` bit 12 are clear |
| `TCO2_CNT` | `0008` | reset-default GPIO11 alert-disable state |
| `TCO_TMR` | `0004` | four-count initial value |
| `TCO_RLD` samples | `0004`, `0002`, `0004` | the current value changes and cycles while halt is clear |

No status bit was acknowledged.  The retained session cannot prove when the
two timeout bits were first set or which earlier reset produced them.  In
particular, their presence plus `GCS.NR=0` is not by itself a license to infer
why the board remained in ROMMON.

## Reversible halt proof

A separately sealed table admitted only the exact tuple above, wrote
`TCO1_CNT=0800`, and required exact readback.  Three `TCO_RLD` observations
across two bounded `0x00100000`-iteration delays were all `0002`.  Thus setting
only `TCO_TMR_HLT` demonstrably freezes the timer on this target.

The host executor then rolled the one reversible mutation back.  ROMMON
reported the saved value `0000`, exact rollback readback `0000`,
`ROLLBACK=ok`, and a final successful discard.  A following independent
`script status` showed `TXN_VALID=00`.  The target therefore ended in its exact
pre-test TCO control state.

## Consequences for platform initialization

The smallest defensible progression is:

1. Add an early, fail-closed TCO halt stage that accepts only the measured
   `TCO1_CNT=0000` pre-state or exact `0800` retained state, sets only bit 11,
   and requires complete-word readback.  Log `TCO1_STS`, `TCO2_STS`, `GCS`,
   and `TCO_RLD` before the write.  Do not set `TCO_LOCK`.
2. Keep the timeout-status clear as a later, separately reviewable W1C stage.
   The first halt image should preserve the evidence rather than call the
   historical broad status-clear helper.
3. Before any SCI enable, initialize the 8259 pair with the existing
   `setup_i8259()` primitive or an equivalently exact isolated stage.  Its
   documented post-state is slave mask `ff`, master mask `fb` (only cascade
   IRQ2 open).  Set only ELCR IRQ9 to level-triggered after that initialization.
4. Keep IOAPIC GSI9 masked and all PM/GPE enables zero until a real SCI handler,
   acknowledgement and EOI path exists.  Merely setting LAPIC LINT0 ExtINT is
   insufficient.

TCO halt and 8259 initialization are different hypotheses and should remain
separate release stages.  Neither operation fixes the independently observed
USB over-current/VBUS condition.

## Evidence

The complete inventory, hashes, operation counts, observations and cleanup
state are in
[`captures/2026-09-06-b06vp-car-pic-elcr-tco.metadata.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
The register meanings come from Intel ICH10 Family Datasheet `319973-003`,
sections 10.1.75 and 13.9, and from the local coreboot implementations in
`src/drivers/pc80/pc/i8259.c` and
`src/southbridge/intel/i82801jx/early_init.c`.
