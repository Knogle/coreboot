> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VQ-PLATRO1 read-only platform census

## Purpose and isolation

`B06VQ-PLATRO1` is a diagnostic derivative of B06VQ. It preserves B06VQ's
guarded RAM/QPI, IOH routing, Radeon physical-VBIOS, SeaBIOS, iPXE and two
ICH10 EHCI-required-field writes. Its sole addition is one bounded,
non-fatal, read-only census after PCI resources and decodes are enabled and
immediately before SeaBIOS.

Build identity:

```text
X58PROE-B06VQ-PLATRO1-20260906
X58 Pro-E B06VQ PLATRO1
```

This image does not select USBTRACE1, the SATA MAP/PCS/clock experiments, the
complete historical ICH10 driver, ACPI generation or SMP. It creates no new
resource and changes no LPC, PM, GPIO, RCBA, IOAPIC, HPET, PIRQ or Super-I/O
register. PCI configuration reads still use the ordinary CF8 address cycle,
and logging necessarily changes UART status.

## Recorded state

The `[PLATRO]` block records:

- LPC identity, PMBASE, GPIOBASE, RCBA, decode enables, SERIRQ and four generic
  decode windows;
- PIRQ A through H routing bytes;
- PM1, GPE and SMI status/enable values;
- 4,096 bounded samples of the 24-bit ACPI PM timer;
- RCBA OIC and HPTC;
- HPET capability, configuration and bounded counter movement, but only when
  the exact RCBA and HPET-decode gates pass;
- all eight resources already owned by the custom PCI domain;
- overlap checks between those PCI windows and decoded SMBus/PM/GPIO ranges,
  IOAPIC, HPET, RCBA, LAPIC and the 16-MiB ROM window.

IOAPIC MMIO is deliberately not read by this release. If an exact decode gate
does not pass, the corresponding MMIO access is skipped and the skip is
explicit in the log. The final line reports
`PLATFORM_RESOURCES_MODELLED=0`: observing a fixed range is not yet the same
as reserving it in coreboot's resource tree.

## Expected serial boundary

On a successful inherited path the complete block is bounded by:

```text
[PLATRO] BEGIN X58PROE-B06VQ-PLATRO1-20260906 TARGET_STATE_READ_ONLY=1
...
[PLATRO] END NON_FATAL=1 IOAPIC_MMIO_SKIPPED=1
```

The census runs once. A duplicate call is suppressed and does not stop the
payload. Preserve the complete block rather than reporting only the last
line; the decode-gate values are needed to interpret every conditional read.

## First target procedure

Use the fixed E5645, sole SPD-`0x54` DIMM and HD 5450 setup. Capture COM1 at
115200 8N1 before reset. The image should continue to the ordinary B06VQ
SeaBIOS/iPXE path regardless of diagnostic mismatches, because this release
is observational.

Acceptance requires:

1. exactly one complete `[PLATRO] BEGIN`/`END` block;
2. no decode-gated MMIO access when its identity/base/enable tuple is wrong;
3. a moving PM timer when PM decode is valid;
4. consistent eight-resource reporting and zero unexplained overlap;
5. continued SeaBIOS entry after the census.

This build does not test USB enumeration. The already built
`B06VQ-USBTRACE1` image remains the first physical USB diagnostic.

## Failure and recovery boundary

The W25Q128 composite remains vendor-assisted locally and preserves the CAR
ROMMON/serial diagnostic path. Recovery uses a socketed known-good B06VP,
B06VQ or vendor chip. No target flash, reset or execution was performed while
constructing this release. RAM/QPI remain the requested development
assumption, not a completed ten-run validation milestone.
