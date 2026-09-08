> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WK-HW-02: retry with operator-reprepared Intenso

## Start and recovery record

The operator reports that the Intenso USB medium has now been prepared
correctly and requests another boot attempt. The prior medium was described
as Parted Magic written with dd; the exact replacement ISO version/hash and
conversion/write command have not been provided. No image conversion or
USB-device write is performed by the agent in this run.

```text
test ID: B06WK-HW-02
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta
observed image ID: X58PROE-B06WK-ACPI-REPAIR-20260908
intended ROM hash: bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash chip: socketed W25Q128.V..M, 16 MiB; inherited inventory
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645; CPUID 000206c2 observed
CPU stepping: 2
microcode revision: 0000001f observed in vendor probe
DIMM model: BLS4G3D1609DS1S00, 4 GiB 2Rx8; sole SPD54 captured
DIMM slot: sole SPD54; physical slot label not restamped
GPU: Radeon HD5450 1002:68f9, fixed configuration
PSU: unchanged; model not restamped
boot type: one target-only one-second Shelly AC cycle; no independent electrical G3 measurement
POST trace: serial c3 and automatic CSI/MINIT path; no separate POST-card capture
serial: operator@serial-gateway.example.invalid /dev/ttyUSB1, COM1 115200 8N1
serial log: captures/b06wk-intenso-retry-boot-20260908-02.raw (final capture archived after completion)
result at this checkpoint: automatic start in progress; boot-medium result pending
recovery required: one controlled mains cycle, no flash or manual guard write
notes: no added RAM/QPI test, no guessed BIOS disk mapping, no installation
```

Initial passive capture stopped its replay guard after 1,820 bytes of
repeated `rommon> nfo, script status` text. The following bounded empty-line,
`id`, `vinputs` exchange contains a repeated prefix too, but ends in the
coherent WK identity and `CMOS_DIAG_0E=2c VALID=01`. No `autoguard clear`
was necessary or issued. Both raw observations are retained separately.

The unchanged name-matching Intenso selector was armed at
**11:32:52.748744 UTC** before the AC cycle, with a 600-second deadline,
project lock, exclusive UART and raw-byte capture. It sends only ESC after
the SeaBIOS invitation, then the actual matching Intenso menu digit.

The single target relay request was:

```text
http://192.0.2.215/rpc/Switch.Set?id=0&on=false&toggle_after=1
response: {"was_on":true}
```

Preflight status reported output on / 73.1 W; follow-up output on / 74.3 W.
The return timer was local to the Shelly. ConsolePi was neither rebooted nor
power-cycled. Fresh WK reset output followed with `ENTRY=COLD_DEFAULT`,
`I801_SIG=00:00:00:00:00` and `CMOS0E=2c`, then the inherited three-phase
vendor-assisted initialization and automatic single-clear path. This is
ordinary boot progression, not a new RAM/QPI qualification exercise.

The boot-medium outcome and final capture hashes are appended below after
selection. Earlier B06WK-HW-01 media failure evidence remains unchanged.

## Completed capture: Intenso bootloader and Linux setup reached

The 600-second capture ended normally at **11:42:52.757308 UTC**, exit 0,
releasing the UART. Only the two intended boot-menu bytes were transmitted:

```text
11:39:01.539375 UTC  ESC (1b), after SeaBIOS invitation
11:39:01.657449 UTC  2 (32), after matching Intenso by name
```

The actual menu was Crucial SATA / Intenso / JetFlash / iPXE. Intenso
`Ultra Line 8.01`, removable SCSI type 0, 512-byte sectors, 61,440,000
sectors, was mapped from drive structure `000f21f0` to BIOS disk 0.
Neither JetFlash nor SATA was selected. No key was sent to the subsequent
ISOLINUX menu; its default entry ran after its own countdown.

The immutable raw capture confirms:

```text
[ACPI] B06WK-REPAIR1 FADT_FLAGS=00000065 FIXED_PWRBTN=1 PM_EVENTS_LEFT_MASKED=1 CTBL_CONSUMER_FLAG=01
Booting from Hard Disk..Booting from 0000:7c00
ISOLINUX 6.03 20171017 EHD...D Copyright (C) 1994-2014 H. Peter Anvin et al
1. Default settings (Runs from RAM)
Loading /pmagic/bzImage... ok
Loading /pmagic/initrd.img...ok
Loading /pmagic/fu.img...ok
Loading /pmagic/m.img...ok
Probing EDD (edd=off to disable)... [interleaved BIOS diagnostics] ok
```

The excerpt omits ANSI menu drawing and interleaved BIOS calls; the raw
bytes are preserved. The last observed RX total is 107,574 bytes by
11:39:53 UTC and does not grow during the remaining capture window.

**Result: USB boot-sector execution, ISOLINUX, kernel/initrd loading and
Linux early setup are confirmed.** The previous received-sector signature
rejection is overcome on this attempt with the same WK firmware. This does
not retroactively prove the exact cause or byte layout of the previous
medium; neither ISO nor stick contents were read back here.

The BIOS diagnostics are not in themselves evidence of a fatal failure:
the pinned SeaBIOS `src/disk.c:701–730` returns a normal error for absent
drive mappings probed at DL=83..8f. `src/system.c:328–355` returns unsupported
for INT15 EC00/E980. `src/kbd.c:146–159,222` advertises AX0305 as unsupported
and logs/returns when called. None of those paths halts or resets the CPU.
The final EDD completion is evidence of setup-stage progress, **not** proof
of kernel decompression, long-mode entry, ACPI evaluation or a live desktop.

No native kernel serial-console command line was established in this run.
Subsequent UART silence cannot distinguish display-only progress from a
hang. A screen-status/photo question was sent to the operator; the display
outcome is pending at capture closure. No second reset, installation, disk
write or speculative register mutation followed. The target is left running.
Windows A5 is not retested and is not claimed resolved.

## Preserved artifacts

All filenames below are relative to `research/msi/captures/`.

```text
b06wk-intenso-retry-boot-20260908-02.raw
  bytes: 107574
  sha256: c8d4d2699f663aa91e7c6007381d8b81b364f7957f1edba00d057c26d6dd73c8
b06wk-intenso-retry-boot-20260908-02.raw.json
  sha256: 668287a4bded7c58ccf8ab65763a8700196a7a7614bc3707cc68574fdb73b507
b06wk-intenso-retry-initial-20260908-02.raw
  sha256: 2c0fcf82014e53eba19d3399b6ce4184c4aa764034ba8b0ef7536154efbfd11a
b06wk-intenso-retry-id-20260908-02.raw
  sha256: 32a9c8ae710f39bcf63a35ba7b3bc744d357ec7cbb04ec14e9f8116ac2930d97
b06wk_select_intenso_20260908.py (unchanged archived helper)
  sha256: ab747a4ef5100dc477afc73b787e2ebbee784c82e61eaa93be46fccc13e75060
```

Local boot-capture SHA256 matches the remote helper's metadata digest.

## Operator display result: Linux driver initialization reached, then stalls

The operator subsequently reports "hier ist es stuck" and supplies
`/path/to/workspace/Downloads/PXL_20260908_134258640.jpg`, SHA256
`5a4e1f529a5958109b059c62ff65414819b7f666f44eb7d196145e3e0ed6bbd6`.
This is the outcome of the same HW-02 attempt, not an additional boot.
The photo contains actual Linux **6.8.0-31-generic** driver output:

```text
[27.862308] usb 3-1: new low-speed USB device number 2 using uhci_hcd
[27.879417] uhci_hcd 0000:00:1d.2: UHCI Host Controller
[27.898345] uhci_hcd 0000:00:1d.2: irq 18, io port 0x0000df40
[27.931922] hub 8-0:1.0: USB hub found
[27.933299] hub 8-0:1.0: 2 ports detected
[27.979897] i8042: PNP: No PS/2 controller found.
[27.998441] mousedev: PS/2 mouse device common for all mice
[28.036957] rtc_cmos 00:00: registered as rtc0
```

This supersedes the earlier uncertainty about kernel entry: **Linux's main
kernel and device-driver initialization are now confirmed**, not merely
ISOLINUX or real-mode setup. No desktop, shell, root mount, successful USB
mass-storage operation under Linux, working IRQ delivery, complete ACPI
validation or full OS boot is proved. The last printed line is not a stack
trace or a reliable identification of the stalled instruction.

Read-only source checks, without resetting or touching the target:

- WK's `b06wj_platform.asl:74–81` describes RTC0 as PNP0B00, I/O70–71,
  IRQ8. MADT overrides IRQ0 and SCI9, not IRQ8. The photo is consistent
  with RTC PnP discovery; it does not prove IRQ8 delivery.
- `acpi_tables.c:160–179` leaves day/month/century zero and retains the
  FADT fixed-RTC-event-not-supported bit. This does not disable the
  legacy CMOS RTC. No direct IRQ8 resource conflict was identified.
- PS/2 absence is consistent with the intentionally cleared FADT i8042
  advertisement and omitted PS/2 namespace under WI/WK; it is not a new
  fatal error established by this photo.
- In [upstream Linux v6.8 RTC class code](https://github.com/torvalds/linux/blob/v6.8/drivers/rtc/class.c#L427),
  the registration message precedes the optional `rtc_hctosys()` call.
  That reads RTC time and calls `do_settimeofday64()` before printing
  the system-clock setting. This makes RTC/timekeeping a specific
  hypothesis, **not a diagnosis**. The exact Ubuntu-derived kernel
  binary/configuration and earlier full kernel log are not available.
- In upstream v6.8 `rtc-cmos.c:1041–1107`, the RTC accessibility check
  and, for a valid IRQ, IRQ request occur before registration. Later
  work includes NVRAM registration and ACPI fixed-event handler setup.
  The generic register-read loop in `rtc-mc146818-lib.c:21–87` has a
  bounded retry count; an indefinite UIP wait must not simply be assumed.
- The pinned SeaBIOS already initializes RTC registers and installs its
  IRQ8 handler (`src/hw/rtc.c:64–71`, `src/clock.c:38–60`); RTC must not be
  described as entirely uninitialized. WK publishes HPET, whose measured
  capability includes legacy replacement. Neither that capability nor
  the firmware's initially masked IOAPIC entries demonstrates correct
  Linux-owned timer/IRQ delivery. RTC/HPET takeover is another hypothesis
  for a later isolated comparison, not a measured failure.

Next proposed test uses the same firmware and medium with explicit Linux
console/initcall tracing. Remove `quiet`/`splash`, preserve the existing
kernel/root/initrd arguments and append:

```text
earlycon=uart8250,io,0x3f8,115200n8 console=tty0 console=ttyS0,115200n8 keep_bootcon ignore_loglevel loglevel=8 initcall_debug
```

These parameters are described in the
[Linux v6.8 command-line documentation](https://www.kernel.org/doc/html/v6.8/admin-guide/kernel-parameters.html).
The console must be captured before boot; BIOS serial output alone does
not enable the kernel console. No ACPI/APIC/HPET disable bundle is proposed
for the baseline trace. A later one-variable bypass should be selected from
the actual failing call path. No new image, reset, kernel-argument change
or invasive test was performed in response to this photograph.
