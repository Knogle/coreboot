> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WK-HW-01: Intenso selected, boot sector rejected

**B06WK ramstage, new ACPI marker and SeaBIOS reached; requested Intenso
selected, but its boot sector was rejected. No OS entry or A5 outcome.**

```text
test ID: B06WK-HW-01
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus WK dirty source delta
observed identity: X58PROE-B06WK-ACPI-REPAIR-20260908
intended ROM hash: bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no flash readback
flash chip: socketed W25Q128.V..M, 16 MiB, inherited inventory
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, CPUID 206c2, inherited inventory
CPU stepping: 2, inherited inventory
microcode revision: prior 1f; initial CPU phase not captured this run
DIMM model: BLS4G3D1609DS1S00, 4 GiB 2Rx8, inherited inventory
DIMM slot: sole SPD54; physical slot label not restamped
GPU: Radeon HD5450 1002:68f9 with physical option ROM
PSU: unchanged; model not restamped
boot type: operator-started, reset/cold provenance not independently established
POST trace: no separate card capture; late RAMINIT, postcar, ramstage, ACPI, SeaBIOS observed
serial log: captures/b06wk-intenso-boot-20260908-01.raw
result: FIRMWARE/PAYLOAD/USB SELECTION PASS ONCE; BOOT SECTOR REJECTED
recovery required: none; no reset, guard manipulation or firmware write
notes: accepted RAM/QPI baseline not requalified; firmware-internal clear path left running
```

The initial six-second passive capture began at 10:28:51 UTC. It retained
48,026 bytes, almost entirely repeated LF bytes after a partial clear marker.
Its metadata reports timeout without a terminal marker; it does not establish
an init failure. Preserve it as transport evidence, not a new hardware pass.

The exclusive Intenso selector started 10:30:40.634816 UTC, preserving RX.
It captured completion of the last built-in clear window and coherent
postcar/ramstage output. No extra tests or register commands were sent.
Runtime line 518 contains:

```text
[ACPI] B06WK-REPAIR1 FADT_FLAGS=00000065 FIXED_PWRBTN=1 PM_EVENTS_LEFT_MASKED=1 CTBL_CONSUMER_FLAG=01
```

`ACPI: done.`, the 1670-byte CBFS DSDT and SeaBIOS's WK product identification
follow. This is native construction/identity evidence, not a complete runtime
table readback or OS validation. The compiled date banner says Sep 7; the WK
identity is observed but exact flash byte identity was not measured.

SeaBIOS enumerated Intenso Ultra Line 8.01, 512-byte sectors, 61,440,000
sectors, removable; JetFlash and the Crucial SATA SSD were also present.
At 10:33:03.247393 UTC the helper sent ESC after the exact invitation.
At 10:33:03.365799 UTC it sent the matched Intenso menu digit **2**:

```text
1. AHCI/5: CT1000MX500SSD1 ATA-10 Hard-Disk (931 GiBytes)
2. USB MSC Drive Intenso Ultra Line 8.01
3. USB MSC Drive JetFlash Transcend 128GB 1100
4. iPXE (PCI 02:00.0)
```

The Intenso drive object `f21f0` was mapped to BIOS disk 0. SeaBIOS then
reported `Boot failed: not a bootable disk` (interleaved debug text splits
the word on the raw console) and automatically fell back to iPXE/netboot.xyz.
No installer, alternative medium, or filesystem write was selected by us.

The actual pinned SeaBIOS `src/boot.c:886..906` checks carry after INT13
sector read, then emits this message only when the received MBR signature
differs from `0xaa55`. Thus the read did not report failure, but the received
sector lacked the expected `55 aa` tail. No raw-sector dump was made, so this
does not distinguish a wrongly written/non-BIOS medium from incorrect bytes
returned by the current storage path. It does not prove a UEFI-only ISO.
Next discriminator: identify the ISO/writing method and inspect sector 0 on
the USB preparation host; do not disable the firmware signature check.

## Immutable evidence and closure

- [Main raw log](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index): 89,763 bytes,
  SHA256 `ae047d7affdc19f8104ca73519fc8ac319e9e8c008b693963099f65636027107`.
- [TX/timing metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index):
  exactly ESC and Intenso digit 2, `selected=true`.
- [Initial passive capture](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index):
  SHA256 `6ca83f6ab4517bf02235d22a53f21c74b8cce4ec7d1c058c3b0eee6bb035b871`.
- [Executed selector](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index):
  SHA256 `ab747a4ef5100dc477afc73b787e2ebbee784c82e61eaa93be46fccc13e75060`.

After the boot failure/network fallback, the helper alone was stopped with
SIGINT (verified owned PID 133187), before its 300-second deadline. Python's
finally block released UART exclusivity and wrote the complete metadata;
the SSH command therefore exits nonzero with KeyboardInterrupt. This is not
a target reset or hardware failure. The target was left undisturbed.
