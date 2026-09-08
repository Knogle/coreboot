> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WJ-HW-01: ACPI platform description and JetFlash handoff

Result at this snapshot: **B06WJ identity, new ACPI construction, SeaBIOS and
JetFlash boot-sector execution confirmed; operator reports that Hiren's is
loading. Windows outcome remains pending.** No new image was built or flashed
during this session, and no additional RAM/QPI qualification was requested.

## Configuration and procedure

```text
test ID: B06WJ-HW-01
build identity: X58PROE-B06WJ-ACPI-PLATFORM-20260907
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived B06WJ delta
intended ROM SHA256: 9ad2108df0a88b66384d6833f3c50bf66f2509f2a739baf0c1b05509b476bd06
flash chip: socketed W25Q128.V..M, 16 MiB; no new programmer readback
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, CPUID 000206c2, stepping 2
microcode revision: live 0000001f
DIMM model: BLS4G3D1609DS1S00, 4 GiB, 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9, physical option ROM
USB medium: JetFlash Transcend 128GB 1100, operator's Hiren's medium
PSU: unchanged fixed configuration; model not restamped
boot type: one-second Shelly mains interruption; COLD_DEFAULT observed
POST trace: serial IRQ READY/a8 path; no independent POST-card capture
serial: operator@serial-gateway.example.invalid /dev/ttyUSB1, 115200 8N1
serial log: captures/b06wj-hw01-20260908T0030-boot-snapshot.raw
result: PARTIAL PASS — new table path and local USB loader; OS outcome pending
recovery required: one user-requested target AC cycle; no flash recovery
notes: ConsolePi was neither power-cycled nor rebooted; target left loading
```

The session date is 8 September in Europe/Berlin, 7 September UTC. The log
filename is a local run label, not a precise wall-clock timestamp.

Initial passive captures found an already running iPXE/netboot.xyz session.
Ctrl-C returned to its menu; a burst of arrow keys reached the FreeDOS
submenu rather than the intended shell. No installer was selected, and the
prompt-gated `show product`/`help` commands were not sent. Before further menu
work the operator requested a reset. No guessed BIOS-drive `sanboot` command
was issued.

The selector was armed before cycling only Shelly switch 0 at
`192.0.2.215`, with `on=false&toggle_after=1`. Preflight reported output on;
the cycle returned `was_on=true`, and follow-up status confirmed output on.
The existing generic Shelly script rejects intervals below five seconds, so
this session used the exact previously authorized one-second RPC directly;
the script was not changed. Electrical G3 was not independently measured.

## New observed results

- Fresh reset banner and SeaBIOS's SMBIOS product both identify B06WJ.
- Snapshot line 90645 records inherited `B06WI-IRQ-ACPI1 READY`, with
  D31IR/D29IR/D28IR/D27IR/D26IR `0232/0237/3201/3216/3250`, IOAPIC ID 1,
  all 24 redirection entries masked and eight legacy PIRQs disabled.
- Line 91068 records the new runtime marker:

  ```text
  [ACPI] B06WJ-PLATFORM1 HPET BASE=fed00000 ID=8086a301 MIN_TICK=0080 CPU_NAMESPACE=CP00/UID0 LPC=PIC/PIT/RTC LOW_IO=1 HW_WRITE=0 SMP=0 SMM=0
  ```

  `ACPI: done.` follows. The 1076-byte DSDT is loaded from CBFS, and HPET is
  the sixth root table. This is firmware-construction evidence, not an OS
  dump or successful Windows evaluation of every AML object.
- SeaBIOS `rel-1.17.0-1-g5497f431` starts; the existing USB keyboard and
  JetFlash initialization runs. No new physical-keyboard test was added.
- At capture +360.442 seconds the selector sends ESC. At +360.660 seconds it
  sends the actual matched JetFlash menu key `2`. The menu names the SATA
  SSD as entry 1 and iPXE as entry 3.
- Lines 91525 onward map the USB drive `f21e0` to BIOS disk 0, then SATA to
  disk 1. Line 91552 reaches `Booting from 0000:7c00`.
- The operator subsequently reports that the medium is loading. The two
  last `INT13 AH=15, DL=00/01` messages match the predecessor's absent-floppy
  probes; they do not themselves report a failed JetFlash read.

There were no manual register experiments, guard writes or additional
resets after selection. The unchanged firmware-internal clear/readback path
still runs before ramstage. RAM/QPI remain the operator-accepted working
baseline, not newly qualified by this run. CSI/MINIT remain explicitly
vendor-assisted.

## Immutable snapshot and capture limitation

The [boot snapshot](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) is
2,622,413 bytes, SHA256
`eca57b17e623856bd72c96c4518af21815870f6b3d420047b9c799b00db621f9`.
It was copied while the passive 600-second remote capture was still running;
it is not represented as a completed 600-second observation. Later output
must be archived separately without overwriting this snapshot.

The first 2,526,979 bytes contain repeated old-looking ROMMON lines and
noise, arriving faster than a 115200-baud physical wire could produce.
That prefix is excluded from live hardware conclusions. The first complete
fresh B06WJ banner begins at byte offset 2,526,979 / line 90228; the following
95,434-byte reset-to-loader sequence contains coherent phase transitions and
the expected timed clears. The original prefix is retained, not silently
removed. Its transport/buffering cause was not investigated in this task.

The [executed selector](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) has SHA256
`7625648cbb9a13fd9b79318c9873b86d2d0956c8d060548c42402e5699f1bc54`.
Unlike the preserved WI copy, this version saves the selected digit before
clearing the match buffer. Selection timings above were emitted by the live
helper; final helper metadata is still pending at this snapshot.

## Remaining outcome

Await operator screen feedback. Neither an A5 fix nor another A5 failure is
claimed for this run yet. Leave the loader undisturbed; do not repeat already
accepted platform tests. If Windows fails, append the new screen outcome and
any available bugcheck parameters to this record.

## Capture completion, appended after the snapshot

The helper subsequently finished normally at +600.033 seconds. The
[complete raw capture](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) and
[final metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) are now archived.
The raw file is byte-identical to the earlier snapshot, with the same size
and SHA256; no additional UART bytes or firmware reset appeared before the
deadline. The metadata confirms ESC at +360.442 and JetFlash key 2 at
+360.660 seconds. Thus approximately 239 seconds after selection are covered,
but serial silence does not distinguish a graphical loader, Windows progress
or a hang. Operator screen outcome is still pending. The capture released
the UART normally; the board was left undisturbed.

## Operator outcome, appended 8 September

The operator subsequently reports "same error" (translated from German) and requests extensive
reference-machine/vendor analysis. This closes the pending outcome above:
Windows again fails with the previously identified top-level
`ACPI_BIOS_ERROR (0xA5)`. No new screenshot, crash dump or four bugcheck
arguments accompanies this confirmation. The identical top-level error does
not identify an identical subtype. B06WJ's additions are therefore insufficient
for this Windows boot, while its recorded firmware/JetFlash milestones remain
valid. The original raw capture and pending-outcome historical text are
unchanged. The subsequent RAM-only diagnostic session is a separate test.
