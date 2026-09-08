> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VP CAR HPET-to-IOAPIC delivery experiment

## Result

The first direct timer-delivery experiment produced a useful negative result.
ICH10 HPET timer 0 was programmed as an edge-triggered one-shot on route 20,
the corresponding IOAPIC entry was temporarily routed to BSP LAPIC vector
`0x51`, and the main counter advanced from zero to `0x00c1b628`, well past the
`0x00100000` comparator.  The matching LAPIC IRR bit nevertheless remained
zero.

This rejects **only** interrupt delivery in the retained B06VP pre-QPI CAR
state.  It is not evidence that the HPET, IOAPIC, or LAPIC is defective.  The
next meaningful repetition belongs after the known DDR/QPI/X58/ICH10 handoff,
where the DMI/IOH interrupt path is in its payload-facing state.

## Documentation boundary

The register model follows Intel ICH10 Datasheet document `319973-003`:

- consumer `OIC` at RCBA `0x31ff` controls IOAPIC decode;
- the HPET aperture is 1024 bytes and its address/enable is selected by RCBA
  `HPTC` at `0x3404`;
- `GEN_CONF.ENABLE_CNF` starts/stops the main counter;
- timer configuration bit 2 enables interrupt generation and bits 13:9 select
  a route allowed by the read-only route-capability field.

The live HPET identity was `0429b17f:8086a301`.  This differs by one low-ID
bit from the printed datasheet reset example and is therefore treated as a
measured target identity, not normalized to the document's literal.

The IOAPIC only defines indirect registers `0x00`, `0x01`, and redirection
entries `0x10..0x3f`.  Selector `0x03` is reserved on ICH10.  Its observed
`00170020` return must not be interpreted as a boot-configuration register or
written by a generic IOAPIC helper.

## Staged execution

The experiment was deliberately split so every dangerous step had a known
precondition and cleanup path:

1. The read-only route census established IOAPIC ID `0`, version `00170020`,
   a fully masked entry 20 (`low=00010197`, `high=10000000`), BSP APIC ID `0`,
   LAPIC version `01060015`, `SVR=000000ff`, and empty IRR banks.
2. The arm table kept the entry masked while setting destination APIC ID `0`,
   vector `0x51`, edge/fixed delivery, `SVR=0000010f`, and comparator
   `0x00100000`.  The HPET and timer interrupt stayed disabled.
3. The trigger table selected route 20 (`TIM0_CONF=00002834`), unmasked only
   entry 20, enabled the HPET, waited a bounded interval, captured the main
   counter and LAPIC IRR, then stopped the counter, disabled timer interrupts,
   and re-masked the entry **before** its deliberate IRR assertion.
4. The assertion failed because IRR bank 2 was `00000000`.  The counter value
   proves this was not merely a comparator that had not elapsed.
5. A separate exact restore table restored the entry-20 low/high tuple, LAPIC
   SVR, timer configuration, comparator, counter, and interrupt status.
6. A final decode-restore table disabled HPTC and OIC.  An independent
   read-only run then observed `OIC=00`, `HPTC=00000000`, both apertures as
   open bus, `SVR=000000ff`, and no open ROMMON transaction.

The immutable inventory and every digest are recorded in
[`captures/2026-09-06-b06vp-car-hpet-ioapic20-delivery.metadata.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## Consequences for the next images

- A quiescent HPET decode/resource stage is safe to isolate next: validate the
  live ID, leave `GEN_CONF=0`, leave every timer interrupt disabled, and reserve
  `fed00000..fed003ff`.
- Do not publish an HPET ACPI table until that post-QPI stage passes on target.
- Do not claim working interrupts from an IOAPIC identity/readback test alone.
- A second delivery test needs a post-QPI diagnostic build.  If it accepts an
  interrupt instead of merely polling IRR, it also needs a real IDT handler,
  source acknowledgement, EOI, bounded timeout, and an exact cleanup log.
- SCI, SMI and PIRQ enablement remain independent milestones.

## Additional USB routing result

The immediately following read-only USB census measured the PCI interrupt pins
as D26 `A/B/C/C` and D29 `A/B/C/A`.  Combining those measured pins with MSI's
vendor `D26IR=3250` and `D29IR=0237` yields:

| Function | Pin | Direct APIC GSI |
|---|---:|---:|
| `00:1a.0` | A | 16 |
| `00:1a.1` | B | 21 |
| `00:1a.2` | C | 18 |
| `00:1a.7` | C | 18 |
| `00:1d.0` | A | 23 |
| `00:1d.1` | B | 19 |
| `00:1d.2` | C | 18 |
| `00:1d.7` | A | 23 |

The current B06VP route is still the generic linear `3210` for both devices;
all legacy PIRQ bytes remain disabled at `80`.  A later MSI-routing stage can
therefore change exactly two RCBA words while keeping the corresponding
IOAPIC entries masked.  This routing work is required for OS INTx correctness,
but it is not the explanation for a SeaBIOS USB keyboard failing in a polling
path.
