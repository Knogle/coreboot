> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WK-HW-03: Intenso Linux COM1/initcall diagnostic boot

## Hypothesis and controlled scope

HW-02 reached Linux 6.8.0-31-generic UHCI/PnP/RTC initialization but the
operator reported a stall with the last visible line `registered as rtc0`.
The user authorizes a new logged boot, automatic Intenso selection and
kernel-console/initcall diagnostics. This test changes only the bootloader's
in-memory Linux command line; it does not rebuild/flash firmware or alter
the boot medium. ACPI/APIC/HPET and the accepted RAM/QPI baseline stay enabled
with their existing policy.

Expected discriminator: a kernel-owned COM1 log containing the effective
command line and initcall progress, ideally a failed/stalled call or panic.
Loss of serial output alone is not proof of a halt. Failure/recovery path:
preserve the raw capture, stop only the owned UART helper, and use the
established target-only one-second AC cycle when another attempt is needed;
socketed known-good WJ/vendor firmware remains available. Never cycle
ConsolePi or select JetFlash/SATA as a fallback.

```text
test ID: B06WK-HW-03
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta
intended ROM hash: bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no flash readback
flash chip: socketed W25Q128.V..M, 16 MiB; inherited inventory
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645; CPUID206c2, fixed configuration
CPU stepping: 2
microcode revision: associated prior boot1f; current value to be captured
DIMM model: BLS4G3D1609DS1S00, 4 GiB 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9 with own option ROM
PSU: unchanged; model not restamped
boot type: one target-only Shelly one-second timed AC cycle; electrical G3 not independently verified
POST trace: fresh WK/c3 and automatic initialization captured, later phases pending
serial: operator@serial-gateway.example.invalid /dev/ttyUSB1, 115200 8N1
serial log: captures/b06wk-intenso-linux-diag-20260908-03.raw; archive on completion
result at start checkpoint: automatic firmware start in progress
recovery required: authorized target reset from previously reported Linux stall
notes: Intenso Ultra Line8.01, JetFlash and Crucial remain present; only Intenso is selected
```

## Capture and reset start

No competing serial reader was present in the ConsolePi process preflight.
The helper was armed at **13:56:14.712832 UTC**, with a 1200-second bound,
exclusive UART/project lock, immutable raw log and explicit TX metadata.
It observes the fresh WK identity, sends ESC only at the SeaBIOS invitation,
matches the Intenso menu digit, and sends TAB only at the observed ISOLINUX
default-entry editor invitation. It does **not** automatically execute an
edited kernel command. Further reviewed TX goes through its private FIFO;
no second process opens the UART.

The selector was tested against the previous complete hardware trace in
101-byte chunks: exactly ESC, `2`, TAB, in order, then no further automatic
TX on replay. An initial chunk-boundary bug was corrected before deployment.
The deployed helper is `captures/b06wk_intenso_linux_diag_20260908.py`, SHA256
`d8c47720d4dc8a3d409328925b08a09fadffbdd104a1f421ba9331df518c8aae`.

One request to the known target Shelly at `192.0.2.215`:

```text
Switch.Set?id=0&on=false&toggle_after=1
response: {"was_on":true}
preflight: output=true, 80.1 W
follow-up: output=true, 73.7 W
```

The local return timer was used; there was no separate on request. The
repository's older generic Shelly helper enforces a minimum five-second
interval, so the already-established direct one-second RPC was retained
without broadening that script's policy. Fresh WK output follows with
`ENTRY=COLD_DEFAULT`, `CMOS0E=2c` and zero I801 signature. No manual guard
clear was required. Early reset bytes are retained as a distinct raw prefix.

## Planned editor procedure

Preserve the actual observed kernel and initrd/root/live arguments, remove
`quiet`/`splash` if present, append COM1/earlycon/initcall diagnostics, and
verify the entire edited line before sending Enter. No `acpi=off`,
`noapic`, `hpet=disable` or memory/topology workaround is part of this run.

[Syslinux 6.03 editor source](https://kernel.googlesource.com/pub/scm/boot/syslinux/syslinux/+/refs/tags/syslinux-6.03/com32/menu/menumain.c#445)
confirms that Ctrl-A moves to the start and redraws, Ctrl-U clears the whole
line, and Enter immediately executes the edited command. ANSI cursor
redraws must not be concatenated into a duplicated command line. Actual
editor observations, TX bytes and final kernel evidence follow below.

## HW-03 outcome: blocked before payload, no Linux diagnostic attempt

The automatic boot reached ramstage but stopped in USB-AUTO before GPIO57
release, ACPI publication or SeaBIOS. No serial TX was issued by the selector.

```text
[USB-AUTO] UHCI4 USBCMD=0000 USBSTS=0020 USBINTR=0000 PORTS=0c88/0c80
[USB-AUTO] UHCI5/6/1/2/3 USBCMD=0018 USBSTS=0024 USBINTR=0000
[USB-AUTO] PRECISE-B06WD-BASELINE INVALID persistent=0 require_oca_active=1
[USB-AUTO] B06WK-AUTO-GPIO57-USB1 FAIL reason=exact-late-preflight rollback_requested=0 rollback_ok=1; payload blocked; cold recovery required
```

The grouped five-controller line summarizes five individual raw records.
`b06wg_usb_auto.c:406–407` requires `0000/0020/0000` on every UHCI; those five
command/status tuples fail the exact comparison. The corresponding UHCI
header defines command0018 as global suspend plus force global resume,
status0004 as resume detect. HW-02 had the required tuple on all six.
The changed PORTSC CSC/PEC bits are telemetry-only and are not this rejection.
GPIO57 input/low and OCA active on every port are the expected pre-release
state. `require_oca_active=1` is the requested mode, not proof of a failed
electrical check. `rollback_requested=0` means no USB-AUTO GPIO write and
no attempted rollback, not a tested rollback success.

Residual OS/controller state or a wake transition is plausible; whether
the one-second mains interruption removed all relevant power is not known.
The capture was stopped through its private control FIFO at
**14:02:47.399824 UTC**, normal exit, 60,544 bytes, SHA256
`f75a4e1f8746487155ef8d3f08aa3fbfb8e2acad3af604cc79408ba333da2adf`.
Metadata SHA256:
`96127f2d248b737879787101a793f3aba7da45a7bd43fdf488fdd930a8a096ae`.
Both raw and JSON are archived under `research/msi/captures/`; no prior log
was overwritten. No Linux arguments were entered and no guard was bypassed.

## HW-04 controlled recovery retry

Same build/hash, flash chip, board/CPU/DIMM/GPU/PSU and USB configuration as
HW-03; no firmware or media change. A new exclusive capture was armed at
**14:03:42.848742 UTC**:

```text
test ID: B06WK-HW-04
serial log: captures/b06wk-intenso-linux-diag-20260908-04.raw
boot type: second target-only one-second timed AC cycle
POST trace at start: fresh WK/c3 and ENTRY=COLD_DEFAULT
result at start checkpoint: recovery boot in progress
recovery required: USB preflight rejected retained/unexpected UHCI state on HW-03
notes: same hypothesis and recovery procedure; no speculative controller or GPIO writes
```

Before the second cycle, Shelly output was on at71.5W. The same single
`on=false&toggle_after=1` request returned `{"was_on":true}`. This is a
controlled recovery retry, not evidence that the USB mismatch is fixed.

The independent native-source audit found no pre-gate UHCI operational
writer explaining0018/0024: the WK endpoints lack an HC `.init` callback,
the narrow EHCI helper writes only EHCI PCI configuration, and the two
UHCI admission/auto collectors read operational registers. `NO_SMM=y` and
masked SMI sources rule out assuming an ordinary native S3/SMM USB resume
path. The local vendor-assisted early code means this is not an exhaustive
exclusion of all writers. SeaBIOS would normally reset UHCI after payload
entry, which the exact preflight currently prevents. A future bounded HC
quiesce/reset stage could normalize this state; no such change is made in
this diagnostic-only run.

## HW-04 outcome and HW-05 two-second recovery comparison

HW-04 repeats the identical five-UHCI command/status rejection before any
USB-AUTO GPIO change or payload. Capture stopped normally at
**14:10:08.375551 UTC**, with no UART TX; 60,544 bytes. Raw SHA256
`6f034fa691ad834ceddc6e3728cb7a1bfe631aba37b0a3f3b293a2710ed60f51`;
metadata SHA256
`477c3e97de422a2f62d1f3303789d369844981802f61be0fcb44083689ef19c0`.
The second identical one-second reset did not normalize these registers.
No Linux command line or kernel diagnostic was reached on HW-04 either.

For HW-05, the already user-authorized maximum of two seconds is used once
instead of repeating the same one-second condition. Firmware, device
population and proposed Linux diagnostics remain unchanged.

```text
test ID: B06WK-HW-05
hardware/build/hash: same fixed configuration and intended ROM as HW-03/HW-04
serial log: captures/b06wk-intenso-linux-diag-20260908-05.raw
capture armed: 14:10:09.994856 UTC
boot type: target-only AC request on=false&toggle_after=2
Shelly preflight: output=true, 71.3 W
Shelly response: {"was_on":true}
Shelly follow-up: output=true, 72.4 W
POST trace at start: fresh WK/c3, ENTRY=COLD_DEFAULT, subsequent auto-init
result at checkpoint: recovery comparison in progress; no manual power button needed
recovery required: repeated unexpected UHCI suspend/resume tuple in two shorter resets
notes: electrical power-rail discharge not measured; do not claim G3 solely from timer duration
```

An offline-only renderer was also added for safe future Syslinux editor
readback (`scripts/render_x58_serial_screen.py`). It has no UART or network
access; its tests cover ANSI cursor redraws, line wrapping and refusal to
extract incomplete commands. This does not affect the running capture or
firmware, and is not evidence that the editor has actually been reached.

## Final outcome: short-reset recovery exhausted, Linux trace not reached

HW-05 reproduces the same five-controller0018/0024 mismatch after the
two-second interruption. Capture ended normally at **14:16:38.056573 UTC**,
60,542 bytes. Raw SHA256
`c618b5f3040f3beb4f740a7662a8562a6208a9a6e3ac5053b7dbb50121a74efd`;
metadata SHA256
`76011e028d454a8ab0f13a5af8a266b9d3b5e65fa7605752b7855ded54279f71`.

All three local raw sizes/hashes match their remote metadata. Each contains
the exact-late-preflight failure and **zero serial TX events**. Metadata
`stage=seabios` means the selector is *waiting for* the SeaBIOS invitation;
it does not mean that SeaBIOS was reached. No ESC, Intenso choice, TAB,
kernel argument or Enter was transmitted in these three attempts.

The controlled comparison is therefore:

| Run | Timed mains interruption | Late UHCI state | Result |
|---|---|---|---|
| HW-03 | 1 second | five0018/0024, one0000/0020 | pre-payload USB guard stop |
| HW-04 | 1 second | same | pre-payload USB guard stop |
| HW-05 | 2 seconds | same | pre-payload USB guard stop |

The task's Linux COM1/initcall experiment remains **unperformed**. These
failures neither reproduce nor diagnose the earlier Linux RTC stall or
Windows A5. They expose a separate reproducible reset/preflight limitation.
No new firmware was built, no equality gate bypassed, no controller-reset
code installed, and no disk/flash was written. The owned capture helpers
have exited and released the UART; the target remains powered in its
firmware guard halt. No fourth automatic reset was issued.

The operator was asked about a complete, physically verified power-off
until board standby indicators extinguish, followed by a coordinated
power-on with a fresh capture. That experiment has not happened yet and is
not guaranteed to fix the mismatch. If the tuple persists after genuine
power removal, a separately authorized bounded UHCI normalization change
is preferable to silently weakening the gate. A cold-start return path
may require the physical power button, hence operator coordination.

Host-only preparation passed six serial chunk-size replay cases (1,17,64,
101,512,8192 bytes), ambiguous-target rejection and no-repeat-auto-TX
checks. The offline screen renderer passed14 tests. None of those tests
constitutes hardware validation of the never-reached editor path.

## HW-06 operator cold start, capture armed before power-on

The user requests that logging start immediately and says they will perform
a cold start about one minute later, then asks for the planned Intenso/Linux
diagnostic routine. No agent-controlled Shelly request or reset is made.

```text
test ID: B06WK-HW-06
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta
intended ROM hash: bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash chip: socketed W25Q128.V..M, 16 MiB; inherited inventory
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, fixed configuration
CPU stepping: 2, inherited inventory pending current probe
microcode revision: prior1f; current value to be captured
DIMM model: BLS4G3D1609DS1S00, 4 GiB2Rx8; fixed configuration
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: operator-initiated cold start; off-duration/rail discharge not independently measured
POST trace at checkpoint: fresh WK/c3, COLD_DEFAULT, CMOS0E2c, I801 zero, CSI phase1
serial: operator@serial-gateway.example.invalid /dev/ttyUSB1, 1152008N1
serial log: captures/b06wk-intenso-linux-diag-20260908-06.raw; archive after closure
result at checkpoint: automatic startup in progress
recovery required: manual cold start after three short-cycle USB-guard failures
notes: no agent reset, extra RAM/QPI test, firmware/flash/storage modification or gate bypass
```

The unchanged diagnostic helper was armed at **15:47:11.822799 UTC** with
an 1800-second capture bound and exclusive UART. No bytes had arrived by
the 60-second heartbeat; fresh reset bytes/WK identity follow thereafter.
The user was told that capture was ready before starting. The same
fresh-marker/ESC/Intenso/TAB state machine is used, with no automatic Enter.
Actual USB admission, editor readback and Linux result are appended below.

## HW-06 result: USB gate recovered; diagnostic edit missed the loader boundary

The operator cold start reaches the expected six-controller UHCI baseline:
all six show `USBCMD=0000 USBSTS=0020 USBINTR=0000`, unlike HW03–05.
The existing GPIO57 release sequence executes, all current OCA bits clear,
and `B06WK-AUTO-GPIO57-USB1 PAYLOAD ADMIT` follows. The repaired ACPI marker
(`FADT_FLAGS=00000065`, `CTBL_CONSUMER_FLAG=01`) is present. SeaBIOS enumerates
Intenso, JetFlash, the USB keyboard and the Crucial SATA SSD. This is one
successful operator-cold-start recovery observation, not proof of the exact
electrical discharge condition or of general reset reliability. The raw
prefix contains three `COLD_DEFAULT` entries, including interrupted early
startup output, before the final completed initialization. No agent reset
was issued; do not count those prefixes as independently qualified cold boots.

Automatic actions were actually observed and logged:

| UTC | UART action and verified result |
|---|---|
| 15:54:19.470901 | ESC after the actual SeaBIOS invitation |
| 15:54:19.584120 | `2`, matched to `USB MSC Drive Intenso Ultra Line 8.01` |
| 15:54:20.021217 | TAB after the ISOLINUX default-entry invitation; editor appears |
| 15:54:37.741825 | Ctrl-A; complete original command redrawn |

The exact original line is:

```text
.linux /pmagic/bzImage max_loop=256 edd=on vga=normal initrd=/pmagic/initrd.img,/pmagic/fu.img,/pmagic/m.img
```

It has no existing `quiet`, `splash`, `console`, `earlycon` or `loglevel`
parameter. The menu-generated `.linux` type prefix is part of the command,
not a typo. The final Ctrl-A redraw occupies raw bytes102941..103073,
followed by the explicit cursor-to-start sequence at103074. Joining its
80-column CRLF wrap and removing only its known ANSI decorations gives an
exact match to the line above. The original snapshot is103082 bytes, SHA256
`9374c7fc2ae65f1f0ee2d9b66575ef197925038ca950c5b4768f945672e9b91a`.

**The intended diagnostic command did not reach a verified editor buffer.**
At raw offset103089, the loader starts `/pmagic/bzImage`, then reports all
three initrds `ok`, completes EDD setup and emits terminal reset/clear.
By the15:55:12 heartbeat, RX has reached its final109283 bytes. There was
no agent Enter before that transition. The original UART helper incorrectly
retained its `editor` state after loading began. The next agent operation
therefore sent Ctrl-U,30 text fragments and Ctrl-A at15:55:53–15:56:05,
all at the already-final raw offset109283. These32 stale TX events are an
automation error, **not evidence of edited boot parameters**. No echo or
new readback followed, and no Enter was sent. The separately checked edited
snapshot still contains only the original command and fails the planned-line
comparison. Its filename says `editor-edited-snapshot` because that was the
intended checkpoint, not because an edit succeeded. No response proves
whether those late bytes were consumed; do not claim that nothing received them.

Syslinux6.03 has a concrete possible explanation for leaving the editor:
TAB clears the local key timeout, but not global `TOTALTIMEOUT`. The editor's
`mygetkey(0)` still checks that global deadline; expiry uses `longjmp()` to
boot ONTIMEOUT/the original default command without Enter. Ctrl-A itself
only changes the cursor. The actual medium configuration and any physical
keyboard input are unknown, so global timeout remains a **hypothesis**, not
a measured cause. See the exact upstream
[menu source](https://kernel.googlesource.com/pub/scm/boot/syslinux/syslinux/+/refs/tags/syslinux-6.03/com32/menu/menumain.c#179)
and [.linux prefix generation](https://kernel.googlesource.com/pub/scm/boot/syslinux/syslinux/+/refs/tags/syslinux-6.03/com32/menu/readconfig.c#395).

Capture closed normally at **15:58:18.060440 UTC**,109283 bytes, after
additional passive observation with no new RX. Local raw and metadata size/
digest agree:

- raw SHA256: `9f77fc34aa951c6488b233e0c840ede57de516516fdc88e2ee26f19a883c484e`
- metadata SHA256: `da5c405d06f72c827f5227f2658b32686a83e0eda1c524cb1725d30c5fd4f272`
- total TX events36: four valid pre-loader controls,32 stale post-loader controls;
  **zero Enter events**.

The private capture FIFO was removed by normal helper cleanup and the UART
released; historical raw files, metadata and snapshots remain preserved.
The target was not reset or flashed. No disk operation was explicitly issued
by the agent; this is not a claim about all effects of the booted live system.
There is no native-kernel serial output, new display result, completed OS
session, or diagnosed RTC/Windows-A5 cause in this capture. The diagnostic
plan remains pending. Before another coordinated boot, the new helper must
reject controls after loader evidence and require fresh editor readback.

### Subsequent operator display report

The operator subsequently reports a few messages summarized as
`interrupt too long`. Exact wording, handler, timestamps and durations have
not yet been supplied; a photograph with surrounding lines was requested.
Do not turn that paraphrase into a fabricated `hrtimer`, `perf`, RCU or IRQ
stack trace. The stored serial capture cannot contain the native Linux
messages because the diagnostic command was not applied. This report does
not establish the same RTC stall, a completed OS session or an ACPI cause.

### HW06 photo: timer warnings and vendor counterexample

Photo `/path/to/workspace/Downloads/PXL_20260908_160044743.jpg`, SHA256
`d7635c25bd9863b50fcc8947663af0b103e647b130a1ad91cf2aa987e954d21d`,
subsequently supplies actual Linux output. Key transcription:

```text
222.309234 pci_bus 0000:ff: busn_res: can not insert [bus ff] under domain [bus 00-ff] (conflicts with (null) [bus 00-ff])
222.541272 hpet0: at MMIO 0xfed00000, IRQs 2, 8, 0, 0
222.552982 hpet0: 4 comparators, 64-bit 14.318180 MHz counter
222.609935 clocksource: Switched to clocksource tsc-early
249.453911 perf: interrupt took too long (85221 > 85035), lowering kernel.perf_event_max_sample_rate to 2000
250.260259 hrtimer: interrupt took 1644846 ns
307.722506 perf: interrupt took too long (106826 > 106526), lowering kernel.perf_event_max_sample_rate to 1000
```

These are photo transcriptions, not captured serial lines. The hrtimer
interval is1.644846ms. Perf reduces sampling adaptively; neither this nor
the timer warning alone diagnoses ACPI/GPIO/IRQ routing or TSC calibration.
Primary upstream6.8 references (Ubuntu's exact patchset not reconstructed):
[perf](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/v6.8/kernel/events/core.c#497),
[hrtimer](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/v6.8/kernel/time/hrtimer.c#1827).

The **working vendor reference has the identical bus-ff conflict twice**,
and identical HPET IRQ/counter values. Local artifact
`blobs-local/msi-x58-pro-e/x58-acpi-nvs-reference-20260908-01/dmesg.txt`,
verified SHA256 `9254a38907087f6a39e2fce5961646ef726039ca5275876b273a5c5d5b0947df`,
contains these at lines424/450 and458–460. It identifies the same
Ubuntu6.8.0-31-generic kernel and vendor BIOS V8.14B8. Thus the bus warning
is **not target-specific** and does not justify an immediate rootbus-range
fix. Linux's peerbus scan creates a separate ff root whose bus resource
overlaps the previously described00–ff range; the scan continues. `(null)`
is the resource name, not a null-pointer crash.

The vendor reference switches to tsc-early at0.497280s, versus222.609935s
in the target photo. This is a discrepancy in reported boot progression,
not a448x CPU benchmark: elapsed time calibration, RAM population
(reference12GiB vs target4GiB), SMP and console policy are not controlled.
Early TSC calibration and paired initcall-entry/return records are needed.

### Focused cache-policy audit, no implementation change

The active WK postcar path caches0..2GiB WB and2..3GiB WB, overlays
A0000..BFFFF UC and marks the top256KiB ROM WP. The remapped1GiB at4..5GiB
explicitly remains UC due to the32-bit postcar helper API
(`fail_closed_memmap.c:37–54`). The successful ramstage handoff marker
follows an exact gate checking this layout and defaultUC (DEF_TYPE800).
Thus **not all RAM is UC**, but the upper usable RAM is not made WB here.
The ramstage CPU-cluster routine only sets LAPIC; the release ELF lacks
normal `x86_setup_mtrrs`. SeaBIOS MTRR setup is QEMU-only, not this path.

This is a concrete firmware cache-policy gap and potential source of slow
high-memory access. Linux may trim or adjust MTRRs; no live post-kernel MSR
capture yet establishes the effective outcome. No causal claim for the
timer/RTC problem and no firmware change are made from this audit.

The subsequent pinned upstream6.8 check narrows the kernel caveat:
`mtrr_trim_uncached_memory()` accepts only valid WB/UC variable entries;
the valid WP-ROM entry in the inherited WK layout causes an early return.
The cleanup eligibility helper has the same type restriction. Consequently
automatic trimming is not an expected cure for this unchanged layout,
even though the actual running kernel configuration/state is not captured.
See [trim gate](https://github.com/torvalds/linux/blob/v6.8/arch/x86/kernel/cpu/mtrr/cleanup.c#L928)
and [cleanup gate](https://github.com/torvalds/linux/blob/v6.8/arch/x86/kernel/cpu/mtrr/cleanup.c#L562).
This is source-supported expected behavior, not a fresh target MSR read.

## HW07: requested reset to ESC menu; user owns subsequent inputs

The user requests another reset into the SeaBIOS ESC menu and will select
Intenso and enter serial parameters personally. The replacement capture
helper's `--stop-at-seabios-menu` mode sends only the invited ESC, then
remains passive and rejects all FIFO hex commands. Capture continues into
Linux. Its SHA256, verified locally/remotely, is
`85e32547dec2a9b2f74ecacdbbd12ed67371c094fec991d862bf4147ac2db01a`.
Twenty host tests pass, including rejection of all32 late HW06 controls
and sole-ESC replay. Historical helpers/logs remain unchanged.

```text
test ID: B06WK-HW-07
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta; unchanged
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash/board: socketed16MiB W25Q128.V..M, MSI MS-7522; PCB label not restamped
CPU/stepping/microcode: E5645,CPUID206c2,step2,ucode1f observed
DIMM/slot: BLS4G3D1609DS1S00,4GiB2Rx8,sole SPD54; physical label not restamped
GPU/PSU: HD5450,unchanged PSU; model not restamped
boot type: one user-authorized target-only Shelly off/on with2s local return timer; no independent G3 proof
capture armed:16:07:47.377772 UTC,exclusive UART1152008N1,1800-second bound
Shelly preflight:id0,output=true,82.0W
Shelly response:{"was_on":true},requested near16:08:02 UTC
Shelly follow-up:output=true,72.6W
POST trace:fresh WK/c3 at16:08:07,normal CSI/MINIT sequence,postmem clear in progress
serial log:captures/b06wk-linux-user-serial-20260908-07.raw; archive after closure
result:in progress; ESC menu not yet reached
recovery required:user requested reset from Linux
notes:no ConsolePi power cycle,agent device selection,extra RAM/QPI tests,firmware or disk edits
```

User-supplied diagnostic suffix (kernel/full initrd retained; quiet/splash
removed only if present):

```text
earlycon=uart8250,io,0x3f8,115200n8 console=tty0 console=ttyS0,115200n8 keep_bootcon ignore_loglevel loglevel=8 initcall_debug
```

No mem/ACPI/APIC/HPET workaround is requested for this baseline; actual
executed parameters remain to be captured.

HW07 checkpoint: normal postmem completes and the USB/payload path reaches
SeaBIOS. At **16:13:51.984531 UTC**, the helper sends its sole ESC after the
actual invitation; `Select boot device:` is received at16:13:52.083087 and
the helper becomes passive. The menu lists Intenso as2, JetFlash as3.
Subsequent non-agent inputs select Intenso and open its TAB editor; no digit,
TAB or command text is transmitted by this menu-only helper. At16:14:09,
the editor shows the unchanged original `.linux /pmagic/bzImage ...` line.
Capture remains active, with its original deadline near16:37:47 UTC; final
raw/metadata archival and actual Linux cmdline evidence are still pending.
The local `b06wk-linux-user-serial-hw07-editor-checkpoint.raw` is only a
prefix snapshot, not a closed capture. No second reset was issued.

### HW07 native Linux serial and userspace entry confirmed

The user reports `boot aktiv`. The active capture now contains actual
Linux6.8.0-31-generic messages, not just SeaBIOS redirection. Both the early
`Command line:` and later `Kernel command line:` exactly confirm:

```text
BOOT_IMAGE=/pmagic/bzImage max_loop=256 edd=on vga=normal initrd=/pmagic/initrd.img,/pmagic/fu.img,/pmagic/m.img earlycon=uart8250,io,0x3f8,115200n8 console=tty0 console=ttyS0,115200n8 keep_bootcon ignore_loglevel loglevel=8 initcall_debug
```

The kernel enables uart8250 at I/O3f8/115200n8 and then ttyS0. Repeated
identical lines after2.809800s are the retained bootconsole plus regular
serial console, not duplicate boot events. Their extra115200-baud output
is an intentional diagnostic perturbation; timing is not directly comparable
to quiet VGA-only boots. No new UART TX or reset is issued after ESC.

| Kernel timestamp | Captured progress |
|---|---|
| 0.000000 | PIT-based TSC calibration2527.192MHz |
| 9.142184 | `acpi_init` returned0; ACPI interpreter and IOAPIC enabled |
| 10.619933 | switched to tsc-early |
| 18.465987 /18.565028 | refined TSC2526.999MHz; switched to tsc |
| 57.274089 | rtc0 registered |
| 57.284259 | RTC read/set system clock to2024-04-01T07:43:06UTC |
| 57.381136 | `cmos_init` returned0 after192253us |
| 83.725316 | initrd memory freed after unpacking |
| 85.246897 | `inet6_init` returned0 after25.731677s |
| 107.050646 | `Run /init as init process` |
| 116.118590 /118.239331 | loop0/loop1 configured |
| 141.574498 | native AHCI/libata/SCSI attaches Crucial CT1000MX500SSD1 as sda |
| 187.917800 | userspace eudev starts |

The former apparent RTC stop is therefore passed, and `/init`, module
loading and userspace udev activity are now proven. The observed RTC date
is stale, not the actual capture date. No date-setting action was performed.
The completed IPv6 call was slow, not permanently stuck. The broad kernel
contains many unsupported-platform initcalls returning-19; these are not
automatically chipset failures. No Linux panic, ACPI BIOS exception, or
`nobody cared`/disabled-IRQ report was found in the first three snapshots.
A completed Parted Magic desktop/root session is still pending.

High memory is not trimmed away: NODE_DATA is allocated at13ffd3000..13fffdfff,
the Normal zone covers100000000..13fffffff, and Linux reports
3832936K/4192288K available (about3.66GiB/4GiB). The four-entry MTRR map is
consistent with the firmware layout but omits types/bases/masks. High-memory
use is proven; UC remains a strongly supported firmware-path inference,
not a fresh post-kernel MSR dump. Early and refined TSC calibration differ
by only76.4ppm, arguing against a gross calibration failure in this run;
the approximately5% difference to the vendor reference frequency remains
unexplained and is not an intentional new clock setting.

At85.658163s, `mce: [Hardware Error]: Machine check events logged` appears.
Its two identical copies are console duplication, not two established MCEs.
The working vendor reference has the same generic notification at1.062576
and1.062642s (reference lines733–734), but this does **not** prove identical
or harmless errors. No bank/STATUS/ADDR/MISC payload is yet available to
classify severity or assign a RAM/QPI cause. Preserve this issue for follow-up.

Immutable prefix snapshots (not final captures):

- `b06wk-linux-user-serial-hw07-kernel-checkpoint01.raw`, SHA256
  `a927a5ed265c873dda25e92c70761e8c4c3f565c34358f436daad5191cfddd0d`.
- `b06wk-linux-user-serial-hw07-kernel-checkpoint02.raw`, SHA256
  `3ce668bad438d4fc7a4cfb21f6e0338fa57ef92a209f6fbbb68afe06fcdaa2e4`.
- `b06wk-linux-user-serial-hw07-kernel-checkpoint03.raw`, through userspace
  udev startup; final capture continues remotely under exclusive UART ownership.

At the later16:20:37UTC readout, additional module probes and IOAT DMA
initialization are progressing. A clocksource watchdog reports a long
readout interval, but its reported reference/candidate durations agree
(1243833177 vs1243832892ns), not a captured clocksource-skew failure.
The operator was asked for the current display state. No kernel parameter,
ACPI/IRQ/cache setting or storage operation is being injected by the agent.

### HW07 USB/storage/network and Radeon driver outcome

Passive output through16:27UTC proves additional forward progress:

- USB HID keyboard drivers bind; Realtek ALC888 audio codec initializes.
- Intenso attaches as `sdb` with partitions1/2 at390.96s; JetFlash as `sdc`
  with partitions1/2 at392.51s. These names describe this boot only.
- The native `r8169` driver registers RTL8168c/8111c `eth0`, MAC
  `02:00:00:00:00:01`, at405.12s. No acquired IP/working network session
  is yet captured.
- ISO9660 Joliet/RRIP parsing appears at408.57s.
- Radeon deactivates the VGA console at473.291023s, switches to a dummy
  console and recognizes CEDAR1002:68f9, ATOM BIOS C09302 and512MiB VRAM.
- At479.860093s the direct request for `radeon/CEDAR_pfp.bin` fails with
  error-2, followed by failed firmware load, fatal GPU init and probe-2.
  This is an OS-driver firmware request, distinct from the recognized
  onboard ATOM VBIOS. It is not evidence that coreboot lost PCIe access.
- Crypto and other module initcalls continue through613.836352s after the
  GPU probe failure. That failure therefore did not immediately stop the
  entire kernel. A working DRM framebuffer/desktop is not established.

The firmware file may be missing from the initramfs or otherwise unavailable
in the loader's search path at probe time; the on-stick filesystem has not
been inspected. No image change or `nomodeset` retry was performed. The
single native-serial run remains active and passive; no extra TX/reset.

Pinned Linux6.8 source corroborates the GPU interpretation:
[CEDAR selection and firmware request](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/v6.8/drivers/gpu/drm/radeon/r600.c#2553)
and [ATOMBIOS-before-microcode initialization](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/v6.8/drivers/gpu/drm/radeon/evergreen.c#5180).
The [KMS error path](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/v6.8/drivers/gpu/drm/radeon/radeon_kms.c#150)
returns before complete modesetting/fbdev setup. A blank display is therefore
plausible after the logged VGA deactivation without a whole-system hang;
the operator's actual display outcome is still needed. Further CEDAR runtime
files (including ME/RLC) would also be needed; adding only a replacement
onboard VBIOS to coreboot would not repair this OS file-loading error.

Additional immutable snapshots:

- checkpoint03 SHA256:
  `2ec956f14d8af298e3755dbed9d40914139404361dd63603badd667e01a86391`.
- checkpoint04 SHA256:
  `1ab196357c0f7e2f18bb2e40a8463a4bdd2fd1a16823d488c2a42347e559bf4a`,
  622372 bytes, through613.836352s. The regular capture heartbeat continues
  afterward but is not a target liveness signal. No further target bytes
  have arrived as of16:30UTC; that does not establish a kernel failure.

### HW07 capture closure and final observed progress

After about six minutes without new UART bytes, the agent closes only the
capture to archive the observation; the board is left powered and unchanged.
The final drain contains new target output: `kvm_x86_init` is called at
964.375516s and returns0 at964.387633s. Thus the apparent long pause did not
establish a terminal hang; Linux continued module initialization more than
16minutes after kernel entry. A live desktop/login is still unconfirmed.
No final-root switch, working OS network connection, panic or machine-check
record details are established by this capture.

Capture armed16:07:47.377772UTC and closed normally
**16:32:31.184562UTC**, elapsed1483.809s. Metadata has `error:null`,
`stop_at_seabios_menu:true`, final stage`booting` and exactly **one UART TX**:
the invited ESC at16:13:51.984531UTC. The stop control is handled locally by
the capture helper, not transmitted to the target. UART exclusivity is
released and the helper's temporary FIFO is removed by normal cleanup.

Final immutable artifacts, copied locally and verified against the remote
metadata's byte count and raw SHA256:

- [Full raw serial log](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
  622646bytes, SHA256
  `b08f7670ba9f8cc6e0b586cd8c22bdd59dd734518ba1ac43c5137efe214e81a4`.
- [Capture metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
  SHA256`40c31209585dc4ec5aacce151f15e0dcf6ed46735f7c17d9f6b096a20945b5f9`.

Current outcome: native Linux serial and initramfs userspace proven; RTC
boundary passed; extremely slow but continuing module initialization; Radeon
runtime firmware unavailable; no completed OS session or Windows A5 fix
claimed. Pending operator display feedback and later focused runtime-cache,
machine-check and live-image firmware inspection, not another blind reset.
