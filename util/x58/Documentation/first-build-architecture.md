> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# First coreboot build architecture

Status: implementation and progression record, updated 2026-08-31.  B00,
B01, and B02 have one user-reported terminal `df` result each.  B03 has one
user-reported terminal `de` and is the first image to enter romstage.  B04 has
one user-reported terminal `dd`; its formal cold/stability/repetition evidence
remains open.  B05 is byte-reproducible and statically audited; its first
reported hardware run reached exact `DEV_ERR` terminal `fa`, not success code
`dc`.  B06A is the bounded pin/address diagnostic response to that result.
Two fresh ccache-disabled builds produced the same 4-MiB raw ROM, SHA-256
`83100b39b15997576f0e7a599a0e8ab5c1e78fdf1cff6f204926620e206e930e`.
It has not yet been hardware-tested.

## Decision

The first flashable image should be a real 4 MiB coreboot ROM, but should stop
deliberately in bootblock after proving only the reset path, cache-as-RAM,
ICH10R LPC decode, Fintek UART, and diagnostics:

```text
CPU reset vector
  -> coreboot x86 16-bit entry                POST 0x01
  -> 32-bit protected-mode entry              POST 0x10
  -> Intel non-evict cache-as-RAM              POST 0x21..0x2f
  -> mainboard C-in-CAR marker; no X58 access  POST 0xc0
  -> read and verify ICH10R 8086:3a16          POST 0xc1,0xd0
  -> snapshot and program narrow LPC decode    POST 0xc2
  -> identify F71882/83 shared-ID family       POST 0xd1,0xd2
  -> enable/read back COM1 at 0x3f8            POST 0xd3
  -> UART scratch test, loopback, serial report POST 0xd4
  -> intentional CLI/HLT loop                  POST 0xdf
```

This image must not initialize DRAM or QPI, program PCIEXBAR, enumerate PCI,
enter ramstage, run SMM, execute an option ROM, or launch a payload.  The
purpose is to turn the first hardware test into a small set of independently
observable facts instead of attempting the whole platform at once.

`PAYLOAD_NONE` alone is insufficient.  The board hook must explicitly halt
before `run_romstage()`; a fail-closed romstage stub must also halt if it is
ever reached accidentally.

## Evidence behind the design

The following combine **verified-static** image observations with one explicit
user-reported provenance fact:

- the image is exactly 4 MiB and the reset vector is at raw offset
  `0x3ffff0`;
- **user-reported provenance:** the file was compared with the target EEPROM
  contents and reported byte-identical;
- the image has no Intel Flash Descriptor signature and no identified ME or
  GbE region, so the target ROM is descriptorless rather than an IFD image;
- the top 256 KiB bootblock maps at `0xfffc0000..0xffffffff`;
- MSI establishes a 64 KiB non-evict CAR window at
  `0xfff80000..0xfff8ffff` and a write-protected XIP MTRR for the bootblock;
- MSI uses MSR `0x2e0`, fills/tests all 64 KiB of CAR, and places the early
  stack near `0xfff8ffc0`;
- the legacy `RUN_CSEG` enters Fintek configuration mode at index port `0x4e`
  using two `0x87` writes and exits with `0xaa`;
- Setup identifies the Super I/O as F71882F; coreboot `superiotool` maps ID
  `0x4105` to F71882FG/F71883FG and COM1 to logical device `0x01`.

These observations are detailed in
[MSI early-boot evidence](../research/msi/early-boot.md).

## Flash layout

Build one complete descriptorless `coreboot.rom`:

```text
raw 0x000000..0x3fffff    4 MiB coreboot ROM/CBFS
CPU-visible top alias     0xffc00000..0xffffffff
reset vector              0xfffffff0
```

Do not configure Intel descriptor mode and do not splice in ME, GbE, or IFD
objects.  This conclusion applies to the user-verified EEPROM image, not to
every MS-7522 variant or to any unidentified second storage device.

The first image should select the normal 4 MiB board ROM size and the policy
that forbids early SPI writes.  CBFS reads remain necessary.  No CMOS option
table, persistent event log, or early MRC cache should be enabled.

## Upstream baseline and reusable code

The implementation should be developed as a small patch series on a current
coreboot checkout.  The relevant upstream paths inspected here are unchanged
between local audit commit `78d314b7c3abd383c661f4f9d1533eb0fb4e6031` and
the then-current remote main commit
`c6c871909a1200f83ed5e324e6c1ad0f8c1591f9`.

Reuse:

- `src/cpu/x86/entry16.S` and `entry32.S`;
- `src/cpu/x86/early_reset.S`;
- `src/cpu/intel/car/non-evict/cache_as_ram.S`;
- `src/cpu/intel/car/non-evict/exit_car.S`;
- `src/cpu/intel/car/bootblock.c` and `romstage.c`;
- `src/southbridge/intel/i82801jx/` for ICH10R;
- `src/superio/fintek/common/early_serial.c` for pre-RAM serial;
- the standard 8250 I/O UART driver and port-`0x80` POST path.

Do not reuse register programming from the former `intel/nehalem` directory.
That code was renamed to `intel/ironlake` because it is Arrandale/Ironlake.
Likewise, `model_1067x`, LGA775 CAR, and X4X DDR3 raminit are not Bloomfield/X58
implementations.  They are at most organizational references.

## Proposed source ownership

```text
src/mainboard/msi/x58_pro_e/
  Kconfig
  Kconfig.name
  Makefile.mk
  board_info.txt
  bootblock.c             Fintek/UART, build ID, deliberate stop
  romstage.c              stage dispatcher; fail-closed initially
  devicetree.cb           minimal compile-time topology

src/northbridge/intel/x58/
  Kconfig
  Makefile.mk
  bootblock.c             initially a POST-only/no-write hook
  x58.h                   only established IDs/fields
  chip.h
  romstage.c              later IOH/QPI coordination
  memmap.c                later address map/resources
  northbridge.c           later ramstage device operations

src/cpu/intel/socket_LGA1366/
  Kconfig
  Makefile.mk             owns common non-evict CAR inclusion once

src/cpu/intel/model_206cx/
  Kconfig
  Makefile.mk
  ...                     first E5645/CPUID 0x206c2 target

src/cpu/intel/model_106ax/
  ...                     Bloomfield only after the E5645 path is stable

src/superio/fintek/f71882fg/
  ...                     full ramstage driver later, not needed for build 0
```

Only the mainboard and E5645 CPU/socket subsets exist in B00--B06A.  The X58
northbridge and full F71882FG-family driver entries above remain future
ownership targets, not linked implementations.

The CPU model code owns CPUID matching, microcode loading, and CPU driver
behavior.  The socket layer owns the common CAR path so selecting CPU models
cannot link it twice.  The CPU-integrated IMC/Uncore implementation may remain
below the CPU model or be split into a common Nehalem Uncore directory once the
shared boundary is understood.  Board SPD topology, voltage/clock policy, and
slot constraints remain in mainboard code.  X58 IOH QPI/DMI/PCIe behavior
belongs in `northbridge/intel/x58`.

## Implemented CAR layout

B00--B06A use the generic Intel non-evict implementation with:

```text
CONFIG_DCACHE_RAM_BASE=0xfff80000
CONFIG_DCACHE_RAM_SIZE=0x00010000
CONFIG_DCACHE_BSP_STACK_SIZE=0x00004000
```

The base and size are not borrowed from another chipset: they reproduce the
MSI bootblock.  A 16 KiB BSP stack is a conservative initial allocation within
the verified 64 KiB window.  Stack high-water usage should be measured; it may
later be reduced.  Do not use the unrelated Ironlake address
`0xfefc0000`.

The generic assembly already supplies the useful `0x21..0x2f` diagnostic
codes.  Keep them unchanged so a last-code trace identifies the exact CAR
phase.  The generic early-reset path is also useful after a CPU-only reset
leaves CAR/MTRR state behind.

Build 0 should not set or write PCIEXBAR.  A likely ECAM base such as
`0xe0000000` belongs to a later read/validate/write experiment, not the CAR
milestone.

## ICH10R constraints and implemented delta

The initial proposal considered selecting the complete upstream `i82801jx`
bootblock path.  B00--B06A deliberately do not do that: it would also enable
SPI caching, broad LPC/SERIRQ policy, RCBA MMIO, and other board policy that is
not yet established for the MSI board.

The implemented progression is narrower:

- B01 reads PCI `00:1f.0` and requires exact ICH10R LPC ID `8086:3a16`;
- B02 board code directly programs only
  `LPC_IO_DEC=0x0010`, `LPC_EN=0x2001`, and no generic LPC windows;
- B03 adds no ICH register writes;
- B04 moves the existing `i82801jx_setup_bars()` primitive into a reusable
  early-core source file and calls only it in romstage, setting RCBA
  `0xfed1c001`, PMBASE `0x0501` plus ACPI enable, and GPIOBASE `0x0580` plus
  GPIO enable;
- B04 separately calls the common `smbus_enable_iobar(0x400)` helper, but never
  accesses a programmed I/O/MMIO window and performs no SMBus transaction;
- B05 preserves B04, requires runtime CPUID `0x000206c2`, and performs one
  bounded I801 byte-data read from address `0x50`, command `0x02`, expecting
  DDR3 type byte `0x0b`;
- B06A additionally records `SMBus_PIN_CTL`, writes exactly `0x04` to set only
  documented `SMBCLK_CTL`, requires clock and data high, and issues one bounded
  type-byte command at each `0x50..0x57`.  Exact `DEV_ERR` clears and advances;
  it is not treated as proof of an empty slot.

Every B04 register is guarded by live-window/cold-default preflight and
read-back checks.  The complete `SOUTHBRIDGE_INTEL_I82801JX` selection,
`i82801jx_early_init()`, SPI, broad LPC/SERIRQ, GPIO pin policy, IOAPIC, SMM,
and TCO/watchdog code remain absent.  In particular, the suspicious existing
eight-bit operation at PCI config offset `0xac` is not linked or executed.

TCO handling remains a future isolated experiment.  B00--B06A do not claim to
stop the watchdog; a stable terminal code must be timed and any reset reported.

## Defensive Fintek/UART hook

The correct static port is `0x4e`; do not probe or program both `0x2e` and
`0x4e`.  Before modifying the UART logical device, read and log the Fintek ID
and vendor ID.  The existing tool database gives raw register bytes
`20=05, 21=41, 23=19, 24=34`, aggregated by `superiotool` as device ID
`0x4105` and vendor ID `0x3419`.  Comparing the four bytes avoids mixing
datasheet and little-endian display conventions.  Device ID `0x4105` denotes
the shared F71882FG/F71883FG family and does not distinguish the package.

Conceptual board flow:

```c
#define SERIAL_DEV PNP_DEV(0x4e, 0x01)

void bootblock_mainboard_early_init(void)
{
	post_code(POST_FINTEK_BEGIN);
	if (!f71882fg_id_matches(0x4e)) {
		post_code(POST_FINTEK_ID_ERROR);
		halt();
	}
	fintek_enable_serial(SERIAL_DEV, CONFIG_TTYS0_BASE);
	post_code(POST_FINTEK_READY);
}
```

The small ID reader should use the same `0x87, 0x87` entry and `0xaa` exit
sequence as the common helper and must leave configuration mode on every path.
The full F71882FG ramstage driver can wait.  Selecting another Fintek model's
driver merely because it compiles would encode false hardware assumptions.

The serial header's electrical level and pinout remain hardware-unverified.
Do not attach an RS-232 adapter until it is established whether JCOM1 carries
TTL-level UART or level-shifted RS-232.

## Board Kconfig policy

The implemented B06A board policy selects or defines:

```text
BOARD_ROMSIZE_KB_4096
CPU_INTEL_SOCKET_LGA1366
MISSING_BOARD_RESET
NO_ECAM_MMCONF_SUPPORT
NO_MONOTONIC_TIMER
NO_SMM
SUPERIO_FINTEK_COMMON_PRE_RAM          stage 2 and later
SOUTHBRIDGE_INTEL_I82801JX_EARLY_CORE stages 4 through 6
SOUTHBRIDGE_INTEL_COMMON_EARLY_SMBUS  stages 4 through 6
```

It does not select `NORTHBRIDGE_INTEL_X58` because that implementation does not
exist yet, and it does not select the complete
`SOUTHBRIDGE_INTEL_I82801JX`.  CBFS is read from the memory-mapped SPI image;
the executed B00--B06A paths contain no SPI write.

The checked-in B05 defconfig expresses:

```text
CONFIG_VENDOR_MSI=y
CONFIG_BOARD_MSI_X58_PRO_E=y
CONFIG_X58_PRO_E_BRINGUP_STAGE=5
CONFIG_PAYLOAD_NONE=y
CONFIG_POST_IO=y
CONFIG_POST_IO_PORT=0x80
CONFIG_BOOTBLOCK_CONSOLE=n
CONFIG_CONSOLE_SERIAL=y
CONFIG_CONSOLE_SERIAL_115200=y
CONFIG_UART_FOR_CONSOLE=0
CONFIG_COLLECT_TIMESTAMPS=n
CONFIG_NO_SMM=y
CONFIG_LOCALVERSION="x58-pro-e-b05"
```

The B06A defconfig changes only the stage and build identity:

```text
CONFIG_X58_PRO_E_BRINGUP_STAGE=6
CONFIG_LOCALVERSION="x58-pro-e-b06a"
```

The resolved configuration retains coreboot's pre-RAM CBMEM console buffer in
CAR; the static CAR audit includes it and still leaves 28,816 bytes free.  No
post-CAR or ramstage path is permitted to run.  Timestamps are disabled because
the current CPU port has no verified timebase.  VGA/ACPI/option-ROM defaults in
unreachable ramstage do not imply that B05 or B06A executes those facilities.

The verified installed E5645 determines the first CPUID Kconfig selection:
`model_206cx` for CPUID `0x206c2`.  Do not enable the Bloomfield family until
that path is stable.  Obtain microcode through the usual locally supplied
Intel microcode package/submodule; do not commit the copy extracted from MSI
firmware.

### Minimal devicetree rule

The implemented B00--B06A devicetree is intentionally only:

```text
chip mainboard/msi/x58_pro_e
  device cpu_cluster 0 on end
  device domain 0 on end
end
```

The stage code addresses `00:1f.0` and `00:1f.3` explicitly before DRAM.  The
complete ICH10 ramstage driver remains off and therefore must not be implied by
invented device nodes.  Before that driver is enabled, add every node it
requires, introduce a verified F71882FG-family driver/node rather than a fake
related model, and audit the full board policy.

## Build-0 code behavior

There is no X58 northbridge hook in B00--B06A.  POST `c0` is emitted at the
start of `bootblock_mainboard_early_init()` and proves C execution in CAR.  The
same hook performs the staged LPC/Fintek/UART gates.  B00--B02 then stop at
`df`; B03/B04/B05/B06A return to the generic BIST gate, and
`bootblock_mainboard_init()` emits `c3` immediately before `run_romstage()`.

The code embeds and, from B02 onward, transmits an immutable build identifier.
It reports that QPI/DRAM remain untouched.  No TCO/watchdog operation is
present; terminal stability is an observation to record, not a claimed setup
step.

The build identifier should also be findable as an ASCII string in the ROM so
a programmed chip can be identified offline.  Each hardware image gets a new
identifier and recorded SHA-256.

For stages B00 through B02, `mainboard_romstage_entry()` emits error code `ee`
and halts if reached unexpectedly.  B03 deliberately enters it, emits its own
markers, and halts at `de`.  B04 uses the same XIP/CAR entry, performs its
bounded ICH10R PCI-configuration experiment, and halts at `dd`.  B05 preserves
that experiment, performs its single bounded SPD type-byte read, and halts at
`dc`.  B06A instead initializes only the SMBus clock-release control, checks
pin levels, scans eight type-byte addresses, reports a terminal result, and
halts.  None of these paths may return to generic `romstage_main()`, because
the latter prepares the
post-CAR transition and assumes that platform memory initialization has
completed.

## POST-code contract

Keep generic coreboot codes and reserve board/platform ranges:

| Code | Meaning |
|---:|---|
| `0x01` | reset vector reached |
| `0x10` | 32-bit entry reached |
| `0x21..0x2f` | existing Intel non-evict CAR phases |
| `0x41..0x48` | B06A terminal: exactly one DDR3 type-byte responder at `0x50..0x57`, respectively |
| `0x49` | B06A terminal: multiple DDR3 type-byte responders; bitmap is in the serial report |
| `0x4a` | B06A terminal: no exact successful response; all completed probes were exact `DEV_ERR` |
| `0x4b` | B06A terminal: at least one exact successful response returned a type other than `0x0b` |
| `0x4c/0x4d/0x4e` | B06A terminal: clock/data/both pin conditions invalid |
| `0x4f` | B06A initial host ownership/state or command read-back invalid |
| `0x50` | B06A transaction poll bound expired; no KILL/release/recovery attempted |
| `0x51` | B06A exact `BUS_ERR` result |
| `0x52` | B06A FAILED, combined, or unexpected transaction result |
| `0x53` | B06A good scan result, but final UART report/`TEMT` failed |
| `0x54/0x55/0x56` | B06A pin phase entered / `SMBCLK_CTL` written / both pins observed high |
| `0x57` | B06A CPUID leaf-1 EAX was not exactly `0x000206c2` |
| `0x80..0x87` | B06A immediately before START at `0x50..0x57` |
| `0x88..0x8f` | B06A exact `DEV_ERR` at `0x50..0x57`; cleared and next address follows |
| `0x90..0x97` | B06A exact success and DDR3 type `0x0b` at `0x50..0x57` |
| `0x98..0x9f` | B06A exact success with another type at `0x50..0x57` |
| `0xc0` | mainboard early hook reached in CAR; no X58 access performed |
| `0xc1` | read-only ICH10R PCI-config access started |
| `0xc2` | B02 narrow LPC decode/readback started |
| `0xc3` | B03/B04/B05/B06A BIST gate passed; normal CBFS loader handoff follows |
| `0xc4` | B04/B05/B06A ICH10R/SMBus snapshots and preflight begun |
| `0xc5` | B04/B05/B06A RCBA/PMBASE/GPIOBASE read-backs and LPC invariants passed |
| `0xc6` | B04/B05/B06A SMBus controller BAR/HOSTC/PCI-command read-backs passed |
| `0xc7` | B05 I801 host/semaphore/cold state accepted |
| `0xc8` | B05 command setup read back; the only START follows immediately |
| `0xc9` | B05 transaction accepted, DAT0 read, and host released |
| `0xd0` | exact ICH10R ID accepted |
| `0xd1` | narrow LPC decode passed; Fintek ID access started |
| `0xd2` | Fintek ID bytes `05,41,19,34` accepted |
| `0xd3` | UART LDN 1 enabled at `0x3f8` and read back |
| `0xd4` | internal UART tests and bounded banner transmission completed |
| `0xd5` | B03/B04/B05/B06A XIP `mainboard_romstage_entry()` reached |
| `0xd6` | bounded B03 romstage marker and UART `TEMT` completed |
| `0xd7` | bounded B04/B05/B06A register report and UART `TEMT` completed |
| `0xd8` | bounded B05 success report and UART `TEMT` completed |
| `0xdc` | expected B05 success halt after reading exact DDR3 type `0x0b` |
| `0xdd` | expected B04 success halt in romstage |
| `0xde` | expected B03 success halt in romstage |
| `0xdf` | expected B00/B01/B02 success halt in bootblock |
| `0xe0` | B05 CPU signature was not exactly `0x000206c2` |
| `0xe1` | no PCI-config response from ICH10R BDF |
| `0xe2` | unexpected ICH10 LPC device ID |
| `0xe3` | unexpected Fintek device-ID bytes |
| `0xe4` | unexpected Fintek vendor-ID bytes |
| `0xe5` | active generic LPC decode or narrow LPC read-back failure |
| `0xe6` | UART logical-device read-back failure |
| `0xe7` | UART scratch-register or internal-loopback failure |
| `0xed` | bounded UART transmit/flush timeout |
| `0xee` | forbidden post-CAR/cbmem-top path or pre-B03 romstage reached |
| `0xf0` | B04 RCBA live-base preflight or read-back failed |
| `0xf1` | B04 SMBus function absent (`0xffffffff`) |
| `0xf2` | B04 SMBus function is not exact `8086:3a30` |
| `0xf3` | B04 SMBus cold-default precondition/helper/read-back failed |
| `0xf4` | B04 PMBASE/ACPI live-base preflight or read-back failed |
| `0xf5` | B04 GPIOBASE/GPIO-control live-base preflight or read-back failed |
| `0xf6` | B04 narrow-LPC/SERIRQ/generic-window invariant failed |
| `0xf7` | B04 bounded UART report or `TEMT` timed out |
| `0xf8` | B05 initial I801 state or command read-back invalid |
| `0xf9` | B05 poll limit reached; no KILL/release/recovery attempted |
| `0xfa` | B05 exact `DEV_ERR` result; address `0x50` may not have acknowledged |
| `0xfb` | B05 exact `BUS_ERR` result |
| `0xfc` | B05 FAILED, combined, or otherwise unexpected terminal result |
| `0xfd` | B05 transaction succeeded, but data was not `0x0b` |
| `0xfe` | B05 successful data result, but final UART report/`TEMT` failed |

Reserve `0xa0..0xaf` for QPI progress and `0xe8..0xec` for QPI failures to
align later traces with the observed MSI CSI module.  Reserve `0xb0..0xbf` for
native DDR3 training.

## Patch series for the first image

Keep each hypothesis independently reviewable:

1. **CPU/socket skeleton:** add LGA1366 and one exact installed CPU signature;
   wire the existing non-evict CAR with the MSI-derived base and size.
2. **Mainboard skeleton:** 4 MiB descriptorless target, minimal devicetree,
   port-`0x80` diagnostics.
3. **Fintek path:** verify bytes `05,41,19,34`, enable LDN 1 at `0x3f8`,
   test UART locally, print a bounded banner, and halt.
4. **XIP romstage:** use the normal CBFS handoff and stop before post-CAR.
5. **ICH10 early-core split:** expose only the existing BAR helper to B04 and
   add common SMBus BAR/enable programming with preflight/read-back guards.
6. **Single I801 transaction:** add the exact-CPUID B05 probe with semaphore
   ownership, one START, bounded polling, explicit error terminals, and no
   timeout recovery guess.
7. **Bounded pin/address diagnostic:** B06A sets only documented SMBCLK
   release, verifies both pins high, and performs at most one type-byte START
   per standard SPD address while holding one host-semaphore acquisition.
8. **Reproducible config:** checked-in defconfigs and build/test record
   templates with a unique version string.

An X58 northbridge implementation, TCO stop, full ICH10 policy, and the
existing PCI-`0xac` width bug remain separate future work.

Before hardware use, inspect each final map and disassembly to prove that the
reset vector enters the intended CAR path and that execution stops at that
build's documented bootblock or romstage boundary.

## Build-0 acceptance test

Prerequisites remain: exact chip marking/voltage, external programmer cycle,
known-good original and spare, verified POST path, CPU identity, board
revision, and safe UART connection.

Success is:

- ten cold boots with the same POST sequence and final `0xdf`;
- ten reset-button/warm-reset attempts with an explained, repeatable trace;
- stable serial banner and identical CPUID/microcode values where UART works;
- no reset during the timed deliberate halt; TCO is not configured yet;
- no SPI write and no DRAM/QPI/PCIEXBAR access;
- the vendor chip remains untouched and the vendor image can be restored from
  the tested spare-chip workflow.

Last-code interpretation:

```text
0x10       failure before/at CAR setup
0x21..2f   exact generic CAR phase failed
0xc0       C-in-CAR worked; investigate southbridge path
0xc1       ICH10R PCI-config read was about to run
0xd0       exact ICH10R ID passed; investigate LPC path
0xc2       LPC setup started; investigate generic decode/write/read-back
0xd1       LPC passed; investigate Fintek ID access
0xd2       Fintek ID passed; investigate UART LDN setup
0xd3       UART LDN read-back passed; investigate local UART test
0xd4       UART banner path completed; external reception is wiring-dependent
0xc3       bootblock tail/BIST passed; investigate CBFS/XIP stage transfer
0xd5       romstage C entry reached; next is B03 d6 or B04/B05/B06A c4
0xd6       B03 marker flushed; next expected code is de
0xc4       B04/B05/B06A snapshots/preflight running; no new ICH write completed yet
0xc5       B04/B05/B06A ICH10R BAR read-backs passed; inspect SMBus configuration
0xc6       B04/B05/B06A SMBus BAR/enable read-backs passed; report is next
0xd7       B04/B05/B06A ICH10 report flushed; B06A next enters code 54
0xc7       B05 accepted the cold host state; command setup follows
0xc8       B05 command setup passed; the single START follows immediately
0xc9       B05 transaction succeeded and host was released; check type/report
0xd8       B05 success report flushed; next expected code is dc
0xdc       B05 read type byte 0x0b and reached its intentional halt
0xf9       B05 poll bound expired without recovery; remove AC power completely
0x54..56  B06A pin-control phase passed; first address probe follows
0x57      B06A rejected a CPUID other than exact 0x000206c2
0x80..9f  B06A address-qualified transaction progress; see the table above
0x41..53  B06A terminal result; 0x50 timeout requires complete AC removal
0xdd       B04 reached its intended configuration-register terminal state
0xde       B03 reached its intended romstage terminal state
0xdf       B00/B01/B02 reached the intended bootblock terminal state
```

## Controlled progression after build 0

Use an explicit integer/choice Kconfig bring-up stage so every experimental
ROM has one deterministic terminal point:

| Build | Added behavior | Required terminal proof |
|---|---|---|
| B00 | reset/CAR and C entry | `0xdf` halt |
| B01 | exact read-only ICH10R LPC-ID probe | `d0,df` |
| B02 | narrow LPC, Fintek, and internal UART proof | `d4,df` |
| B03 | load/enter XIP romstage from CBFS | `c3,d5,d6,de` halt in CAR |
| B04 | isolated existing ICH10R BAR helper plus SMBus BAR/enable registers | `c4,c5,c6,d7,dd`; exact read-backs, no window access or bus transaction |
| B05 | one I801 byte-data read of SPD `0x50:0x02` | `c7,c8,c9,d8,dc`; data exactly `0x0b` |
| B06A | documented pin release plus one type-byte probe per `0x50..0x57` | one explicit `0x41..0x53` result or `0x57` CPUID rejection; no dump or retry |
| B06B | fixed `0x54` DDR3 base-section read | bytes `0x00..0x7f` plus revision-selected CRC |
| B06C | fixed `0x54` declared-used bytes plus decode | double-read `0x80..0xaf`; non-programmed DDR3-800 JEDEC-cycle candidate |
| B06D | one-DIMM physical-slot correlation | measured address-to-silkscreen map |
| B07 | read-only CPU-Uncore/X58/QPI snapshot | IDs/status captured; no retraining |
| B08 | isolated, bounded QPI experiment | link status or exact timeout/error |
| B09 | one approved DIMM at DDR3-800 | deterministic training plus 256 MiB test |
| B10 | post-CAR, memory resources, essential PCI | stable GPU/storage enumeration |
| B11 | diagnostic payload | serial payload start and diagnostic boot |
| B12 | EDK2 payload | UEFI shell and sane memory map |

The B06A row refers to raw 4-MiB image SHA-256
`83100b39b15997576f0e7a599a0e8ab5c1e78fdf1cff6f204926620e206e930e`.
Two fresh ccache-disabled builds produced those exact bytes.  The first target
run reached terminal `0x45`, proving exactly one DDR3 type-byte response at
logical address `0x54` after all eight bounded scans completed.

### B03: prove the stage loader

Let bootblock call the normal CBFS romstage loader, then stop at the first line
of `mainboard_romstage_entry()`.  This proves CBFS access and the extended CAR
stack without pretending memory exists.

### B04: isolate the existing ICH10R early core

Link only `i82801jx_setup_bars()` and the common
`smbus_enable_iobar(0x400)` helper into romstage.  Do not select the complete
`SOUTHBRIDGE_INTEL_I82801JX` path.  B04 writes and reads back RCBA, PMBASE,
GPIOBASE, and the SMBus BAR/enable registers while preserving the proven narrow
LPC values, SERIRQ, and disabled generic windows.  An enabled RCBA/PM/GPIO
window at a different base fails before relocation; the SMBus function must
start at exact cold defaults.  B04 does not touch RCBA MMIO, SMBus status/data,
or SPD and stops at `dd`.

The user has reported terminal `dd` once, but not whether that run followed
complete AC removal.  The formal B04 cold-test cell therefore remains open.
A warm or CPU-only reset may intentionally stop at `f3` if SMBus PCI
configuration was retained.

### B05: read one SPD type byte without training

B05 now implements the smallest on-wire experiment.  It preserves the narrow
B04 controller setup, accepts only runtime CPUID `0x000206c2`, and reads one
byte from I801 address `0x50`, command/offset `0x02`.  Exact DDR3 type `0x0b`
is required for terminal `dc`.

The existing common I801 byte-read helper is not called because it depends on
`udelay(1)`, while the current model-206cx port has no verified pre-RAM
timebase.  B05 instead uses one START and at most 1,000,000 serialized status
reads.  That is an iteration bound, not a claimed duration.  It handles the
I801 host semaphore before touching other host registers, performs no retry,
and does not guess at KILL/recovery on timeout.  At `f9`, remove AC power.

Code `c8` marks the last boundary immediately before START; `c9` means the
transaction result was accepted, DAT0 was read, and the host was released.
Only `c9,d8,dc` proves the expected type byte.  `fa` is `DEV_ERR`, which can
simply mean that the current physical DIMM population does not expose a device
at `0x50`.  B05 is not a scan, complete dump, CRC, or slot-map result.

The first reported B05 hardware run stopped at exact `fa`.  Its DIMM
population and pre-DRAM pin state were not recorded, so that result is an
exact transaction classification rather than proof that address `0x50` was
physically absent.

### B06A: isolate SMBus pin state and logical addresses

B06A keeps the B05 CPU and B04 controller gates, acquires the I801 host once,
but uses its own terminal `57` for any CPUID other than exact `0x000206c2`.
It records `SMBus_PIN_CTL`, writes exactly `0x04`, and polls at most 100,000 pin
reads until control, clock, and data bits read back as `0x07`.  It then issues
one byte-data read of type offset `0x02` at each standard SPD address
`0x50..0x57`.  Each command has the same one-million-status-read bound.  There
is no retry, full SPD read, checksum, slot inference, IMC access, QPI access,
or DDR training.

An exact `DEV_ERR` is cleared and the next address is attempted.  Any
`BUS_ERR`, combined/unexpected result, or pin fault terminates with its own
code.  A transaction timeout deliberately performs no KILL, semaphore release,
or recovery; remove AC power after terminal `50`.  After eight completed
commands, terminals `41..48` identify one DDR3 responder, `49` reports more
than one, `4a` reports none, and `4b` reports a successful non-DDR3 type.

The vendor-booted Linux reference answered at `0x50`, `0x52`, and `0x54` and
showed runtime `SMBus_PIN_CTL=0x07`.  That observation motivates the pin
diagnostic but does not prove the reset/pre-DRAM value or B05's DIMM
population.

### B06B: validate the DDR3 base section without training

B06B fixes the logical address at the B06A result `0x54`, reads offsets
`0x00..0x7f` once each, requires DDR3 type `0x0b`, and validates the JEDEC
base-section CRC using the coverage selected by SPD byte 0.  It neither reads
the upper SPD half nor infers a physical slot.  Missing, malformed, or corrupt
SPD ends at an explicit code rather than a hang or broad retry.

The reproducible image deliberately leaves the working autonomous Slow-QPI
link untouched.  The controlled ratio-6 comparison showed that High-Speed QPI
requires a coupled CPU/IOH PHY, reset, calibration, and training sequence;
post-training register values are therefore not replayed in B06B.

### B06C: verify declared-used bytes and decode a candidate

After B06B proves the fixed device's base block, accept only the observed
DDR3 byte-0 declaration of 176 used bytes in a 256-byte device.  Read offsets
`0x80..0xaf` twice because they are outside the stored base CRC, require
byte-for-byte equality, and leave `0xb0..0xff` including XMP unread.  Then use
the existing coreboot DDR3 decoder after a fail-closed raw-structure and
time-base guard, then derive a DDR3-800 JEDEC cycle candidate for the observed
4-GiB 2R x8 envelope.  This is computation and logging only; the cycle tuple
is not an X58 register encoding and the IMC remains untouched.

### B06D: correlate the physical slot

Move exactly one DIMM among physical slots, one cold test at a time, and map
the logical SPD address to the board silkscreen.  Keep this physical-topology
hypothesis separate from B06C's fixed-address data validation.

### B07: snapshot, do not train QPI

Record CPU signature, all accessible CPU-Uncore device/function IDs, current
PCIEXBAR state, X58 IDs, QPI link status, reset cause, and relevant MSRs after
each boot type.  MSI code references high buses such as `0xff`/`0xfe`, but the
actual device map must be observed before any write.  Do not copy Ironlake
PCIEXBAR or QPI offsets.

### B08: isolated, bounded QPI experiment

Only after the B07 snapshots are stable should one documented or correlated
QPI hypothesis be changed at a time.  Keep the diagnostic path alive wherever
possible, emit a unique code immediately before the write, poll named status
with a fixed bound, and cap any persistent reset request.

The X58 datasheet says a link leaves reset at a slow initial rate and firmware
programs the operational rate and may soft-reset the PHY.  Therefore a QPI
change can temporarily remove the IOH/LPC diagnostic path.  B08 needs a final
port-`0x80` code immediately before the change, bounded polling, and a capped
persistent reset counter.

### B09: narrow native DDR3 target

The first supported memory policy should be deliberately fixed:

```text
one known E5645 CPUID `0x206c2`, stepping 2
one verified channel-0/A0 slot
one 1R x8 non-ECC UDIMM
DDR3-800, 1.5 V, JEDEC only
no XMP, no mixed slots/ranks, no overclocking
```

The implementation architecture is:

```text
SPD bytes
  -> pure policy decoder (geometry/timings; rejects unsupported topology)
  -> board policy (slot map, clocks, voltage, reset wiring)
  -> CPU Uncore/IMC state machine
       geometry/address decode
       safe clock/ratio and timing programming
       DIMM reset, CKE, MRS2/MRS3/MRS1/MRS0, ZQCL
       read DQ/DQS
       receive enable
       write leveling
       write DQ/DQS
       bounded completion/status per rank and lane
  -> SAD/TAD/memory map construction
  -> destructive test of a deliberately limited range
  -> post-CAR transition only after validation
```

The public Xeon 5500 Volume 2 register descriptions are a close semantic
reference, not proof of desktop Bloomfield register identity.  Every device
ID, register field, analog seed, ordering rule, and reset consequence must be
correlated with MSI/Intel disassembly and real vendor-initialized captures.
Unknown fields keep neutral names.

## Payload architecture

SeaBIOS should be the first payload after stable DRAM and PCI because it gives
a smaller diagnostic surface.  A discrete GPU may require its own locally
supplied option ROM for visible legacy VGA; this is a GPU dependency, not an
X58 initialization blob.

EDK2 should then be integrated through coreboot's supported Universal Payload
path.  EDK2 consumes the established memory map, ACPI/SMBIOS information, PCI
resources, and initialized devices.  It must not contain an Intel desktop-X58 PEIM
or be expected to initialize Bloomfield DDR3/QPI.

## Binary-object policy for this architecture

| Object | B0 | Native final path |
|---|---:|---|
| CPU microcode for exact CPUID | local licensed input | required through normal coreboot mechanism |
| Intel descriptor / ME / GbE region | absent | absent for this verified descriptorless EEPROM layout |
| MSI `CSI_INITDLL` / `MINITDLL` | excluded | excluded |
| Intel `63C0690C...` PEIM | excluded | excluded; semantic reference only |
| RAID, PXE, JMicron option ROMs | excluded | optional/non-goal, never platform prerequisites |
| GPU option ROM | excluded | conditional for legacy VGA only |
| SeaBIOS / EDK2 | excluded | built from source after platform init works |

An experimental MSI vendor-assisted branch may be useful only as a local,
hash-bound research instrument.  It must be named visibly, cap the observed
`0xe801` reset request, document the `0xe0` policy and `0x2bcc` workspace
contract, never redistribute extracted modules, and remain separate from the
native release path.

## Information still required before flashing B0

The architecture is implementable now, but safe physical testing still needs:

- exact board revision;
- exact flash-chip manufacturer, part, package, and voltage;
- completed external erase/write/read-back/restore cycle on a spare;
- actual CPU model, S-spec, CPUID, stepping, and current microcode revision;
- verified port-`0x80` observation hardware;
- JCOM1 pinout and electrical level.

For B2 and later, additionally capture the exact DIMM model/slot and the full
vendor-booted state listed in [implementation-plan.md](implementation-plan.md).
