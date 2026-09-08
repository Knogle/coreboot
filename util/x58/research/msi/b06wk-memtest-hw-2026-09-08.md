> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WK-HW-08: operator-prepared Intenso memory-test boot

The operator requests one reboot and Intenso selection; its former Linux
contents have been replaced with Memtest. Exact product/version and legacy
boot compatibility are not yet observed. No storage/image modification,
RAM/QPI retuning or test-policy override is requested or performed.

## Configuration and hypothesis

```text
test ID: B06WK-HW-08
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta; unchanged
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash chip: socketed W25Q128.V..M,16MiB; inherited inventory
board revision: MSI X58 Pro-E/MS-7522; PCB revision not restamped
CPU: E5649@2.53GHz, identified by actual HW07 Linux brand string
CPU stepping: CPUID206c2,step2; await fresh firmware readout
microcode revision: inherited observed1f; await fresh readout
DIMM model: inherited BLS4G3D1609DS1S00,4GiB2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: planned user-requested target-only Shelly2-second timed interruption
POST trace: pending
serial log: captures/b06wk-intenso-memtest-20260908-08.raw; pending
result: PREPARING; no Memtest start/pass claimed
recovery required: none yet; established socketed known-good recovery retained
notes: exclusively own /dev/ttyUSB1,1152008N1; invitedESC then uniqueIntenso digit only; passive thereafter
```

Hypothesis: the unchanged WK path will admit SeaBIOS and boot the
operator-prepared memory-test medium. Expected evidence is a fresh WK
identity, normal inherited CSI/MINIT/postmem sequence, USB admission, actual
SeaBIOS Intenso menu entry and memory-test startup. A USB exact-gate failure
is a known short-reset failure mode; do not bypass it or reset-loop blindly.
If the tester has no serial output, a display photo is needed to establish
version, tested range, pass count and errors. A single error-free interval is
not qualification of all RAM or proof of the Linux performance cause.

Read-only preflight: no existing UART owner found; Shelly192.0.2.1
switch0 reports output=true,79.7W. ConsolePi is not power-cycled.

## Inventory correction to earlier HW07 notes

The immutable HW07 raw log at lines1736–1737 identifies the target as
`Intel(R) Xeon(R) CPU E5649 @ 2.53GHz`. The vendor reference dmesg line163
identifies E5645@2.40GHz. Earlier prose identifying the target as E5645 was
an inventory error, not a newly established CPU swap. Shared CPUID206c2
does not distinguish these SKUs. The TSC values2527.192MHz versus2405.878MHz
are consistent with the different nominal CPU frequencies; they are not
by themselves evidence of anomalous target overclocking/time calibration.
Historical raw logs and failed-test records remain unchanged.

## HW08 execution started

The new `--stop-after-intenso` capture mode passes25/25 host tests, including
old pre-reset Linux text, complete HW06 replay and rejection of later TAB
and all FIFO hex commands. Local/deployed helper SHA256:
`e33f2c33fd8690b343837b755ec4aa6b466e90e4d5c9a0480be8e4a21f30dd05`.

- Exclusive capture armed **18:26:31.535509UTC**, 1800s/32MiB bounds.
- One target-only Shelly `on=false&toggle_after=2` near **18:26:47UTC**;
  response `{"was_on":true}`. Follow-up output=true,73.8W.
- Fresh WK/c3 received **18:26:52UTC**; normal CSI reset/return path starts.
- Before the fresh WK marker the capture receives about4MiB of repeated,
  partly truncated old `obex-check-device` text. Preserve this prefix, but
  do not assign it to the new firmware/Memtest boot. Its repetition/transport
  provenance is not diagnosed here. No pre-reset UART keys were transmitted.
- The helper's `seabios` state only means waiting for its invitation; early
  romstage output at this checkpoint is not yet actual SeaBIOS execution.

At18:27:31UTC, CSI/MINIT have returned and the retained hard gate passes.
Fresh CPUID206c2 and microcode1f agree with the prior capture. The image
explicitly reports `RAM/QPI ASSUMED_STABLE` and omits its exhaustive tests;
the unchanged single-clear/sparse-readback steps still run before payload.
The first low-memory clear passes and the01000000..017fffff clear begins.
No new register writes, extra qualification loops or reset are injected.

## Result: USB preflight blocks the requested Memtest handoff

By18:32:32UTC the unchanged postmem/ramstage path reaches the late USB gate.
UHCI4 (`00:1a.0`) has expected USBCMD/USBSTS0000/0020. UHCI5/6 and UHCI1/2/3
instead show0018/0024, matching the earlier HW03–05 short-reset failures.
The exact terminal message is:

```text
[USB-AUTO] B06WK-AUTO-GPIO57-USB1 FAIL reason=exact-late-preflight rollback_requested=0 rollback_ok=1; payload blocked; cold recovery required
```

No SeaBIOS invitation/menu occurs; **no Intenso selection and no Memtest
execution/result** are established. No USB guard bypass or GPIO57 release
is injected. The zero-TX capture is stopped normally at18:33:43.402045UTC,
elapsed431.869s, with `error:null`; UART locks are released. Board remains
powered at the firmware halt. No second reset is performed. A full cold
recovery or authorization to exceed the established2-second off duration is
needed for a meaningfully different next attempt.

Final raw size4129633bytes; first fresh WK marker at byte4069102 leaves
60531bytes attributable to the fresh boot. The preceding repeated-text
prefix is retained as received and excluded from fresh-boot conclusions.

- [Full raw capture](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index), SHA256
  `be2706932d951b0ec65c682480516b4278f9ce264c3fd195eb22cffc355ba9b5`.
- [Metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index), SHA256
  `a300dd04fe0614bec6edfd8c3ae41c1cf1deaae293bd24e33906a2c8808471c9`.

Local raw byte count/hash match remote metadata; transmit event list is
empty. Intenso contents/bootability/version remain untested, so this failure
cannot be attributed to Memtest or the newly prepared medium.

## HW09: operator reports cold start; late attachment for Intenso selection

The operator reports a completed cold start and requests USB Intenso boot.
No agent reset, Shelly action or register/guard manipulation is performed.
Exclusive COM1 capture is armed **18:44:21.427607UTC** using the same
hash-verified helper with `--stop-after-intenso`. Initial RX is only the
suffix `01000000..017fffff; sparse-readback=9`; therefore the reset-vector,
CSI/MINIT and first cold-start identity are **not captured in this session**.
The later fresh WK ramstage identity is required before automatic ESC/Intenso
actions. This is not a claim of a complete reset-to-payload trace.

```text
test ID: B06WK-HW-09
build commit/ROM: intended unchanged WK,a2eb375438c85cd7908140636cbee8407bee8c0d plus archived delta; no programmer readback
ROM hash: bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0
flash/board: inherited W25Q128.V..M16MiB,MSI X58 Pro-E/MS-7522; PCB revision not restamped
CPU/stepping/microcode: inherited E5649,CPUID206c2,step2,ucode1f; not freshly read before attachment
DIMM/slot: inherited BLS4G3D1609DS1S00,4GiB2Rx8,SPD54; physical label not restamped
GPU/PSU: inherited HD5450,unchanged PSU; model not restamped
boot type: operator-reported cold start; power-off duration and rail discharge unmeasured
POST trace: early trace missed; attach during existing postmem clear
serial log: captures/b06wk-intenso-memtest-20260908-09.raw; in progress
result: awaiting USB gate/SeaBIOS/Intenso
recovery required: operator cold start after HW08 USB gate halt
notes: no extra tests or firmware/media changes; only invitedESC and uniqueIntenso digit authorized
```

HW09 interim readout: the first checkpoint has803254bytes, of which803036
are LF bytes. The first received fragment is followed by a long LF-only
interval, then real postmem progress: the01000000..017fffff clear passes and
02000000..027fffff begins. The newline stream then ceases. Its origin is not
established; byte growth/heartbeats alone are not treated as target progress.
The checkpoint contains no fresh WK identity yet, so no UART TX is allowed.

### HW09 USB recovery and actual Intenso boot-sector handoff

The second clear passes, then the fresh ramstage identity is captured and
the normal USB gate admits the payload. SeaBIOS initializes USB storage and
reports both USB and PS/2 keyboard initialization (accepted physical input
is not separately tested). The observed medium is **Intenso Alu Line5.00**,
512-byte sectors,7864320sectors (3.75GiB/about4GB), not HW07's earlier
UltraLine8.01 medium. The exact Memtest product/version remains unknown.

- **18:48:43.079883UTC:** actual SeaBIOS invitation, ESC transmitted once.
- **18:48:43.189182UTC:** unique menu entry2 names Intenso Alu Line5.00;
  ASCII2 transmitted once. Helper enters permanently passive mode.
- SeaBIOS maps that Intenso as boot drive0, reports `Booting from0000:7c00`,
  then the medium prints `GRUB loading...` and `Welcome to GRUB!`.

This proves payload admission, real USB boot-sector execution and GRUB entry,
not yet execution of a memory-test algorithm or a completed/error-free pass.
The displayed E820 map includes high RAM100000000..140000000; the tester's
actual selected range and effective cache setup have not been observed.
No media/configuration edits or further UART keys are issued.

Checkpoint02 raw878352bytes SHA256
`6b5bbc136abc63c6e0efb1220d879ce89671b5242d041d54f73aae1da6e63e01`.
The operator is asked for a display photo showing GRUB or Memtest with its
version/pass/errors; capture continues passively while awaiting that evidence.

### HW09 final capture outcome

No further serial bytes arrive after `Welcome to GRUB!`. The capture closes
normally at **18:51:24.691076UTC**, elapsed423.266s, without changing the
board. Metadata confirms `error:null`, exactly two UART TX events (ESC and
ASCII2), no reset and passive handoff. UART locks are released. Display
feedback remains pending; serial silence does not prove either a Memtest
hang or successful execution.

- [Final raw](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),878352bytes,
  SHA256`6b5bbc136abc63c6e0efb1220d879ce89671b5242d041d54f73aae1da6e63e01`.
- [Final metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
  SHA256`918e0bd740dadaaf9d49ae097c1d615c875677fadd757a11dae5fbebf5bdf933`.

Raw byte count/digest agree with the metadata. The retained file is a
late-postmem-to-GRUB trace, **not** a complete cold-boot log or a memory-test
result. This run recovers the USB/payload boundary that blocked HW08; no
new build, flash, firmware-policy change or medium rewrite was needed.

### Operator follow-up: Memtest did not run

The operator subsequently explicitly reports that Memtest did not run.
HW09 therefore proves only the Intenso boot-sector/GRUB boundary, not a
tester start or memory-test failure. The exact Memtest product/version,
visible stopping point and failing boot step remain unknown. No additional
hardware access or input was issued for this follow-up.

The requested [offline Linux/IRQ/cache analysis](b06wk-linux-latency-root-cause-analysis-2026-09-08.md)
keeps this observation separate from HW07's actual Linux execution and
HW08's pre-payload USB rejection. Existing capture bytes and earlier
pending-observation records are preserved.

## Later operator iPXE netboot: Memtest execution and 23 reported errors

This is a **separate later observation**, not a reinterpretation of the
earlier USB/GRUB attempt. The operator reports booting Memtest86+ through
iPXE and supplies `/path/to/workspace/Downloads/PXL_20260908_201023349.jpg`.
Photo SHA256:
`7dc5dac730ea6dcb7b7ba2113eaf8abdad42b5884b0e67e06bce4a786a7b9ccd`.
No same-boot serial capture, tester binary hash, selected memory-map source
or new ROM identity was obtained. WK is the presumed unchanged firmware,
not freshly verified by the photo. No agent hardware action was performed.

### Directly visible evidence

- Version 5.01; State Running; one active/total logical CPU; SMP disabled.
- E5649 @2.53GHz, measured CLK2527MHz, displayed CPU temperature35C.
- Memory4094M; displayed DDR3-798 (399MHz), CAS6-5-5-15,64-bit mode.
- L1/L2/L3 benchmark figures45125/19742/15503MB/s; DRAM-bandwidth field blank.
- Elapsed1:45, pass progress1%, completed passes0, Errors23.
- Current heading: Test#3, Moving inversions1s/0s Parallel, test84%,
  current range4096M–5120M, pattern00000000.
- Ten visible error rows, counts14–23, all on CPU0 and within the final
  4-KiB page below3GiB. The first thirteen error addresses are not shown.

| Failing address / Good | Bad | XOR error bits | Displayed count |
|---|---|---|---:|
| 0xbffffaac | 0xbffffa0a | 0x000000a6 | 14 |
| 0xbffffacc | 0xbffffa4a | 0x00000086 | 15 |
| 0xbffffaec | 0xbffffa4a | 0x000000a6 | 16 |
| 0xbffffb8c | 0xbffffb0a | 0x00000086 | 17 |
| 0xbffffbac | 0xbffffb0a | 0x000000a6 | 18 |
| 0xbffffbcc | 0xbffffb4a | 0x00000086 | 19 |
| 0xbffffbec | 0xbffffb4a | 0x000000a6 | 20 |
| 0xbffffc8c | 0xbffffc0a | 0x00000086 | 21 |
| 0xbffffcac | 0xbffffc0a | 0x000000a6 | 22 |
| 0xbffffccc | 0xbffffc4a | 0x00000086 | 23 |

These are actual reported mismatches, not a passing memory test. The count
does not establish23 distinct defective addresses, and the screenshot does
not prove that all23 errors belong to the visible page.

### Source-supported interpretation of the unusual display

The [official5.01 archive](https://memtest.org/download/archives/5.01/memtest86%2B-5.01.tar.gz)
explains the apparently inconsistent values. `main.c:815` prints the pattern
ID in the header, while `error.c:333` prints `test+1` in error rows. Index2
is the own-address test, index3 moving inversions. `ad_err2()` explicitly
uses the address as the expected value (`error.c:91–95`). Thus the visible
rowTst3/address-valuedGood entries fit the preceding own-address test while
the heading has advanced to moving inversions. Historical rows persist;
the current4–5GiB range does not locate these older failures in high RAM.
Exact correspondence to the downloaded binary remains unverified.

### Specific reservation conflict hypothesis

HW07 coreboot describes low RAM through0xbfffffff. SeaBIOS later leaves
0xbfffd000–0xbfffffff RESERVED in E820 (raw1312–1320 and1527). This is
exactly12KiB: its16MiB ZoneHigh minus16764928bytes returned at prepboot.
[SeaBIOS allocator](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
lines428–454/558–562 establish the allocation/reservation mechanism.
The recorded UHCI frame-list base at0xbfffd000 links this vicinity to USB
runtime structures; UHCI also allocates queue heads/transfer descriptors
through high-memory allocation. The owner of the specific failing dwords
on0xbffff000 is not established.

A predecessor trace
`captures/2026-09-06-b06vn-hw-02-seabios-through-ipxe.raw:292–295`
places the4-KiB EHCI00:1a.7 periodic frame list exactly at0xbffff000.
The current EHCI source uses the same allocation pattern. This strengthens
the USB-schedule hypothesis but is explicitly not a current Memtest-boot
PERIODICLISTBASE readback.

Memtest5.01 `memsize.c:39–43` prefers `query_linuxbios()` and calls
`query_pcbios()` only if that fails. `linuxbios.c:98–163` finds/follows LBIO
tables and converts their RAM entries into testable memory. It does not
merge subsequent SeaBIOS reservations. This provides a concrete way to
test memory still used by firmware/USB DMA. The pinned embedded iPXE code
does not itself promote reserved E820 entries to RAM; the tester's direct
coreboot-table preference is the stronger source-supported explanation.

The conjunction of a single visible failure page, its location in the
prior SeaBIOS reservation and this table-selection behavior makes a
firmware/tester ownership conflict a strong candidate. It does not prove
which map this particular binary selected or which writer changed memory.
The current netboot's final E820 map may differ from HW07. A DIMM/IMC fault,
aliasing or a residual protection-region issue is not conclusively excluded.
The vendor's8MiB TSEG setting is not a target readback and must not be
substituted for the observed12KiB SeaBIOS allocation.

Next discriminating test: use the final BIOS E820 reservations rather than
the earlier coreboot map, or deliberately exclude the known firmware-owned
tail for a narrowly labelled comparison. Do not remove the reservation or
alter DDR timings solely to suppress these errors. Record the tester binary,
actual selected map and additional error addresses. The previously found
4–5GiB WB-MTRR omission remains an independent issue; this screenshot
neither disproves it nor establishes high-RAM errors. No fix/test was run
while interpreting this photo.
