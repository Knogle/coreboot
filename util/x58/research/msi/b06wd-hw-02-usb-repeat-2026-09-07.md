> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WD-HW-02: controlled one-second AC interruption repeats the USB OC state

Date: 2026-09-07

Status: **REPEAT PASS THROUGH RAMSTAGE; USB CONTROLLERS STRUCTURALLY READY;
ALL TWELVE PORT VIEWS REPORT EXTERNAL OVERCURRENT ACTIVE; NO USB ENDPOINT**.

## Test record

```text
test ID: B06WD-HW-02
image ID: X58PROE-B06WD-SEABIOS-INPUT-20260906
build commit: a66879da91968b2fad3fc63fbaf1678651c952d9 on x58-pro-e/seabios
ROM hash: expected local W25Q128 9c2bd669f34d913d41e9902320e753e13a24ba3d936f1bd0a6e7f1b23e50533f; no programmer read-back retained
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 target; exact PCB revision not restamped
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2, microcode 0000001f
DIMM: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8, sole SPD responder 0x54
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
NIC: RTL8168 10ec:8168
PSU: unchanged test setup; exact model not restamped
boot type: Shelly relay off with device-local toggle_after=1; this proves a one-second mains interruption, not electrical G3
POST trace: serial covers reset entry, all three High-QPI phases, MINIT, RAM tests, postcar, ramstage, PCI/resources, SATA/AHCI/input/USB admission and terminal ACPI failure; no independent diagnostic-card trace retained
serial logs: three consecutive artifacts listed below
result: repeat of B06WD-HW-01 platform and USB result; terminal wrong-BDF ACPI gate remained expected
recovery required: none; B06WD halted at its deliberate ACPI gate
notes: physical USB-device attachment and port were not independently restamped; absence of CCS is not used as device-presence evidence
```

## Capture continuity and provenance

The capture utility held exclusive access to `/dev/ttyUSB1` at 115200 8N1.
The first and middle acquisitions reached their time limits while the target
was inside deliberately slow uncached RAM tests.  A new exclusive reader was
armed after each timeout.  The zero-byte middle artifact and the first line of
the final artifact bound a silent interval from the start to the completion of
the `01000000..017fffff` test.  No reset or power operation occurred between
the three files.

| Artifact | Bytes / lines | SHA-256 | Interpretation |
|---|---:|---|---|
| `2026-09-07-b06wd-usb-cold-02.raw` | 20,132 / 303 | `b4ffc764e7939f5e6bcc97b6bcbc214fe7fedca403e2f7ce40c91171a5c7ffcb` | Reset-to-MINIT prefix, low-memory test and start of the first 8-MiB test; plausible-rate capture, timeout before marker |
| `2026-09-07-b06wd-usb-cold-02b.raw` | 0 / 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | Retained proof that the UART was silent during the next six-minute acquisition; timeout before marker |
| `2026-09-07-b06wd-usb-cold-02c.raw` | 38,640 / 433 | `5789a719b1d796cc968b96ec0517d5bfc32b1e34f756d58bf2318b88b7db19b5` | Final RAM-test completion through the exact terminal ACPI marker; plausible-rate capture |

The matching metadata hashes are respectively
`abce31f53c93b3cf4eebc19989c458caa273e6db07ef6f2212c66cd5f3f69aa6`,
`a1d65701441d7850ef8f657311a3d9e7261eb5fcdc45627dd31792c1ccd0db82`
and `384ec2d275f335c37822e6a20b45779f671f858a3de705a02cffb713fe73e854`.
The RLE indexes are derived aids, not substitutes for the raw evidence.

## Repeated platform boundary

The run again reached all three High-QPI phases, returned from the locally
hash-pinned MINIT path, admitted profile 07, passed the uncached low-memory and
two 8-MiB windows, finalized the persistent guard, entered postcar and
ramstage, and completed selective PCI/resource work.  SATA policy again moved
from in-memory `0003` to `0002` without a hardware command write, and AHCI
again reached `AE=1`, `PI=3f` at its dynamic memory-only ABAR.

This is a useful repeat for the fixed configuration.  It is not ten-boot
qualification, native open RAM initialization, or general DIMM support.

## USB result

Both EHCI functions were present in D0 with memory decode and valid BARs; all
six UHCI functions were present with I/O decode.  The global USB gates again
reported no disabled controller, no EHCI clock gate, no port-power override,
normal controller mapping and native OC-pin routing:

```text
[USB-GATE] FD=02000001 USB_DIS=0000 CG=00000000 CG20=0 PPO12=000 MAP0=0 GPIO_OC=00000000/00000000
```

Every EHCI port was `00003030`; every UHCI port was `0c80`.  The independent
admission pass summarized the complete result:

```text
[USB-ADMIT] ELECTRICAL EHCI CCS=000 PE=000 OCA=fff OCC=fff UHCI CCS=000 PE=000 OCA=fff OCC=fff CLASS=OC_NO_CONNECT
[USB-ADMIT] B06WC-USBADMIT1 READY STRUCTURAL=PASS ELECTRICAL=CLASSIFIED MUTATIONS=0
```

Per Intel ICH10 register definitions, EHCI bit 4 and UHCI bit 10 are the
read-only live overcurrent-active state; the adjacent change bits are sticky
history.  Thus software cannot clear the live condition with a PORTSC write.
The fact that all twelve EHCI and UHCI views agree again makes the common
board-level OC#/VBUS/power-switch path the leading hypothesis.  It does not by
itself distinguish absent VBUS, a real overload, switch polarity, board-
revision differences, or an OC bias/wiring issue.

No USB descriptor, HID keyboard initialization or accepted key action was
observed.  The PS/2 result also repeated: KBC decode opened, but the interface
test returned `03` and keyboard reset ACK was `ff`.

## Next live discriminator

B06WD's terminal ACPI failure calls the non-interactive coreboot halt path, so
serial writes at that point cannot change registers.  A reset-retentive entry
that preserves configured SMBus state should fail closed into the CAR ROMMON.
At a genuine `rommon>` prompt, the next order is:

1. record B06WD-live GPIO bank-2 direction and level, especially GPIO56;
2. record all EHCI/UHCI port states without mutation;
3. run only exact-precondition, reversible GPIO experiments with immediate
   readback and rollback; and
4. recover with the socketed known-good image or a one-second Shelly cycle if
   the target stops before rollback.

GPIO56 is correlated by the public Rev2 schematic with `USB_MODE`, but the
target revision and UP7533 truth table remain unverified.  A low-level trial
must therefore be labelled invasive research rather than platform init.
