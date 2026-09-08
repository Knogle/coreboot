> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WA quiescent ICH10 HPET-decode stage

Date: 2026-09-06

Status: **build-only; built twice byte-identically and statically verified; do
not flash until B06VZ has a recorded target-hardware PASS**.

## Purpose and one-delta rule

B06WA is a separately selectable successor to B06VZ.  It adds one narrowly
bounded platform operation: expose the ICH10R HPET at `fed00000` through RCBA
`HPTC`, prove that the live block has the measured identity and remains
quiescent, and reserve its 1-KiB aperture in coreboot's domain resource model.

The design is based on Intel ICH10 Family Datasheet 319973-003 and the local
B06VP experiments:

- [`ich10-hpet-decode-b06vp-oic3.xrs`](../research/scripts/ich10-hpet-decode-b06vp-oic3.xrs)
  proved the exact `HPTC 00000000 -> 00000080` transition with address selector
  zero and no HPET functional write;
- the immutable
  [`decode/census metadata`](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
  records `GCAP_ID=0429b17f:8086a301`, zero general configuration, zero
  interrupt status, a stopped counter and timer 0 disabled; and
- the immutable
  [`counter run/stop metadata`](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
  proves the counter clock and stop semantics while explicitly leaving routes,
  comparators and interrupts untouched after cleanup.

Those experiments justify decode and readback.  They do not prove interrupt
delivery.  The earlier HPET-to-IOAPIC experiment did not deliver an interrupt
in the retained pre-QPI CAR state, so B06WA intentionally does not publish an
HPET ACPI table or configure an interrupt.

All B06VZ behavior is retained: resources 0 through 14, the deterministic
masked IOAPIC, PCI/SATA path, SeaBIOS, deferred USB trace and RTL8168 iPXE ROM.
The 13-function devicetree, PCI allocator apertures and generic SMBIOS inputs
remain byte-for-byte inherited.

## Exact preflight and sole hardware delta

The B06WA routine executes once after the inherited B06VY IOAPIC routine has
returned and before the normal raw PCI preflight.  Before its first and only
possible `HPTC` write it requires:

1. no earlier B06WA attempt;
2. the inherited IOAPIC stage to have run;
3. LPC identity `00:1f.0 = 8086:3a16`;
4. exact enabled RCBA `fed1c001`;
5. inherited `OIC=03`;
6. complete `HPTC` equal to exactly `00000000` or `00000080`; and
7. when `HPTC=0`, both HPET capability dwords to read as open bus
   (`ffffffff:ffffffff`).

If `HPTC=0`, B06WA constructs exactly `00000080`: selector bits 1:0 remain
zero, selecting `fed00000`, and only decode-enable bit 7 is set.  It performs
one 32-bit RCBA write.  If the complete register is already `00000080`, it
performs no write.  Every other complete value fails closed; unknown policy
bits are never retained or normalized.

Serial and POST diagnostics remain available around this operation.  “First
write” above means the first write to the HPET/ICH10 experiment state; the
pre-existing UART and POST-code diagnostic path is intentionally preserved.

## Exact post-decode gate

Before resource construction or PCI enumeration may continue, B06WA requires:

```text
HPTC                    00000080 exactly
GCAP_ID high:low        0429b17f:8086a301 exactly
GEN_CONFIG high:low     00000000:00000000 exactly
GEN_INT_STATUS high:low 00000000:00000000 exactly
timer 0 IRQ-enable bit  clear
main counter            unchanged across a bounded 4096-us delay
LPC / RCBA / OIC        unchanged and exact
```

The final gate rereads general configuration, interrupt status, timer-0
interrupt enable, HPTC, RCBA and LPC identity immediately before setting the
internal ready flag.  Capability, timer configuration and both counter samples
are logged in full.  B06WA never writes an HPET register: it does not start the
counter, arm a comparator, select an interrupt route, enable legacy
replacement, clear interrupt status or modify a timer.

There is deliberately no automatic HPTC rollback after the decode write.  If a
later live gate fails, POST `78` halts with the aperture potentially still
decoded, but with no HPET functional write made by B06WA.  This preserves the
first-failure evidence and avoids an unverified second chipset mutation; the
socketed-chip/B06VZ recovery path remains authoritative.

The new POST codes are:

| Code | Meaning |
|---:|---|
| `75` | exact preflight passed; decode operation begins |
| `76` | exact `HPTC=00000080` readback passed |
| `77` | complete quiescent HPET gate passed |
| `78` | B06WA fail-closed stop |

A successful cold `HPTC=0` path is expected to include:

```text
[HPET] B06WA PRE LPC_ID=3a168086 RCBA=fed1c001 OIC=03 HPTC=00000000 ALLOWLIST=00000000,00000080
[HPET] B06WA POST HPTC=00000080 WRITE=1 GCAP=0429b17f:8086a301 GENCFG=00000000:00000000 ISR=00000000:00000000 ...
[HPET] B06WA READY BASE=fed00000 SIZE=00000400 STOPPED=1 TIMER0_IRQ=0 ROUTE_WRITE=0 COUNTER_WRITE=0 ACPI_TABLE=0
```

On an allowed retained `HPTC=80` path, `WRITE=0`; every live gate still runs.

## Resource contract

B06WA appends exactly one fixed, assigned, reserved, stored MMIO resource:

| Index | Kind | Half-open range | Exact semantic flags |
|---:|---|---|---|
| 15 | ICH10R HPET | `fed00000-fed00400` | assigned, fixed, memory, reserve, stored |

It preserves B06VZ resources 0 through 14 and requires exactly 16 domain
resources.  The inherited exact bound/flag checks still cover 0 through 14;
the new audit requires exact index, base, size and flags for 15.  Compile-time
and runtime checks place the HPET range after the IOAPIC and before RCBA,
compare it with every earlier domain-memory range, and compare it with every
assigned leaf BAR.  The allocator apertures remain I/O `1000-ffff` and MMIO
`c0000000-dfffffff`.

The expected resource messages are:

```text
[RESOURCE] B06VZ fixed reservations 8..14 installed; allocator apertures unchanged
[RESOURCE] B06WA HPET reservation 15 installed; B06VZ resources preserved
[RESOURCE] B06VZ exact resources 0..14 and non-overlap audit PASS
[RESOURCE] B06WA exact resources 0..15 and non-overlap audit PASS
```

## Deliberate omissions

B06WA adds none of the following:

- HPET counter, comparator, route or interrupt enablement;
- HPET, MADT, FADT, MCFG, DSDT or any other ACPI table;
- SCI, SMI, PIRQ, PIC, LAPIC or IOAPIC-redirection writes;
- USB, 8042, legacy-ownership or board GPIO policy;
- AP startup, SMP declaration or another CPU object;
- a broad ICH10 device model or generic ICH10 LPC initialization; or
- a new SMBIOS producer or SMBIOS input change.

The release gates retain `CONFIG_HAVE_ACPI_TABLES=n`, `CONFIG_SMP=n`,
`CONFIG_IOAPIC=n` and `CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n`.  The narrow board
code can therefore validate already decoded hardware without accidentally
bringing in table generation, generic interrupt programming or the broad
southbridge driver.

## Flash gate and target procedure

Do not use the B06WA W25Q128 image until B06VZ has a preserved target log
showing its inherited B06VY IOAPIC `READY`, both exact B06VZ resource messages,
and continued SeaBIOS/iPXE execution.  A successful B06VZ build is not a
B06VZ hardware PASS.

After that prerequisite exists:

1. retain the known-good B06VZ chip/image and verified vendor backup;
2. start COM1 capture at 115200 8N1 before applying power;
3. use the documented socketed W25Q128 recovery configuration;
4. perform one cold boot and preserve the complete log;
5. require POST `75`, `76`, `77`, the three exact HPET lines, both resource
   audit lines and continued SeaBIOS/iPXE milestones; and
6. on POST `78`, `3c` or `3d`, restore B06VZ and inspect the first failed exact
   gate rather than broadening its allowlist.

No target write, reset or B06WA execution is recorded here.  Even a B06WA
hardware PASS proves only deterministic, quiescent HPET decode and truthful
resource exposure.  It does not prove HPET interrupt delivery or authorize an
ACPI HPET table.

## Build

```bash
./scripts/build_x58_b06wa.sh
```

The wrapper calls the shared deterministic successor builder.  It performs two
clean fixed-epoch coreboot/SeaBIOS/iPXE builds, runs the complete repository
test suite between them, requires byte identity, audits the inherited payload
and option-ROM policy, and emits the public 4-MiB base plus local user-firmware
composites.  The latter remain under `blobs-local/` and are not distributable
repository inputs.
