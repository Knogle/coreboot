> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VY deterministic ICH10R IOAPIC mask stage

Date: 2026-09-06

Status: built twice byte-identically and statically verified; not yet run on
target hardware.

## Hypothesis

B06VX already reaches the SeaBIOS/iPXE path with its corrected AHCI handoff
and deferred USB trace intact.  B06VY tests one additional, isolated platform
initialization hypothesis: after the inherited ICH10 base and SATA contracts
succeed, the ICH10R IOAPIC can be decoded and handed to the payload in a
deterministic, fully masked state without installing an interrupt route.

The implementation is intentionally narrower than coreboot's historical
`i82801jx` LPC initialization.  It installs no IOAPIC ExtINT route, performs no
maximum-vector lock write, no PIRQ or SCI routing write, no HPET enable, no
ACPI generation, no USB/GPIO write and no broad southbridge-driver enable.
The inherited B06VP LAPIC/PIC virtual-wire state is unchanged.

## Evidence used

The earlier B06VP ROMMON work established the following facts on the target:

- D31:F0 identifies as `8086:3a16`, its RCBA register is `fed1c001`, and the
  exact OIC transition `00 -> 03` exposes the IOAPIC at `fec00000`.
- selector 0 reads IOAPIC ID `00000000`.
- selector 1 reads `00170020`: version `20`, maximum-redirection index `17`,
  hence 24 redirection entries.
- two cold-boot censuses observed different undefined vector/delivery/
  destination contents, but the mask bit was set in every one of the 24 low
  dwords.
- a separate entry-23 experiment proved high-then-low data-window writes,
  exact readback, restoration of the original pair, and final selector zero.

Relevant immutable raw-capture hashes are:

```text
3bc9fc4498dbbdd3f8ef4cf770c5c82be1bea567d48dc46ab7aaae43d073dbcd  b06vp-car-ioapic-redir-00-05
a290511ce92fa9a477e873079f53021a53f456ab5bbb56b0a4e8ee1b973ecd23  b06vp-car-ioapic-redir-06-11
e6df0992f5e3fa6d60d0dca1951412c1737b394210fcfc7ec844fb87bdbc0f3a  b06vp-car-ioapic-redir-12-17
ad3daf89293c847bbc8725e03b4628dc06cca4fba55fc1d0bb0fc3b16920e200  b06vp-car-ioapic-redir-18-23
bb9e338b8b6fffc4edf67524a2d93c87fa82474360df90b362724d81e1cbfb1c  b06vp-hw-cold-02-ioapic-redir-00-05
c3ab1354fefe5f809114f907ae4a77490ce7e080d39febba9528764b0ff6078c  b06vp-hw-cold-02-ioapic-redir-18-23
6edfd6b709a9b9a0bbef4920cf1fe2608804c968b201dc9b68950de42c2ea0ca  b06vp-hw-cold-02-ioapic-entry23-mask-restore
b2d3fced5ad3a964dfb8cd47344cf36f7f399e39f63a3a5478229b9052fd398d  b06vp-hw-cold-02-ioapic-decode
```

These observations justify only decode, identity/count validation and a
masked canonical state.  They do not prove interrupt delivery.

## Exact execution contract

The stage runs once in the domain scan hook, after
`b06vqi_program_baseline_once()` and `b06vu_program_ahci_route_once()`, and
before the inherited raw PCI root preflight.

It applies these gates and operations in order:

1. Require the inherited ICHBASE and corrected AHCI-route stages to have run.
2. Require D31:F0 ID `3a168086` and RCBA `fed1c001` exactly.
3. Admit OIC only as `00` or `03`; write exactly `03` only from `00`, and
   require exact readback.
4. Require IOAPIC ID `00000000` and raw version dword `00170020` exactly.
5. Read and log all 24 low/high redirection pairs.  If any low dword lacks
   bit 16, halt before changing any pair.
6. Immediately before each pair, reread its low dword and require bit 16
   still set.  Write the high dword first as `00000000`, then the low dword as
   `00010000`, skipping already-canonical values.  Every data-window write has
   exact readback.
7. Read and log all 24 pairs again, require every pair to be exactly
   `HIGH=00000000 LOW=00010000`, then select register zero and require
   `IOREGSEL=00000000`.

Every rejected state halts before PCI enumeration.  Once IOAPIC decode is
active, the failure helper makes a best-effort selector-zero restore before
emitting the terminal failure code.

## POST and serial acceptance criteria

```text
70  exact LPC/RCBA/OIC gates admitted; decode operation begins
71  OIC=03 and exact ID/version/count tuple accepted
72  complete initial 24-entry census found every entry masked
73  complete canonical write/readback/final census and selector restore passed
74  terminal fail-closed rejection
```

A successful serial trace must include:

- `PRE` lines for entries `E00` through `E23`;
- `POST` lines for entries `E00` through `E23`;
- `READY ENTRIES=24`, a bounded `CHANGED` count, and
  `IOREGSEL=00000000`;
- the explicit negative write-policy markers `IOAPIC_EXTINT_ROUTE=0`,
  `MRE_LOCK_WRITE=0`, `PIRQ_WRITE=0` and `SCI_WRITE=0`;
  and
- continued inherited B06VX PCI, SeaBIOS, USB-trace, SATA and iPXE output.

POST `73` alone proves only this IOAPIC contract.  It does not prove usable
IRQ delivery, ACPI, HPET, USB input, SATA media access or an operating-system
boot.  Preserve the complete COM1 log from before power-on.

## Expected boot-state variants

- A cold path may report `OIC=00 WRITE=1` before the exact `03` readback.
- A retained/warm path may report `OIC=03 WRITE=0`.
- `CHANGED` may vary because reset redirection contents were observed to vary.
  This is accepted only if the full initial census says every entry is masked
  and the final 24 pairs are canonical.

Any other OIC byte, identity/version tuple, unmasked initial entry, failed
readback, or second invocation must terminate at POST `74`.

## Explicit omissions and next dependency

Fixed resource reservations are deliberately not part of B06VY.  The current
domain owns a small, exact index contract and has no real D31:F0 LPC device
model.  Adding IOAPIC/RCBA/LAPIC reservations here would change allocator and
resource semantics at the same time as the first automatic IOAPIC write path,
making the hardware result harder to attribute.  A successor may add only
isolated fixed reservations after B06VY is proved on hardware.

Likewise, a successor must separately prove one interrupt route before adding
an IOAPIC ExtINT entry, PIRQ/SCI policy, MADT/ACPI declarations or an MRE lock
write.  SeaBIOS is still the routing owner in B06VY.

## Recovery

This remains experimental firmware.  Test only with the documented socketed
W25Q128 recovery path, a verified external programmer and a verified vendor
backup available.  POST `74` intentionally does not continue to the payload.
If serial output stops during or after the canonicalization sequence, remove
power and restore the known-good image externally; do not infer recovery from
the POST display alone.
