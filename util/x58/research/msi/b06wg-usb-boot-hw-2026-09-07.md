> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WG automatic USB, keyboard and local Hiren's boot observation

Date: 2026-09-07
Evidence status: **hardware-observed, single-run milestones; not a stability result**

## Fixed test context

```text
test IDs: B06WG-HW-USB-01 and B06WG-HW-HIRENS-01
image ID: X58PROE-B06WG-AUTO-USB-SEABIOS-20260907
expected W25Q128 image SHA-256: ba5c6d24365ed7fceee4fe64b4a6179244fbe60c295f13349c6da30eb764ac7c
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board: MSI X58 Pro-E / MS-7522; exact PCB revision not restamped
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode: embedded fixed-test revision 0000001f
DIMM: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8, sole SPD responder 0x54
GPU: AMD Radeon HD 5450 1002:68f9 with its physical VBIOS
NIC: RTL8168 10ec:8168 at 02:00.0
USB storage: JetFlash Transcend 128GB, 512-byte sectors
console: COM1 0x3f8, 115200 8N1 through ConsolePi /dev/ttyUSB1
recovery: socketed known-good chip and Shelly mains switch available
```

The initial automatic run followed a one-second Shelly mains interruption.
That is the established recovery operation, but electrical G3 was not
independently instrumented. The later local-Hiren's capture starts during an
already active USB transfer after resetting the CH341 USB-UART adapter, so it
is intentionally not described as a complete reset-to-payload transcript.

## Preserved captures

```text
caa59c5e4db3a4e9f2053703c10779a309e100b3528dc2b21f852c96af022fcc  research/msi/captures/2026-09-07-b06wg-clean-cold2.raw
size: 121124 bytes

edc8938171612f57bd10115387d435eb36ca244113acfd912e84249b9b5605d4  research/msi/captures/2026-09-07-b06wg-local-usb-hirens.raw
size: 2593277 bytes
```

The first capture is a clean reset-to-SeaBIOS trace. The second deliberately
preserves the very verbose SeaBIOS EHCI transfer stream, subsequent external
bootloader activity, AHCI queries and the following firmware reset entry.
The BSOD itself was graphical and therefore is not present on COM1.

## Proven automatic platform and USB path

The clean capture establishes one complete automatic B06WG passage through:

```text
three CSI/QPI phases and the guarded MINIT return
B06WG sparse post-memory checks and CBMEM handoff
postcar and DRAM-backed ramstage
PCI discovery and fixed resource assignment
Radeon physical option ROM and VGA text output
ICH10R SATA/AHCI enablement
automatic GPIO57 low / 65.536 ms / high USB release
both all-OCA-clear gates, including the final pre-payload gate
coreboot table construction and SeaBIOS SELF loading
SeaBIOS EHCI/UHCI enumeration
USB HID keyboard setup
USB mass-storage enumeration
RTL8168 option-ROM execution and iPXE DHCP/TFTP chainload
```

Relevant retained observations include:

```text
[USB-AUTO] ... PAYLOAD ADMIT
[PAYLOAD] entering SeaBIOS payload
SeaBIOS (version rel-1.17.0-1-g5497f431)
USB keyboard initialized
USB MSC vendor='JetFlash' product='Transcend 128GB' rev='1100'
USB MSC blksize=512 sectors=0xe671800
iPXE 1.21.1+ (g7c39c)
net0 ... using rtl8168 ... [Link:up]
netboot.xyz.kpxe : 388848 bytes [PXE-NBP]
```

The operator subsequently confirmed that USB keyboard input worked in the
iPXE user interface. This is one physical accepted-input observation, not the
required ten-run stability result.

## Local USB Hiren's result

The local USB attempt produced a long sequence of successful EHCI bulk reads.
The retained file contains 41,549 `ehci_send_pipe` lines, emitted at SeaBIOS
debug level 7; this is the precise synchronous-COM1 flood removed by B06WH's
level-6 threshold.
The dominant pattern was a 65,024-byte data read followed by the normal
13-byte CSW and next 31-byte CBW. No SeaBIOS USB timeout or transfer error was
seen in the retained portion. The stream later contained code executing at
`CS:IP=9480:7d82`, followed by more USB reads and successful AHCI disk reads:

```text
AHCI/5: ... intbits 0x1, status 0x50 ...
AHCI/5: ... finished, status 0x50, OK
ahci disk read, lba 0/1, ... rc 0
```

Two `invalid handle_legacy_disk` diagnostics carried `AX=1500` and `DL=0/1`.
They are requests for absent legacy floppy drive types and were followed by
further successful USB and AHCI I/O; they are not treated as the terminal
failure.

The operator observed Hiren's/Windows PE reach a graphical blue screen and
automatically reset before its bugcheck text could be recorded. The following
firmware entry reached the inherited warm-state PCIEXBAR preflight rejection
(`CODE=0d`) and recovery CAR ROMMON. This proves that the local USB path got
beyond SeaBIOS media enumeration, boot-sector handoff and substantial loader
I/O far enough for Windows to render a bugcheck. It does **not** identify the
bugcheck code, prove a completed Windows kernel boot, or validate all reported
memory.

## Separate iPXE observations

The same B06WG session also exposed later network-loader failures after the
known-good initial DHCP/TFTP chainload:

```text
1c0ded02: TLS record authentication/MAC-tag failure
4c0c6035: HTTP core 120-second idle watchdog
4c072035: generic no-progress timeout
```

Those exact meanings were correlated with the locally pinned chainloaded iPXE
source revision. They do not prove whether the underlying cause is the
RTL8168/root-port path, the chainloaded UNDI transition, or corruption in the
largely untested high DRAM area. `No more network devices` at the end of an
autoboot attempt is normal control flow and does not by itself mean the NIC
disappeared.

## What remains unproven

B06WG deliberately tests only sparse points in three low windows before
advertising RAM up to 3 GiB below 4 GiB and another 1 GiB above 4 GiB. Neither
the iPXE errors nor the Windows bugcheck can therefore be assigned to USB with
the present evidence. The minimal ACPI/interrupt description and incomplete
ICH10 PCIe-root-port initialization remain independent candidates as well.

The next local-USB run must retain the graphical STOP code, preferably with a
phone video or the Windows legacy F8 option `Disable automatic restart on
system failure`. Interpret the first code before changing platform state:

- `0x0000007B`: prioritize AHCI/storage handoff;
- `0x000000A5`: prioritize ACPI tables;
- memory, machine-check or interrupt-related codes: prioritize a bounded
  high-RAM test/E820 clamp and APIC/IRQ validation.

B06WH is intentionally only a diagnostic-throughput successor: it keeps the
same B06WG platform writes and USB trace journal but lowers SeaBIOS's console
debug threshold from 8 to 6. That suppresses the per-transfer
`ehci_send_pipe`/`uhci_send_pipe` stream while retaining controller setup,
device discovery, bounded USB trace, errors and boot milestones. Since UART
output is synchronous, the lower verbosity also changes I/O timing and must
not be treated as behaviorally timing-identical to B06WG.
