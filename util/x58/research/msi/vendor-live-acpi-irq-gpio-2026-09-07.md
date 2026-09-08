> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI X58 Pro-E live vendor ACPI, IRQ and GPIO correlation

Date: 2026-09-07

## Scope and result

Two read-only snapshots were taken from the identical MSI X58 Pro-E reference
machine at `192.0.2.203` while it was running the vendor firmware.  The
complete RCBA image, all decoded GPIO configuration registers, the LPC routing
registers, ELCR, PIC masks, ACPI tables and PCI configuration spaces were
captured twice.  The two snapshots agree bit-for-bit in RCBA, GPIO, LPC route
policy, ELCR and PIC state.  Only the free-running PM timer changed.

This is strong evidence for a stable board policy.  It is not permission to
copy vendor AML or an entire runtime register image.  In particular, PM status,
GPE status, input levels, OS-selected PIRQ link values and SMM state have
runtime semantics.  The native implementation must reproduce the static
contract and keep unsupported SMM, sleep and wake facilities disabled.

An additional reproducible offline extraction report supplied by the operator
corroborates the known AMIBIOS layout: one 28,290-byte DSDT from module `10`,
six CPU power-management SSDTs, and standard-table templates from module `1b`.
It also independently identifies the DSDT `OperationRegion (BIOS,
SystemMemory, 0xffffff00, 0xff)` with 37 named fields.  Until the reported
archive itself is placed in the local ignored corpus, those new counts are
operator-provided evidence rather than locally reverified artifacts.  The
fixed operation region is nevertheless consistent with the already observed
vendor-runtime dependency and reinforces the decision not to transplant the
DSDT.

The proprietary raw snapshots remain below the ignored `blobs-local/` tree:

```text
blobs-local/msi-x58-pro-e/vendor-live-2026-09-07/platform-ro/
blobs-local/msi-x58-pro-e/vendor-live-2026-09-07/platform-ro-2/
```

Their `metadata.json` SHA-256 values are:

```text
c82b5855861055ea84a8fbdc567cb47d595961e82d3bec149fbe429e8fadc7f4
90788ecf5f0235696a8fba8376f6c382a682f65902ef5e1b8a5794bc83ec21b7
```

The different metadata hashes include timestamps, the changing PM timer and a
corrected raw-RCBA filename.  Direct comparison of the 16-KiB RCBA files and
the GPIO/LPC fields establishes the stable equality described above.

## Vendor static platform contract

The vendor machine decodes PMBASE at `0800`, GPIOBASE at `0500`, RCBA at
`fed1c000`, the ICH10 IOAPIC at `fec00000` and HPET at `fed00000`.  Coreboot's
current decoded PMBASE is intentionally `0500`; ACPI addresses therefore have
to be derived from the live base rather than copied literally.

Stable ICH10 internal interrupt policy:

| Register | Vendor value | Meaning used by the clean-room port |
|---|---:|---|
| D31IP | `03243200` | internal pin selection |
| D30IP | `00000000` | no admitted D30 source |
| D29IP | `10000321` | D29 USB pin selection |
| D28IP | `00214321` | PCIe root-port pin selection |
| D27IP | `00000001` | HDA pin selection |
| D26IP | `30000421` | D26 USB pin selection |
| D31IR | `0232` | A/B/C map to GSI 18/19/18 |
| D30IR | `0000` | no admitted route |
| D29IR | `0237` | A/B/C/D map to GSI 23/19/18/16 |
| D28IR | `3201` | A/B/C/D map to GSI 17/16/18/19 |
| D27IR | `3216` | A maps to GSI 22; unused fields retained |
| D26IR | `3250` | A/B/C/D map to GSI 16/21/18/19 |
| OIC | `03` | ICH10 IOAPIC decode enabled |

The vendor PCI interrupt-pin bytes corroborate the active USB routes:

```text
D26 F0/F1/F2/F7 = A/B/D/C
D29 F0/F1/F2/F7 = A/B/C/A
D31 F2          = B
D28 F4          = A
```

The running vendor-booted Linux instance independently consumes precisely the
expected legacy GSIs.  `/proc/interrupts` shows IRQ0 delivered as IOAPIC
GSI2-edge, SCI on GSI9, and active INTx users on GSIs 16, 17, 18, 19, 21 and
23.  The latter set is exactly the union selected by the stable D26--D31 route
words; the onboard AHCI and RTL8168 subsequently use MSI/MSI-X where their
drivers support it.

The current coreboot target differs materially for D26:F2: its reset/early
state reports pin C and `D26IP=30000321`.  A native vendor-correlated route
stage therefore must change only that admitted field to pin D before exposing
the matching `_PRT`.  The D29IP difference is confined to absent D29:F3 and is
not a reason to rewrite an inactive function.

Vendor PIRQ bytes were stable at `8a/85/8e/8b/80/8f/80/83`.  Bit 7 keeps every
legacy PIRQ route disabled; the low nibbles are link-device IRQ selections.
The APIC-first port should keep the routes disabled and publish direct GSIs.
It must not copy the generic coreboot policy which enables every PIRQ on IRQ11.

## ACPI contract

The live vendor tables establish:

- SCI interrupt 9;
- PM1 event at `PMBASE+00`, PM1 control at `PMBASE+04`, PM timer at
  `PMBASE+08`;
- GPE0 status/enable block at `PMBASE+20`, length 16;
- IRQ0 to GSI2 with conforming polarity/trigger;
- IRQ9 to GSI9, active-high and level-triggered;
- one ICH10 IOAPIC at `fec00000`, GSI base 0;
- MCFG at `e0000000`, segment 0, buses 00--ff;
- HPET decode at `fed00000`.

The vendor FADT is revision 1.  The native coreboot FADT is revision 6, for
which Windows requires the extended GPE0 GAS to accompany the legacy GPE0
fields.  Therefore the native table should publish `0520`, length 16 and let
`fill_fadt_extended_pm_io()` create `X_GPE0_BLK`; all GPE enable registers can
and should remain zero.

The clean-room DSDT needs direct-GSI `_PRT` entries for the admitted topology,
including the D3 IOH bridge carrying the HD 5450 and the D28:F4 root port
carrying the RTL8168.  It must not import the vendor SMI command, link-device
mutation methods, USB wake AML, `_PTS`, `_WAK`, S3/S4 policy or writable BIOS
globals.

## GPIO correlation and boundary

Stable vendor GPIO configuration:

```text
USE_SEL  = 19fdff01    IO_SEL  = e0ff2cc3
BLINK    = 00040000    INV     = 00002000
USE_SEL2 = 030300ff    IO_SEL2 = 0d54fffe
```

The two observed level words were also equal (`e7efecfe/0ff4ff99`), but a level
register mixes output latches with live input pins.  Complete LVL writes are
therefore forbidden.  Any GPIO mutation must:

1. exact-gate LPC identity, GPIO decode and the expected predecessor state;
2. preload only a named output latch with a masked write;
3. change only that pin's USE_SEL/IO_SEL fields;
4. require selected-bit readback;
5. preserve the proven B06WG GPIO57 high-output USB-power contract;
6. retain a bounded failure path and socketed-flash recovery.

The stable vendor configuration is sufficient to guide pin-by-pin admission,
but not sufficient to safely promote every pin in one image.  Full-word GPIO
cloning could switch rail enables, resets, fan controls or strap-overloaded
pads before their sequencing is understood.

## Coreboot-target comparison

After the B06WH Windows `ACPI_BIOS_ERROR (0xA5)` auto-reboot, the target was
left in CAR ROMMON.  A read-only register transcript is preserved as:

```text
research/msi/captures/2026-09-07-b06wh-post-a5-routing-readonly.raw
SHA-256 7e6568dcfa92bf8070a62cb4a022bb1d07fb8fe002f15d6752f1da32c0b1928f
```

That early state has `D31/D29/D28/D27/D26IR=3210/3210/3210/3210/3210`,
`OIC=00`, all PIRQ bytes `80`, no GPE enables and no GPIO routing.  The earlier
B06WH payload trace proves that ramstage subsequently enables OIC, masks all
24 redirection entries, establishes SCI mode and keeps every event source
disabled.  It also proves that the emitted DSDT has no `_PRT`, the revision-6
FADT has no GPE0 block and the MADT omits IRQ0-to-GSI2.

Those three table/routing omissions are the leading explanation for the first
Windows ACPI rejection.  The exact `0xA5` subtype still requires the four
bugcheck parameters through KDCOM or an on-screen parameter-enabled WinPE.

## Implementation order

1. Add FADT GPE0 `PMBASE+20`, length 16, with all enables still zero.
2. Add MADT IRQ0-to-GSI2 and retain SCI IRQ9-to-GSI9 level/high.
3. Exact-gate the active PCI interrupt-pin tuple, program the vendor-correlated
   D26 active-pin field plus D26/D29/D28/D27/D31 route words, and verify full
   readback while every destination remains masked.
4. Publish clean-room direct-GSI root and child-bridge `_PRT` packages for only
   the admitted devices.
5. Keep PIRQ, PCI `INT_LINE`, SMI/GPE enables and IOAPIC redirection entries
   disabled/masked for OS ownership.
6. Admit further GPIOs individually after their board function and sequencing
   are identified; do not bulk-copy LVL or `GPIO_ROUT`.
