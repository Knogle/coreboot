> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VO X58 IOHBUSNO configuration-routing experiment

Status: **BUILT TWICE / BYTE-REPRODUCIBLE / 216 TESTS PASS / TWO COMPLETE
HARDWARE PATHS THROUGH IOH ROUTING, RADEON, PHYSICAL VBIOS AND DISPLAY**.

B06VO keeps B06VN's complete memory, QPI, IOU0, selective-PCI, physical-Radeon-
VBIOS, SeaBIOS and RTL8168-iPXE path.  It tests one additional hardware
hypothesis: the X58 IOH must claim its internal device/function numbers only
on bus 0 before coreboot assigns bridge buses and probes the Radeon.

B06VN proved that X58 root port `00:03.0` was already at Gen1 x16 with
`DLLA=1` and `LT=0`, yet `01:00.0` did not answer.  The same run showed the
IOH's internal functions duplicated on bus 2.  Intel documents this aliasing
when `IOHBUSNO.Valid` is clear.  The vendor-firmware reference instead has
`00:14.0 + 0x10a = 0x0100`.

## One new write hypothesis

The implementation runs after B06VN's complete raw root preflight and before
the `ROOTS_SAFE` milestone, IOU0 start logic or PCI scan.  PCIEXBAR has already
been gated to `0xe0000000`, and the `00:14.0` identity has already passed.

It accesses the documented X58 CSRCFG register directly through PCIEXBAR:

```text
BDF/register: 00:14.0 + 0x10a (IOHBUSNO)
address:      0xe00a010a
width:        16 bits
accepted PRE: 0x0000 or 0x0100 only
write:        exact 0x0100, only when PRE is exact 0x0000
required POST: exact 0x0100
```

`0x0100` sets the documented Valid bit while selecting bus 0.  B06VO does not
use a read-modify-write because every other bit is reserved.  Any PRE value
outside `{0x0000, 0x0100}`, a failed readback, or a second attempt halts with
POST `3f`.  PRE `0x0100` is accepted without a store.  A successful exact
gate emits POST `33`.

This is the only write added by B06VO.  The inherited B06VN conditional IOU0
start action remains unchanged and still executes only if its prior `DLLA=0`
gate is met.

## Read-only routing telemetry

Immediately before and after the optional IOHBUSNO write, direct PCIEXBAR
reads report:

```text
00:14.0 + 0x10a  IOHBUSNO       16-bit
00:14.0 + 0x11c  LCFGBUS base    8-bit
00:14.0 + 0x11d  LCFGBUS limit   8-bit
00:14.0 + 0x134  GCFGBUS base    8-bit
00:14.0 + 0x135  GCFGBUS limit   8-bit
01:0d.0 + 0x000  alias ID       32-bit
02:0d.0 + 0x000  alias ID       32-bit
```

The range-register offsets come from Intel X58 datasheet sections
17.6.5.21/.22 and 17.6.5.31/.32.  They are observations only; B06VO neither
writes nor gates on them.  The reference machine measured local/global ranges
as `00-fb`, but this image intentionally does not copy those values.

An alias remaining after the write is logged, not turned into a second repair
hypothesis.  It would show that IOHBUSNO alone is insufficient or that the
observed access is not governed as expected.

## Paired downstream probe

B06VO replaces only the scanner callback passed to `do_pci_scan_bridge()`.
Coreboot's generic helper first calls `pci_bridge_route(..., PCI_ROUTE_SCAN)`,
which programs primary, secondary and subordinate bus numbers.  The B06VO
callback then acts only for root `00:03.0`:

1. reject secondary bus `00` or `ff`;
2. read `secondary:00.0` through legacy CF8/CFC and direct PCIEXBAR;
3. repeat at cumulative `0`, `1`, `10` and `100` ms;
4. print both 32-bit IDs and whether they match;
5. call the unchanged `pci_scan_bus(bus, min_devfn, max_devfn)`.

The delays are deltas `0/1000/9000/90000 us`, so the added bounded wait is
exactly 100 ms, not 111 ms.  These samples perform no configuration write and
do not change the downstream endpoint's optional/fail-soft policy.

Useful result classes are:

- both paths return an AMD identity: IOHBUSNO routing was the missing
  visibility step, and the existing VGA identity/resource/VBIOS path can run;
- both paths stay `ffffffff`: the link-status/routing repair is insufficient,
  narrowing later work toward endpoint reset, clock or power sequencing;
- ECAM sees the Radeon while CF8 does not: legacy configuration routing or
  the scanner access method is the remaining problem;
- CF8 sees the Radeon while ECAM does not: PCIEXBAR routing/coverage is
  inconsistent and must be understood before further writes;
- `01:0d.0`/`02:0d.0` disappear but the Radeon stays absent: alias repair
  worked, but it was not the endpoint blocker.

## Explicit exclusions

B06VO adds no write to:

- PCI Bridge Control or secondary-bus reset;
- PCIe Link Control, Link Capability or retrain controls;
- IOU0/IOU2/AUX registers beyond B06VN's pre-existing conditional action;
- GPU configuration space or Radeon VBIOS;
- interrupt, APIC or IRQ-routing state;
- LCFGBUS/GCFGBUS;
- payload or network policy.

SeaBIOS retains hardware-IRQ support and physical VGA-ROM checksum/execution
policy.  Coreboot graphics init and coreboot VGA-ROM execution remain off.
CBFS contains the pinned RTL8168 iPXE ROM and no AMD option-ROM blob.

## Expected serial boundary

On the expected reset-state path, the first new lines are structurally:

```text
[IOHCFG] PRE IOHBUSNO=0000 LCFGBUS=..-.. GCFGBUS=..-.. ALIAS01:0d.0=........ ALIAS02:0d.0=........
[IOHCFG] POST IOHBUSNO=0100 LCFGBUS=..-.. GCFGBUS=..-.. ALIAS01:0d.0=........ ALIAS02:0d.0=........
[IOHCFG] B06VO IOHBUSNO exact gate PASS (write=1)
```

After root `00:03.0` gets its temporary secondary bus:

```text
[PCIE] B06VO PEG_PROBE T_US=0 BUS=.. CF8=........ ECAM=........ MATCH=.
[PCIE] B06VO PEG_PROBE T_US=1000 BUS=.. CF8=........ ECAM=........ MATCH=.
[PCIE] B06VO PEG_PROBE T_US=10000 BUS=.. CF8=........ ECAM=........ MATCH=.
[PCIE] B06VO PEG_PROBE T_US=100000 BUS=.. CF8=........ ECAM=........ MATCH=.
```

POST additions:

```text
33  IOHBUSNO exact gate passed (write may have been skipped for PRE=0100)
3f  second attempt, unexpected PRE, or non-0100 readback; terminal
```

The serial log is authoritative; later milestones overwrite a displayed POST
code.

## Build, image and recovery

Reproduce the release with:

```bash
./scripts/build_x58_b06vo.sh
```

The script performs two clean, ccache-disabled builds at fixed
`SOURCE_DATE_EPOCH=1788688800`, checks byte identity, audits SeaBIOS/iPXE and
CBFS policy, runs the VI-through-VO source contracts, verifies the locally
inserted MSI ranges and constructs the top-aligned W25Q128 image.

Program only the complete local image:

```text
blobs-local/msi-x58-pro-e/b06vo/
  msi-x58-pro-e-b06vo-deterministic-w25q128-16MiB.rom
size:    16777216 bytes
SHA-256: 01427d62e1fb50dc5ee9208490017eccf29113599636f4df4400cc9f9075426c
```

Its lower 12 MiB are erased (`0xff`) and its upper 4 MiB equal the verified
deterministic composite byte for byte.  Externally read the chip back and
require the complete 16-MiB hash before fitting it.  Keep the known-good B06VN
or B06VL chip and the vendor recovery chip available.

## Risk and proof boundary

IOHBUSNO is global IOH configuration routing state and is not rolled back.
The exact vendor-observed value and fail-closed precondition limit the change,
but only a target cold boot can prove that this silicon/state accepts it.
Failure before serial or POST remains possible, so socketed-chip recovery is
required.

Construction alone proves only source intent, deterministic artifacts and
bounded control flow.  B06VO-HW-02 and HW-04 subsequently provided two
complete one-second-relay hardware passes:
`IOHBUSNO` changed `0000->0100`, the known bus-2 X58 aliases disappeared, the
Radeon VGA function answered consistently through CF8 and PCIEXBAR, coreboot
assigned its resources, and SeaBIOS entered and returned from its physical
59,904-byte VBIOS.  The operator supplied a photo showing SeaBIOS, RTL8168
iPXE at `02:00.0`, and the same Ctrl-B prompt as the UART trace.
HW-04 then retained at least 120 seconds with no UART progress after iPXE's
first logged `INT 16h AH=01` call.  The leading inference is missing LAPIC
LINT0 ExtINT setup and therefore no legacy-PIC IRQ0 timer delivery; this is the
next isolated hardware hypothesis, not yet a proven cause.  Two runs are not
the required 10-cold/10-warm set, and no network, storage or OS boot has
occurred.  B06VO continues to rely on local,
non-redistributable MSI CSI/MINIT bytes; none were added to the public tree.

See the [B06VO release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
the [B06VO hardware analysis](../research/msi/b06vo-hw-02-2026-09-06.md),
the [B06VO long-run repeat](../research/msi/b06vo-hw-04-2026-09-06.md),
the [B06VN hardware analysis](../research/msi/b06vn-hw-03-hw-04-2026-09-06.md),
and the [Intel X58 datasheet](https://www.intel.com/content/dam/doc/datasheet/x58-express-chipset-datasheet.pdf).
