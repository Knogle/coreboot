> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WG: automatic GPIO57 USB release for SeaBIOS

Status: **SOURCE- AND BUILD-TESTED; ONE TARGET HARDWARE PASS THROUGH SEABIOS,
USB HID/MSC AND iPXE; LOCAL WINDOWS PE REACHED AN UNCAPTURED BSOD; STABILITY
PENDING**.

B06WG is a default-off sibling successor to B06WF and a functional successor
to B06WE. It deliberately does not compile the B06WF interactive late USB lab.
Instead, it promotes only the live-proven GPIO57/H_PWRGD sequence into a
bounded automatic late-ramstage policy and then continues into the existing
SeaBIOS USB stack.

The functional hypothesis is narrow: the B06WD controller state was already
sufficient for SeaBIOS, but the board-level common USB power-good/over-current
path remained held inactive until GPIO57 was driven output/high. The preceding
B06WD CAR experiments showed GPIO57 causally changing both EHCIs from
`3030` to `3020`, clearing live OCA on all EHCI/UHCI port views, and allowing
both a low-speed and a full-speed UHCI attachment to complete reset and enable.
A later B06WF run confirmed the bounded release through 500 ms
(`OCA fff -> 000`) and exposed attached devices in the UHCI views (`CCS=021`,
`LSDA=001`) before continuing to SeaBIOS. It still did not prove a descriptor
transfer or accepted keyboard input. B06WG is the first automatic payload
experiment intended to test that missing step without a manual ROMMON
transaction.

## Stage placement and ownership

The release callback runs at `BS_WRITE_TABLES / BS_ON_ENTRY`: after PCI
resources and normal device initialization, but before ACPI tables. A second
read-only callback runs at `BS_PAYLOAD_BOOT / BS_ON_ENTRY`, after table and
chip-finalization work and immediately before payload loading.

Coreboot B06WG performs no host-controller initialization. The pinned SeaBIOS
configuration already contains:

```text
CONFIG_USB=y
CONFIG_USB_EHCI=y
CONFIG_USB_UHCI=y
CONFIG_USB_HUB=y
CONFIG_USB_MSC=y
CONFIG_USB_KEYBOARD=y
CONFIG_KEYBOARD=y
CONFIG_PS2PORT=y
CONFIG_DEBUG_USB_TRACE=y
CONFIG_DEBUG_LEVEL=8
# CONFIG_THREADS is not set
```

B06WG uses its own `config_seabios_b06wg_usbtrace8` rather than changing the
historical B06VQ/B06WF payload configuration. SeaBIOS defines
`DEBUG_HDL_16` as level 9 and invokes it for every INT 16h entry. The live
B06WF handoff showed that level 9 generated about 3.25 KiB/s of `handle_16`
register dumps while the cursor waited for a key, overflowing the bounded
capture and obscuring the deferred USB trace. Level 8 suppresses that
level-9-only stream; the USB journal is emitted with `dprintf(1)`, so it
remains enabled.

SeaBIOS therefore retains ownership of EHCI/UHCI reset and run state,
`CONFIGFLAG`, companion routing, USB address assignment, descriptor and
configuration transfers, hub traversal, HID boot protocol and keyboard
polling. B06WG does not write a PCI command/BAR, `CONFIGFLAG`, or any EHCI or
UHCI `PORTSC` register.

## USB-focused fast post-memory path

B06WG alone treats the already repeated RAM/QPI result as provisionally stable
so that USB iterations do not repeat the roughly 19-minute exhaustive
post-memory window exercise. This is an explicit experimental tradeoff, not a
new claim that every RAM address is sound. Before using DRAM it still requires
the exact recorded-result tuple and the exact UC-MTRR admission state. It then:

```text
1. clears the low-memory test window exactly once and reads nine fixed offsets
2. performs the inherited 14-address transactional CBMEM/object alias smoke
3. clears the CBMEM and object windows exactly once
4. reads nine fixed offsets from each of those two windows
5. admits CBMEM handoff creation only after all checks pass
```

The omitted work is the phase-A pattern pass, inverted-pattern pass and full
readback pass over all three windows. Failure still stops at POST `17` before
postcar. The single zeroing pass is retained because later CBMEM/object users
require deterministic contents; its duration on target hardware has not yet
been measured. Sparse readback can detect gross mapping or write failures but
cannot replace the exhaustive predecessor test. Every non-B06WG build keeps
the original exhaustive implementation and call order unchanged.

## Automatic transaction

Before the first write, B06WG requires the measured late B06WD platform tuple:

```text
LPC:       8086:3a16
RCBA:      fed1c001
PM:        00000501 / ACPI_CNTL 80
GPIO:      00000581 / GPIO_CNTL 10
FD/CG:     02000001 / 00000000
PPO/MAP:   0000 / 00000000
UPRWC:     0000
GPIO USE:  197e75ff / 030300ff
GPIO DIR2: 0f55fff0, GPIO57 input
GPIO LVL2: configured-output mask 0002000f as measured; GPIO57 low
EHCI:      both exact identities/classes, CMD 0002, valid assigned MMIO BAR,
           D0, EHCIIR2 2002170a, legacy tuple
           00000001/c0040000/00000000, exact capabilities,
           USBCMD 00080000, USBSTS 00001004, USBINTR 0, CONFIGFLAG 0
UHCI:      all six exact identities/classes, CMD 0001, valid assigned I/O BAR,
           LEGKEY 2f00, C8/CA 0, USBCMD 0, USBSTS 0020, USBINTR 0
ports:     live OCA must be set on all twelve EHCI and all twelve companion
           UHCI views
```

The full measured pre-release port words (`3030` EHCI and `0c80` UHCI) remain
diagnostic references, not equality gates. OCC/OCI are sticky history and
CCS/CSC/PE/LSDA are asynchronous attachment state; none may reject the boot.
Only live OCA is an electrical admission/release condition.

After admission, B06WG performs:

```text
1. byte-RMW GP_IO_SEL2 bank-2 MSB: clear only bit 1 (GPIO57 output)
2. require direction target and every direction-neighbor bit to read back
3. require GPIO57 still low
4. sample all EHCI/UHCI port views
5. udelay(0x10000): 65,536 calibrated microseconds
6. byte-RMW GP_LVL2 bank-2 MSB: set only bit 1 (GPIO57 high)
7. require the target bit to read back high
8. sample at high +0, +10, +100 and +500 ms
9. revalidate fixed platform/controller state and require every live OCA clear
10. retain GPIO57 output/high through ACPI into SeaBIOS
11. immediately before payload, revalidate output/high, controller state and
    all-OCA-clear a second time
```

The ICH10 GPIO interface has byte-granularity I/O. The implementation computes
the written byte from the immediate read and changes only target bit 1, but the
physical `outb` necessarily rewrites the complete bank byte. Direction
neighbors are checked exactly. `GP_LVL2` native/input positions may change
asynchronously, so only GPIO57 and the already configured output-level mask are
asserted; unrelated/native input bits are logged.

The earlier XRS operand `0x10000` meant 65,536 uncalibrated `pause`
instructions. B06WG's numerically equal `udelay(0x10000)` is explicitly
65.536 ms and is not claimed to reproduce vendor timing.

## Failure and recovery contract

Any invalid preflight stops without a hardware mutation. Any post-mutation
failure attempts GPIO57 low, then input, but only after re-reading and
revalidating the LPC identity, GPIO decode, GPIO ownership and non-target
direction bits. A lost or changed decode therefore causes no blind rollback
write. Whether rollback succeeds or not, POST returns to terminal `9e`, serial
is flushed, and the payload is blocked with `die()`.

There is no late interactive monitor in B06WG. The inherited early CAR
ROMMON/reset guard and all B06VL memory/QPI fail-closed paths remain present.
For a late failure, use the established one-second AC interruption or the
socketed known-good flash chip. Do not attach valuable USB storage during the
first run.

POST codes:

| Code | Meaning |
|---|---|
| `98` | automatic late callback entered |
| `99` | exact static/controller preflight plus all-OCA-active admitted |
| `9a` | GPIO57 verified output/low |
| `9b` | GPIO57 verified high |
| `9c` | retained output/high and all live OCA clear |
| `9d` | final pre-payload gate passed |
| `9e` | terminal failure; payload blocked |
| `9f` | rollback momentarily completed; terminal code is then restored to `9e` |

## First hardware test

Use the same fixed E5645, sole-SPD-`0x54`, HD 5450 and RTL8168 configuration
as the qualified predecessor. Attach one known USB-2.0 keyboard before power-on
and arm COM1 at 115200 8N1 before the cold start. No ROMMON command is needed;
the GPIO transaction is automatic.

An example read-only capture command on the ConsolePi is:

```bash
python3 scripts/capture_x58_uart.py \
  --device /dev/ttyUSB1 --baud 115200 \
  --output research/msi/captures/2026-09-07-b06wg-cold-01.raw \
  --quiet 1 --timeout 300
```

The minimum successful trace is:

```text
[RAMINIT] EXPERIMENTAL B06WG FAST_POSTMEM: RAM/QPI ASSUMED_STABLE ...
[RAMINIT] B06WG FAST_POSTMEM PASS ...
[USB-AUTO] B06WG-AUTO-GPIO57-USB1 BEGIN ...
[USB-AUTO] GPIO57-RELEASE-GATE ... OCA=000 ... OCA=000 ...
[USB-AUTO] B06WG-AUTO-GPIO57-USB1 PAYLOAD ADMIT ...
SeaBIOS ...
USBTRACE-BEGIN ...
```

The milestone is not merely POST `9d`. Level 8 should make the complete
SeaBIOS USB trace observable without the INT 16h polling flood. Retain it and
require evidence of a successful device descriptor/address/configuration, HID
keyboard binding, and an accepted key. One successful boot remains an
experiment; the project validation policy still requires repeated cold boots.
Record the elapsed time from the first `FAST_POSTMEM` line to ramstage so the
actual speedup from the retained single clear pass can be measured.

Build with:

```bash
./scripts/build_x58_b06wg.sh
```

The W25Q128 image is
`blobs-local/msi-x58-pro-e/b06wg/msi-x58-pro-e-b06wg-deterministic-w25q128-16MiB.rom`.
It contains local user-supplied MSI modules and is ignored/non-redistributable;
only the public 4-MiB coreboot base belongs in a public repository.

## Hardware update

One 2026-09-07 run completed the full automatic path and both GPIO57/OCA
gates. SeaBIOS initialized the HD 5450 display, a USB keyboard and a Transcend
128GB USB mass-storage device; the operator confirmed working USB-keyboard
input. RTL8168 iPXE reached link-up, DHCP and a successful 388848-byte TFTP
chainload. A subsequent local Hiren's/Windows PE attempt performed sustained
large EHCI reads, entered an external loader and reached a graphical BSOD
before automatic reset. Its STOP code was not captured.

This supersedes only the former target-untested statement. It does not turn the
experimental sparse post-memory path into a full-RAM qualification or satisfy
the ten-run cold-boot criterion. See the immutable
[hardware record](../research/msi/b06wg-usb-boot-hw-2026-09-07.md) for capture
hashes, iPXE error decoding and exact non-claims.
