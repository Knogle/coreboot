> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WI-HW-01: IRQ/ACPI transaction and JetFlash handoff

Result: **hardware routing/table-construction and local USB boot handoff
passed; the operator subsequently supplied a Windows screen showing
`ACPI_BIOS_ERROR (0xA5)` again**. The four bugcheck parameters are absent, so
the exact rejected ACPI interface remains unknown. No new firmware image was
built or flashed during this run. RAM/QPI were accepted as the
operator-requested working baseline; no additional memory or QPI experiments
were issued. The initial UART-only observations below remain as recorded;
the appended screen-outcome observation supersedes their pending Windows
status without changing the raw evidence.

## Configuration and capture

```text
test ID: B06WI-HW-01
build identity: X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived B06WI delta
intended ROM SHA256: 3d9e6e03e2ed5a59dcdb95b34e2f0cd82e4becee9aa188b81efe2597374c6b04
flash chip: socketed W25Q128.V..M, 16 MiB; programmer readback not repeated
board: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, CPUID 000206c2, stepping 2, live microcode 0000001f
DIMM: BLS4G3D1609DS1S00, 4 GiB, 2Rx8, sole SPD54; slot label not restamped
GPU: Radeon HD5450 1002:68f9 with physical option ROM
USB medium: JetFlash Transcend 128GB 1100; operator's Hiren's medium
PSU: unchanged fixed configuration; model not restamped
boot type: one-second Shelly AC interruption; COLD_DEFAULT observed
serial: operator@serial-gateway.example.invalid /dev/ttyUSB1, 115200 8N1
capture duration: 600.030 seconds; USB selection at +359.047 seconds
POST evidence: serial B06WI READY implies a6/a7/a8 path; no independent POST-card capture
recovery: CH341 adapter reset, then one authorized AC cycle; no external flash recovery
```

The active build identity was observed in the fresh reset trace. Its intended
ROM hash is the preserved release hash, not a new in-circuit flash readback.
Initial UART sync attempts produced blank/repeated-byte data. Resetting only
the `1a86:7523` adapter and subsequently cycling the target restored a coherent
boot trace. The raw recording retains a short pre-reset noise/old-prompt prefix;
the first full B06WI banner starts the actual boot evidence.

## New hardware evidence

- Raw line 420: `B06WI-IRQ-ACPI1 READY`, with D31IR/D29IR/D28IR/D27IR/D26IR
  `0232/0237/3201/3216/3250`, D26:F2 pin D and IOAPIC ID 1. All 24
  redirection entries remained masked and all eight legacy PIRQs disabled.
  The new route transaction did not halt at A9 or report rollback.
- Line 824: FADT built with PM1 event/control/timer `0500/0504/0508`, GPE0
  `0520`, length `10`, matching extended GPE0 address and no i8042 flag.
- Lines 831/835: MCFG `e0000000`, bus 00–ff; MADT BSP APIC0, IOAPIC1,
  SCI IRQ9/GSI9 and IRQ0 override. This is construction telemetry, not a
  post-OS dump of all live AML tables.
- SeaBIOS initialized USB keyboards and enumerated the Transcend medium.
  At its actual menu, JetFlash was entry 2. The selector sent ESC and then
  that matched entry, without interacting with iPXE.
- Lines 1297 onward map USB drive `f21e0` to BIOS drive 0, SATA to drive 1;
  line 1324 transfers execution to `0000:7c00` from the selected medium.

No firmware reset banner appeared in the remaining approximately four-minute
post-selection observation interval. The last two messages are INT13 AH=15
queries for DL=00/01. In the exact linked SeaBIOS `handle_legacy_disk()`, these
are probes for absent floppy drives, not failed reads from USB BIOS drive 80h.

## Limits and next action

This run proves successful execution of the new routing transaction and
continued USB loader handoff. It does **not** prove interrupt delivery under
Windows, a resolved ACPI A5 error, a desktop, or a new memory-stability result.
Windows graphical output was not available over this UART session; operator
screen feedback was requested. The board was left running without another
reset or any write to the boot medium.

If A5 recurs, the next source-level experiment is a matching BSP ACPI processor
object and coherent LPC/system-device resources. These omissions were found
by source inspection; neither is yet identified as the actual bugcheck cause.
The fixed RAM/QPI path should not be requalified before trying that OS-facing
change. B06WI has no post-table writable debug monitor, so a live AML bypass
cannot simply be issued through its serial console.

## Immutable evidence

- [Raw UART](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index): 95,259 bytes, SHA256
  `d80b43fd2263f2a020be095d3825c352b5f6702b12d6e1f72d4ac00c8146a00e`.
- [Capture events/hash](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
- [Executed one-shot selector](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index): SHA256
  `ca0e6b7d26d419d38038d089988727a0e9ed423cfdd41dca0d22ffa56c15d6cb`.

The selector's event text accidentally omits the selected digit because its
regex match references a bytearray cleared after transmission. The UART write
precedes the clear, and the following drive mapping confirms JetFlash was
selected. The archived script and metadata retain this defect unchanged;
the temporary next-use copy now saves the digit before clearing the window.
The filename's `T2136` is a run label, not a precise UTC start timestamp.

## Subsequent operator screen outcome: ACPI A5 recurs

After the UART-only report, the operator supplied a screenshot for the active
B06WI JetFlash attempt showing Windows' restart screen and
`Stop code: ACPI_BIOS_ERROR (0xA5)`. The screenshot is conversation evidence,
not text contained in the UART capture; no standalone screenshot file or
image hash is recorded here. It does not show any of the four bugcheck
parameters or establish whether a subsequent reset actually completed.

Together, the serial recording and operator image establish that B06WI's new
IRQ transaction succeeds and does not prevent SeaBIOS/local USB execution,
but that its routing and table changes are insufficient to get this Windows
PE attempt beyond the ACPI bugcheck. The repeated top-level stop code does
not prove that B06WI rejects exactly the same ACPI object as B06WH.

The next isolated source experiment is B06WJ: a BSP `CP00` ACPI processor
object matching the existing single-CPU MADT, LPC PIC/PIT/RTC resources,
low-I/O root-bridge coverage and an HPET namespace/table description. This
does not requalify RAM/QPI or alter the working USB path. At this observation
point WJ is implemented in source but has no completed release build or
hardware run; the missing bugcheck parameters leave its causal hypothesis
unproved. B06WI remains the preserved routing/USB reference, not an A5 fix.
