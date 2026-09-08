> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VQ-ICHBASE1 six-field ICH10 baseline

## Purpose and isolation

`B06VQ-ICHBASE1` is an independent, default-off derivative of B06VQ. It
preserves the existing vendor-assisted RAM/QPI path, IOH routing, physical
Radeon VBIOS, SeaBIOS and iPXE policy. Before IOH bus routing, the existing
two-field EHCI helper, IOU0 start and PCI enumeration, it applies exactly six
ICH10 RCBA fields which the Intel ICH10 datasheet marks as BIOS-required.

Build identity:

```text
X58PROE-B06VQ-ICHBASE1-20260906
X58 Pro-E B06VQ ICHBASE1
```

The complete historical `SOUTHBRIDGE_INTEL_I82801JX` device model remains
disabled. This variant enables only the separately compiled
`SOUTHBRIDGE_INTEL_I82801JX_REQUIRED_FIELDS` helper. It does not select the
PLATRO1 or USBTRACE1 diagnostics, the B06VR/B06VS/B06VT SATA experiments,
ACPI generation or SMP.

## Hardware evidence behind the tuple

The exact reset tuple and the exact target tuple below were captured on B06VP
in one retained CAR ROMMON session. A sealed 24-operation script first
asserted LPC identity `8086:3a16`, RCBA `fed1c001`, unlocked FDSW `00` and all
six complete reset dwords. It then performed the six masked updates in the
same order used here and asserted each complete target dword. All 24
operations passed and direct reads after discarding the transaction record
confirmed that the values remained programmed.

This is one live register-mechanics result. It does not by itself prove that
the fields improve USB, SATA, interrupts, ACPI or payload behavior.

| Register | RCBA offset | Exact admitted PRE | Selected field/value | Exact TARGET |
|---|---:|---:|---:|---:|
| CIR8 | `3430` | `00000000` | bits 1:0 = `2` | `00000002` |
| FD | `3418` | `00000000` | set bit 0 | `00000001` |
| CIR9 | `350c` | `00000020` | bits 27:26 = `2` | `08000020` |
| CIR7 | `2034` | `b2b477cc` | bits 19:16 = `5` | `b2b577cc` |
| CIR13 | `0f20` | `b2b477cc` | bits 19:16 = `5` | `b2b577cc` |
| CIR10 | `352c` | `0008c008` | set bits 17:16 | `000bc008` |

The register offsets and requirements are documented in Intel ICH10 Family
Datasheet 319973-003, sections 10.1.44, 10.1.52, 10.1.77, 10.1.80, 10.1.81
and 10.1.83. Unknown field names remain deliberately neutral in source.

## Exact runtime contract

The board wrapper is fail-closed:

1. mark the one-shot attempt before the first snapshot read;
2. require exact LPC ID `3a168086` and enabled RCBA `fed1c001` before any RCBA
   MMIO access;
3. require FDSW byte `00`;
4. accept only the complete PRE tuple above or the complete TARGET tuple;
5. reject every mixed or partial tuple before calling the helper;
6. call the six-update helper once only for PRE; an already complete TARGET
   performs no write;
7. reread and require all fixed gates and all six complete TARGET dwords.

POST codes are:

```text
54  six-field transaction begins after complete admission
56  complete target tuple verified
57  terminal failure
```

The serial success line ends with:

```text
[ICHBASE] B06VQ-ICHBASE1 six required fields exact gate PASS (write=0|1); GCS/CIR5/FDSW/hide/lock/RPFN/MAP/PMIR/IRQ untouched
```

GCS remains intentionally separate. The full historical ICH10 driver programs
its GCS field before calling the same helper, while ICHBASE1 does not program
GCS at all. CIR5, FDSW, function hiding, RPFN, MAP, PMIR, interrupt routing,
watchdog, GPIO, SATA and lock policy are also outside this experiment.

## First target procedure

Use the fixed E5645, sole SPD-`0x54` DIMM and HD 5450 setup. Capture COM1 at
115200 8N1 before reset. Preserve both `[ICHBASE] PRE` and `POST` lines, the
POST trace, the following B06VQ USB telemetry and the complete SeaBIOS/iPXE
result.

The useful first comparison is a true reset-state boot (`write=1`). A retained
TARGET boot (`write=0`) verifies idempotence but is not a substitute. Stop on
POST `57`; do not broaden the accepted tuple in the same image.

## Failure and recovery boundary

The public 4-MiB base contains no MSI CSI/MINIT bytes. The ignored local
W25Q128 image is vendor-assisted and retains the existing CAR ROMMON/serial
diagnostic path. Recovery uses the socketed known-good B06VP/B06VQ or vendor
chip.

No target flash, reset or execution was performed while producing this
release. RAM/QPI remain the requested development assumption, not a completed
ten-run validation milestone, and none of the six fields has yet been tested
inside this automatic ramstage path.
