> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VN X58 IOU0/PEG link-start and physical-VBIOS experiment

Status: **BUILT TWICE / BYTE-REPRODUCIBLE / 209 TESTS PASS /
HARDWARE: RAMSTAGE, SEABIOS AND IPXE OPTION ROM REACHED ONCE; IOU0 ALREADY
GEN1 X16; GPU ENDPOINT ABSENT; NETWORK AND BOOT NOT YET PROVEN**.

B06VN isolates the next hardware question exposed by B06VM: can firmware
release and train the X58 PCIe link from root port `00:03.0` to the discrete
GPU before the selective PCI scan?  This is the PEG/PCIe link to the Radeon,
not the CPU-to-X58 QPI link.

The installed card is an AMD Radeon HD 5450 with its own onboard VBIOS.  The
image therefore embeds no AMD option ROM.  Coreboot starts and enumerates the
link, allocates the card's resources, and leaves execution of the physical
card ROM to SeaBIOS.

## One new hardware hypothesis

After the inherited B06VL version-9 handoff and B06VM's exact CBMEM,
PCIEXBAR, SAD, TOLM/TOHM and bus-0 identity gates, B06VN performs the following
sequence before `pci_host_bridge_scan_bus()`:

1. log read-only PRE telemetry for X58 ports `00:01.0`, `00:03.0` and
   `00:07.0`;
2. require exact IOU0 root-port identity `8086:340a`;
3. if `00:03.0 LNKSTS.DLLA` is clear, issue exactly one 16-bit write of
   `0x000c` to `PCIE_PRTx_BIF_CTRL` at PCIEXBAR address `0xe0018190`;
4. poll at most `10000 * 100 us`, stopping only when `DLLA=1` and `LT=0`;
5. log the three ports again, including current speed and negotiated width;
6. continue into the bounded PCI scan even after a link timeout so COM1 and
   serial SeaBIOS remain available.

The value combines X58's IOU0 x16 mode with the documented start action.  The
action field is not compared after the write because it is write-only/action
state and reads as zero after the vendor firmware has initialized the port.
No standard PCIe Link-Control retrain, CPU reset, QPI retrain, or write to
ports `00:01.0`/`00:07.0` is made.

The implementation follows Intel X58 datasheet section 5.1.2's
`Wait_on_BIOS`/`PCIE_PRTx_BIF_CTRL` model.  It remains a narrowly scoped live
hypothesis until the B06VN serial trace proves the board-specific result.

## Hardware result

Two retained 2026-09-06 payload runs answered this hypothesis without
exercising the write. The second was captured exclusively with a physical-rate
check and no replay contamination.
Before the conditional action, `00:03.0` already reported Gen1 x16,
`LNKSTS.DLLA=1` and `LT=0`; B06VN therefore logged `start-write=0`. The two
other observed X58 ports remained width zero. The physical root link is not
the present blocker.

The GPU nevertheless returned no configuration identity at `01:00.0`, so the
AMD/VGA gate followed its intended fail-soft path. Coreboot completed resource
allocation, wrote its tables and entered SeaBIOS. SeaBIOS found no VGA, but it
did enumerate the RTL8168 at `02:00.0`, load the pinned iPXE ROM, validate its
`AA55` header, execute it, and reach the Ctrl-B prompt. The retained trace ends
after iPXE's first logged SeaBIOS INT 16h keyboard-status request; later serial
silence supports but does not prove a stop there. No Ethernet link, DHCP, TFTP
or boot is claimed.

SeaBIOS also exposed a separate routing defect: the 32 X58 internal functions
at `00:0d.0..00:16.7` appeared again on bus 2. Intel's documented
`IOHBUSNO.Valid=0` behavior claims those device/function numbers on every bus;
the vendor-firmware reference has `00:14.0+0x10a=0x0100`. The successor should
repair and verify that mandatory bus-number register before scanning, then
re-test `01:00.0`. Because this build's generic scan uses CF8/CFC while its
link telemetry uses direct ECAM, that re-test should log both access methods
immediately after assigning the secondary bus. This is a known defect repair,
not yet a guaranteed GPU fix.

See the complete [first B06VN hardware analysis](../research/msi/b06vn-hw-02-2026-09-06.md)
and [hardened repeat](../research/msi/b06vn-hw-03-hw-04-2026-09-06.md).

## Telemetry and interpretation

Each PRE/POST line includes:

```text
ID EXPECT 09c 0a0 0a2 0aa 0c0 190 SPEED WIDTH LT DLLA
```

The most useful result classes are:

- `DLLA=1`, `LT=0`, `WIDTH>0`, followed by an AMD endpoint: the PEG link was
  brought up sufficiently for configuration-space enumeration;
- `DLLA=0`, `WIDTH=0`, endpoint `ffff:ffff`: the start action alone is
  insufficient, so the next work should investigate the X58 clock/reset,
  lane/bifurcation, or additional IOU sequencing around this port;
- `DLLA=1` but the endpoint remains absent: link state and downstream
  configuration visibility disagree, narrowing the fault to bus/probe or
  endpoint reset/power behavior;
- an answering non-AMD or non-VGA function: POST `39` before BAR sizing;
- a valid AMD VGA followed by a later failure: PEG training succeeded and the
  failing resource or payload phase is identified by its later POST/log gate.

B06VN does not guess an HD 5450 device ID.  A present function must have
vendor `0x1002`, PCI class `0x0300`, and a normal header; its exact device and
subsystem identity are printed by the live run.  The optional HDMI-audio
function is deliberately outside this first minimal scan.

## POST additions

```text
30  IOU0 start boundary reached; write is conditional on DLLA=0
31  bounded IOU0 poll completed, either link-ready or timed out
32  AMD VGA endpoint present and accepted
35  VGA absent; continue toward serial SeaBIOS fallback
36  inherited handoff failure
37  platform/PCIEXBAR/IOU identity failure
38  static or enumerated topology failure
39  present PCI identity/class/header failure
3a  command-state failure
3b  bridge-route/bus-number failure
3c  resource failure
3d  resource overlap
3e  final enable-state failure
```

The serial line is authoritative.  A displayed POST code can be overwritten
by a later milestone and video remains absent until the physical VBIOS runs.

## Physical VBIOS and SeaBIOS policy

Coreboot has `CONFIG_VGA_ROM_RUN` disabled, `CONFIG_NO_GFX_INIT=y`, and no
`CONFIG_VGA_BIOS`.  CBFS contains only the independently built RTL8168 iPXE
ROM (`pci10ec,8168.rom`); there is no `pci1002,*.rom`.

SeaBIOS receives:

```text
etc/optionroms-checksum = 1
etc/pci-optionrom-exec  = 1
etc/sercon-port         = 0x3f8
```

Thus SeaBIOS validates the card ROM normally and maps a physical PCI ROM only
for VGA.  If the Radeon enumerates and its legacy x86 image is valid, the
expected serial progression includes `Scan for VGA option rom`, an attempt to
map the ROM BAR, and `Running option rom at c000:...`.  The CBFS RTL8168 iPXE
ROM remains available independently for network boot/debug after platform and
memory initialization.

## Exact first-run target

```text
CPU:   Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM:  one BLS4G3D1609DS1S00 at SPD address 0x54
GPU:   AMD Radeon HD 5450 with onboard physical VBIOS, exact PCI ID measured live
NIC:   onboard RTL8168, PCI ID 10ec:8168, revision 02
flash: socketed W25Q128.V..M, 16 MiB
UART:  COM1 0x3f8, 115200 8N1, no flow control
```

Capture COM1 from before power-on.  The inherited automatic CSI path can
perform its two expected continuation resets before ramstage.  B06VN then
runs without a RAMMON stop after the full memory/handoff gates pass.

Program only:

```text
blobs-local/msi-x58-pro-e/b06vn/
  msi-x58-pro-e-b06vn-deterministic-w25q128-16MiB.rom
SHA-256: 5f1c945aa98f213a09eaf417755c3578c9ebf50d218c1a6460959d019b8e5e45
size: 16777216 bytes
```

Read the chip back and require the full hash before fitting it.  Preserve the
B06VL known-good chip and a verified vendor recovery chip.  Recovery is by
socketed-chip swap or external reprogramming; B06VN has no automatic fallback
to the prior image.

## Build evidence and limitations

The release command `scripts/build_x58_b06vn.sh` completed two clean,
ccache-disabled builds at fixed `SOURCE_DATE_EPOCH=1788602400` and rejected
any byte difference.  The full Python suite passed 202/202 tests.  A separate
audit verified:

- only `b06vn_pci.o`, not `b06vm_pci.o`, is linked;
- the object contains exactly one IOU start store,
  `movw $0xc,0xe0018190`;
- both SeaBIOS policy files are eight-byte little-endian value `1`;
- CBFS has no AMD ROM and retains the pinned RTL8168 iPXE image;
- the lower 12 MiB of the W25Q128 image are erased and its top 4 MiB exactly
  equal the deterministic composite.

This proves construction and source intent. The hardware run additionally
proves that IOU0 was already active at Gen1 x16 and that the write was skipped;
it does not prove a successful endpoint transaction. B06VN still relies on
local, non-redistributable MSI CSI/MINIT code; assumes
the broader memory map is usable; supports only the current CPU/DIMM/topology;
and does not validate SMP, complete ACPI/PIRQ/SMM/S3, storage boot, network
transfer, graphics output, or an operating-system boot.

See the corresponding
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
[B06VM hardware analysis](../research/msi/b06vm-hw-02-2026-09-06.md), and
[Intel X58 datasheet](https://www.intel.com/content/dam/doc/datasheet/x58-express-chipset-datasheet.pdf).
