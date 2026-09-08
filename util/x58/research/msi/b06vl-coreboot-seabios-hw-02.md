> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VL-HW-02: first complete coreboot-to-SeaBIOS hardware trace

Date: 2026-09-05

Result: **one retained hardware execution reached real coreboot postcar,
DRAM-backed ramstage, coreboot tables, and SeaBIOS 1.17.0, then stopped at the
reduced payload's intentional `Boot support not compiled in.` terminal.**

This is the first captured execution in this project that proves the complete
firmware path through a coreboot payload. It is one successful experimental
execution, not yet a reproducibility or operating-system boot result.

## Primary readable log

The normalized, chronological transcript is:

```text
research/msi/captures/2026-09-05-b06vl-hw-02-complete-success-readable.txt
SHA-256: 61bcab5ad23e4f8a5efa4899ee85c061acaeffb6ea9cb35233653f413f859a93
size: 35580 bytes
lines: 602
```

Normalization removed NUL bytes and ANSI terminal escapes and converted both
CRLF and lone CR line endings to LF. It did not synthesize firmware messages.
The file is assembled in order from
the immutable capture fragments listed below. A few bytes at the opening of
the first bootblock banner were already cropped/corrupted in the raw capture;
the subsequent complete B06VL romstage banners and all decisive phase-1
through terminal evidence are retained.

## Immutable source fragments

```text
b0a741b10ede00b0e5453e02506f39b55db7f15d0c74465c3f842255be46714c  research/msi/captures/2026-09-05-b06vl-hw-02-full-to-cbmem-window.raw
70c364d269975d9eb2f37328295ff87b9b24405d1d222130a50e6d378ba17ba0  research/msi/captures/2026-09-05-b06vl-hw-02-cbmem-pass-object-begin.raw
19eef9bbb82a503f8f68ea934af03370d3cdd0c9645c2575173ddf2d99c664ad  research/msi/captures/2026-09-05-b06vl-hw-02-object-pass-through-seabios-pci.raw
a67fba847744eed8e2614a918f7f1d0711bc25cd41724b4fcec834e80361fa0d  research/msi/captures/2026-09-05-b06vl-hw-02-seabios-terminal-valid-prefix.raw
```

The four fragments contain 20,199, 145, 16,680, and 40 bytes respectively.
The final 40-byte fragment is the immediate valid continuation after
`Relocating coreboot bios tables` and contains `NULL` followed by
`Boot support not compiled in.`.

## Evidence index

Line numbers below refer to the primary readable log.

| Lines | Direct evidence |
|---:|---|
| 3-23 | B06VL identity, ICH10R/LPC/UART initialization, real coreboot romstage |
| 26-67 | CSI phase 1 followed by its internal reset |
| 68-140 | CSI phase 2, return, and explicit IOH reset edge |
| 142-216 | CSI phase 3 and High-QPI endpoint, including `QPI80=070f0f03` later in the return report |
| 281-299 | MINIT returns with `EAX=0`; exact return telemetry; B06VL hard-return gate passes |
| 302-304 | complete uncached `0x00000000..0x0009ffff` test passes and is cleared |
| 305-309 | complete uncached 8-MiB CBMEM and object windows plus alias transaction pass and are cleared |
| 310-316 | CBMEM and v9/profile-7 handoff validate; I801 is rearmed; persistent guard is finalized |
| 320-325 | postcar is loaded at `0x017d3000` and starts |
| 329-338 | ramstage is loaded at `0x016b9000`, starts, accepts the exact handoff, and selects the payload path |
| 360-390 | coreboot table forwarder and full table are written; CBMEM inventory is emitted |
| 391-408 | PAM decode is enabled and verified; SeaBIOS SELF is loaded and coreboot jumps to `0x000fecd6` |
| 409-426 | SeaBIOS 1.17.0 runs, finds the coreboot table, memory map, CBMEM console, and MSI mainboard identity |
| 427-598 | SeaBIOS allocator/platform setup and raw PCI probe find 65 functions on bus 0 |
| 599-602 | table relocation completes far enough to reach the intended reduced-payload terminal |

The two complete 8-MiB tests each perform six full passes, or 48 MiB of
volatile traffic and 12,582,912 dword accesses per window. Together they
represent 96 MiB of uncached test traffic, in addition to the low-memory and
alias tests. This supports using the tested ranges for this one execution; it
does not validate all 4 GiB or long-term stability.

## Artifact and target identity

```text
image ID: X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905
coreboot base: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
public base SHA-256: dd9567411ccd4abdbe5b3efdaadef63d1192cef20d3dbfa421c9962df05c7a58
expected local W25Q128 SHA-256: 9f8e1085d09207073983bbb3e26ce51474ced604e020a4206406e1a485c3f5e7
flash: socketed W25Q128.V..M, 16 MiB
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode: 0000001f
DIMM: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8, sole SPD address 0x54
serial: COM1 0x3f8, 115200 8N1, no flow control
payload: SeaBIOS rel-1.17.0, reduced serial-only non-booting configuration
```

The executing flash was identified by the unique B06VL banners and behavior,
but no programmer read-back hash was recorded. The sequence began after a
Shelly-controlled one-second relay interruption. That is recorded as the
power action actually performed, not as independently proven electrical G3.
The two later resets visible in the log were intentional CSI/IOH continuation
resets.

## Precisely bounded claim

This execution proves:

- CPU execution with microcode `0x1f`, CAR, serial output, and three-stage CSI;
- the observed High-QPI endpoint and one successful guarded MINIT return;
- usable reads and writes throughout the explicitly tested low-memory, CBMEM,
  and object ranges during this execution;
- coreboot postcar and DRAM-backed ramstage execution;
- creation and consumption of coreboot tables and CBMEM data;
- real SeaBIOS machine code execution and bus-0 PCI discovery;
- the reduced payload reaching its designed terminal.

It does not yet prove:

- ten repeatable cold boots or a confirmed electrical G3 start;
- a full programmer read-back of the executing chip;
- stability of the complete 4-GiB DIMM;
- coreboot PCI bridge enumeration or resource assignment;
- a downstream GPU, VGA option ROM, SATA/USB boot device, network path, or OS;
- a boot-capable SeaBIOS configuration. `BOOT`, drives, VGA, and option ROMs
  were deliberately disabled in this diagnostic payload.

## Capture-artifact note

A later 16-MiB terminal read contained a valid 40-byte prefix followed by a
high-speed repetition of terminal bytes. Its data rate exceeded the physical
maximum of 115200 8N1, so the repeated tail is a ConsolePi/TTY capture-replay
artifact, not a firmware loop. Only the separately retained, hash-identified
40-byte prefix is included in the primary readable transcript.
