> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WK: native ACPI repairs after the recurring Windows A5

## Scope

This is a coreboot/SeaBIOS firmware image for the fixed MSI X58 Pro-E test
configuration, not a Linux installer or an EDK2 migration. B06WJ and the
subsequent grouped RAM-only ACPI experiment both ended in operator-reported
Windows `ACPI_BIOS_ERROR (0xA5)`. The four bugcheck arguments and the tables
selected by the Windows kernel remain unknown. B06WK does not claim a proven
Windows fix.

The previous RAM-only SSDT and routing corrections are now generated natively
on every boot, with no GRUB table relocation. Two additional vendor-supported
description changes address legacy VGA resources and the fixed power button.
This is a grouped experimental improvement, not a one-variable causal test.

## Four functional deltas

| Area | B06WJ | B06WK |
|---|---|---|
| CTBL resource descriptor | Consumer macro is `10`, setting reserved bit 4; general flags `1c` | Common macro is `01`; generated general flags `0d` |
| Root `_PRT` | 20 entries | 64 entries; add D0/D1/D2/D4..D10/D22 pins A..D to GSI16..19; existing entries/child routes unchanged |
| PCI0 `_CRS` | High MMIO only, plus bus and I/O resources | Also produce `a0000..bffff` and `c0000..dffff`, each length `20000` |
| FADT fixed power button | Flags `75`, fixed button unadvertised, no PNP0C0C | Clear only POWER_BUTTON bit 4: flags `65`; OS owns event setup |

The common consumer macro correction is a specification fix and deliberately
not board-gated. Rebuilding an older configuration from the new common source
also picks it up; this does not alter an already archived ROM. The other
three deltas are gated by `CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR`, default off,
enabled by the new WK config and dependent on WJ/SeaBIOS.

The fixed-button description is supported by the ICH10 fixed `PWRBTN#`
interface and the vendor Linux `PWRF` enumeration. Setting FADT bit 4 without
a PNP0C0C object can also mean no button is present; the correction here is to
describe this board's actual fixed button, not to assert that this combination
is invariably malformed ACPI. PM1_EN/GPE/SMI remain masked in firmware.
No vendor Linux event-enable value is replayed. [Intel ICH10 datasheet,
sections 5.13.8.1 and 13.8.3.2](https://www.intel.sg/content/dam/doc/datasheet/io-controller-hub-10-family-datasheet.pdf)

Unchanged: CPU/microcode/CAR, local MSI CSI/MINIT-assisted RAM/QPI, postcar,
memory map, actual IRQ register transaction, GPIO57 USB release, USB driver
policy, SATA, GPU option ROM execution, SeaBIOS level 6, iPXE and ROMMON.
No additional device, S0/S3/S4/S5 method, SMM service, CPU-only reset, AP/SMP
startup, or GPIO/IOAPIC/event-register write is introduced by WK.

## Build and host validation

Build entry: `scripts/build_x58_b06wk.sh`.
Configuration: `configs/x58-pro-e-b06wk.config`.
Build ID: `X58PROE-B06WK-ACPI-REPAIR-20260908`.

The existing builder performs two clean builds with a fixed SOURCE_DATE_EPOCH,
compares ROM and iPXE bytes, verifies pinned payloads, and refuses to overwrite
different published artifacts. The existing local vendor-assisted composition
and wrapper patch are then retained, with the 4-MiB firmware placed at the top
of the 16-MiB W25Q128 image. Proprietary inputs remain under `blobs-local/`.

Native C generator regressions cover the complete 26-byte DWORD consumer
descriptor, the complete 46-byte QWORD consumer descriptor, and unchanged
producer flags for both sizes. Before the macro fix, exactly the two consumer
cases fail at byte 4 (`1c` vs `0d`); afterward all seven acpigen tests pass.
The make run target masks test-process failures, so the archived direct binary
runs and their exit statuses, not make's success alone, establish that result.

Additional host contracts test exact gating, previous-source preservation,
all 44 new routes, both legacy windows, FADT bit-only policy and build identity.
The final double build is byte-identical; 515/515 project host tests and
7/7 native C acpigen tests pass. The 1670-byte DSDT compiles without errors
or warnings and matches the table extracted from the final ROM. Artifact
hashes, source snapshots and the full log are recorded in the
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
Hardware execution and the Windows outcome remain untested.

## First hardware test

```text
board: MSI X58 Pro-E / MS-7522; PCB revision not restamped
flash: socketed W25Q128.V..M, 16 MiB
CPU: Xeon E5645, CPUID 206c2, stepping 2, prior microcode 1f
DIMM: one BLS4G3D1609DS1S00, 4 GiB 2Rx8, sole SPD54
GPU: Radeon HD5450 1002:68f9 with its physical option ROM
PSU: unchanged; model not restamped
console: COM1 3f8 / 115200 8N1, ConsolePi /dev/ttyUSB1
boot medium: operator-prepared Linux live USB; record exact image/hash
```

Expected result: the new build identity, inherited platform/payload path, and
the new table marker:

```text
[ACPI] B06WK-REPAIR1 FADT_FLAGS=00000065 FIXED_PWRBTN=1 PM_EVENTS_LEFT_MASKED=1 CTBL_CONSUMER_FLAG=01
```

Then boot the agreed live USB with ACPI/IRQ handling enabled, capturing the
kernel's first messages and the tables it actually selects. Do not begin with
`acpi=off`, `pci=noacpi` or `noapic`. Record kernel messages, ACPI tables,
PCI resources, `/proc/interrupts` and `/proc/iomem` if a shell is reached.
Linux success would be useful but would not prove Windows compatibility.
RAM/QPI remain the user's accepted working baseline, not a new qualification
exercise. No build-time host check counts as a hardware pass.

Possible failures: existing guard/POST stops, unchanged A5, a different OS
resource error, or a kernel hang/SCI problem after the new fixed-button
description. Preserve serial output and any screen error rather than silently
retrying. No automatic reset loop is added.

Recovery: retain the archived WJ and the known-good vendor chip; use external
read/write/verify for the exact 16-MiB image. If a target mains interruption
is needed, the established Shelly `192.0.2.215` switch 0 path uses the user's
one-second OFF/timed ON policy. Never reboot or power-cycle ConsolePi. No
flash, reset or Linux boot is performed as part of the build itself.
