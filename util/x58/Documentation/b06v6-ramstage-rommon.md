> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06V6 automatic handoff and DRAM ROMMON

## Purpose and evidence boundary

B06V6 is the first image that automatically replays the one exact
vendor-assisted RAM-init sequence observed with the lab E5645 and sole SPD
responder at `0x54`, leaves cache-as-RAM, loads coreboot postcar and ramstage,
and stops in a new DRAM-resident ROM monitor. It has no payload and performs no
normal coreboot device enumeration.

This is an experimental bridge, not native X58 RAM initialization. The locally
composed image still executes hash-pinned MSI CSI/MINIT code. The source tree
contains no proprietary bytes, and the base coreboot ROM cannot complete the
vendor-assisted path until the private composition step is applied.

The complete B06V6 path is build-tested only. Until a hardware log reaches the
final `0x0f` prompt, do not claim that postcar, ramstage, CBMEM, the PM timer,
the NIC snapshot, or an upload has run on the board.

## Exact automatic path

The path accepts only the recorded E5645 configuration:

- CPUID `000206c2`, microcode revision `0000001f`;
- exactly one DDR3 SPD responder at `0x54`, complete SPD FNV `fb66b530`;
- CSI return tuple `2/2/106`, complete CSI-state FNV `a3736e20`;
- cold-policy FNV `3c0f3a0b`;
- MINIT workspace FNV `6d87f4e4`, EAX zero, bytes `1/2=0`, `4f=02`,
  `e79=01`;
- mapper `00024489`, common status `00001545`, channel-2 geometry/status
  `000002ac/00000003/00000140`, and QPI status `030f0f03`.

The CSI transaction uses two reset-separated passes. A five-byte complemented
I801 host-register signature distinguishes pass 1, pass 2, and a MINIT-in-
progress state. CMOS diagnostic byte `0x0e`, observed as `0x6c`, is changed to
`0xec` before the first automatic vendor call and restored only after every
final gate passes. This prevents a chipset reset that loses all I801 state from
silently restarting pass 1 forever.

A guard failure stops at POST `1f`. On the following boot, enter the retained
CAR ROMMON, issue `unlock RESET`, then `autoguard clear`, verify the reported
read-back, and remove AC power. A CF9 full reset is not equivalent to an AC
cycle. The socketed known-good recovery chip remains mandatory.

Absolute MINIT EBX/ECX/EDX/EDI/EFLAGS values are deliberately not success
gates. Static inspection proved that at least EBX was the address of a caller
result object and therefore changes with the linked CAR layout. B06V6 instead
requires telemetry/result self-consistency, EAX zero, balanced dedicated
vendor stack, complete buffer digests/markers, intact canaries and exact
post-MINIT Uncore state.

## Memory transition

Before creating CBMEM, romstage requires the pre-postcar MTRR state observed in
the successful B06V5 session: default UC, only CAR WB and the 256-KiB ROM window
WP, and all other variable MTRRs disabled. It then writes distinct patterns to
14 representative dwords across both future regions in one transaction:

| Purpose | Range | Cache policy in B06V6 |
| --- | --- | --- |
| CBMEM, relocated ramstage and post-CAR state | `01000000..017fffff` | WB after postcar |
| postcar, later reused as object scratch | `02000000..027fffff` | UC |

All patterns must coexist, read back exactly, and then every original must be
restored exactly. This catches representative cross-window aliasing. Next,
every aligned dword in both complete 8-MiB windows is written and checked with
an address-derived pattern and its inverse, then cleared and checked again.
Thus all 16 MiB used by this transition are touched, but this still does not
prove long-term retention, cache coherency, or the complete 4-GiB DIMM.

CBMEM is initialized with an exact handoff record. Ramstage rejects an offline,
wrapped, out-of-window or undersized CBMEM region, a bad handoff digest, and a
postcar MTRR state other than CBMEM WB plus ROM WP with default UC.

The `0x02000000` overlap with postcar is temporal: postcar occupies about 44
KiB there, loads the relocatable ramstage into CBMEM, and transfers control
without a return path. The ramstage ELF's nominal `0x04000000` VMA is not its
runtime physical destination. Only the later ramstage ROMMON can accept an
object and overwrite the stale postcar bytes.

## POST map

The B06V6-specific codes are collision-free relative to the active board and
generic Intel/coreboot paths:

| Code | Meaning |
| --- | --- |
| `02..08` | automatic begin, platform, CSI pass 1/pass 2, CSI accepted, policy installed, MINIT accepted |
| `09` | exact post-MINIT handoff entered |
| `0a` | pre-postcar MTRRs accepted |
| `0b` | joint two-window alias smoke and both complete 8-MiB UC tests passed |
| `0c` | CBMEM handoff created and verified |
| `0d` | postcar MTRR frame populated |
| `0e` | ramstage ROMMON callback entered |
| `0f` | DRAM ROMMON ready |
| `14..19` | exact-result/MTRR/smoke/CBMEM/handoff terminal failures |
| `1a` | ramstage CBMEM, UART, handoff or postcar-MTRR rejection |
| `1b` | serial upload failure |
| `1c/1d/1e` | requested warm reset/full reset/halt |
| `1f` | persistent automatic guard stop |
| `20` | automatic path declined; CAR ROMMON fallback |
| `33/35` | binary upload active/complete |

Generic postcar teardown still emits its own standard `30/31/32` sequence.
The established local vendor call boundaries `d1/d2` and `d3/d4` remain in
the trace.

## DRAM ROMMON commands

COM1 is fixed at `0x3f8`, 115200 baud, 8N1, without flow control. B06V6 refuses
to enter the monitor if the build or live UART state differs.

```text
id
help
handoff
cpuid LEAF [SUBLEAF]
mtrr
obj status
obj clear
obj crc
obj dump OFFSET COUNT
ramload
timer
netprobe
pci BUS DEV FN REG WIDTH
io PORT WIDTH
mem ADDRESS WIDTH
msr INDEX
unlock RESET
lock
reset warm|full
halt
```

Numbers are hexadecimal; widths are `1`, `2`, or `4`. `obj dump` is limited to
`0x100` bytes per command. Arbitrary MMIO and MSR reads can still hang or fault
the machine. Reset is one-shot armed and performs `WBINVD` before the CF9
sequence because CBMEM is write-back cached.

`ramload` switches COM1 temporarily from line mode to the binary XRL1 protocol.
Before emitting framed `READY`, it waits for a bounded idle interval and
discards at most 256 residual command bytes. Terminal mappings such as CRLF or
CRCRLF therefore cannot become the first bytes of the binary header. The
uploader sends that header only after receiving and verifying `READY`.
It accepts one object of at most 4 MiB anywhere inside
`02000000..027fffff`, requires CRC32 both during receive and during a complete
volatile read-back, and records a nonzero object ID. A failed or replacement
transfer invalidates the prior metadata. `obj clear` does not erase scratch
bytes.

Example from the ConsolePi host, after closing picocom:

```bash
python3 scripts/x58_ram_loader.py send local-module.bin \
  --port /dev/ttyUSB1 --baud 115200 \
  --address 0x02000000 --object-id 0x12345678
```

The sender uses `pyserial` when present and falls back to the Python standard
library's POSIX `termios` interface. The checked `serial-gateway.example.invalid` host currently
has `/dev/ttyUSB1` but no `pyserial`, so this fallback is the expected path.
Writes and output draining in that fallback have explicit deadlines, including
the USB-UART disconnect case.

The object may be dumped and hashed, but B06V6 has no `arm` or `exec` command.
The final ramstage link garbage-collects the loader's unreferenced execution
gate. In particular, uploading a vendor module does not make it callable; its
entry point, mode, stack, fixed-address assumptions, PEI/AMIBIOS services,
parameter ABI, reset behavior and output contract still have to be proven.

## Network transport roadmap

`netprobe` is intentionally passive. It reads ICH10R root port 5 at
`00:1c.4`, its bus numbers, and—only if a usable secondary bus already
exists—the expected RTL8168/8111 function at `secondary:00.0`. It performs no
BAR, command, MMIO, reset, PHY, MAC or DMA write. `timer` verifies only that
the decoded 24-bit ACPI PM timer changes across bounded reads; it does not yet
install a generic coreboot timebase.

The next network build should proceed in measured gates:

1. prove a monotonic timeout source on hardware;
2. assign and verify an isolated RP5 bus number and NIC BAR without running
   global PCI resource allocation;
3. reset the RTL8168, obtain link state and read a trustworthy MAC address;
4. use polling 32-bit DMA rings constrained to a separately smoked buffer;
5. add bounded Ethernet/ARP/IPv4/UDP and fixed-server TFTP RRQ;
6. feed the received XRL1 container through the existing
   `x58_rl_begin/write/finish` sink and return to the prompt.

That TFTP command is a debug file-transfer facility analogous to Cisco ROMMON,
not a boot option. SSH remains a later loaded-debug-OS feature rather than
firmware-resident scope.

## First hardware test

Use the exact CPU/DIMM/Slow-QPI/ratio-6 setup and start after real AC removal.
Capture the entire serial stream and POST history. Do not begin with an upload.

1. Require both B06V6 identities and the exact SPD/platform telemetry.
2. Observe the expected CSI-produced reset and continuation; reject repeated
   pass 1 or any guard failure.
3. Require `08,09,0a,0b,0c,0d`, generic postcar `30,31,32`, then `0e,0f`.
4. At the prompt run only `id`, `handoff`, `mtrr`, `timer`, `netprobe`, and
   `obj status` and save the output.
5. Upload a small non-proprietary pattern file, then run `obj status`,
   `obj crc`, and bounded dumps at its beginning and end.
6. Only after the first log is reviewed, repeat cold boots and expand object
   sizes. Do not execute uploaded bytes in this release.
