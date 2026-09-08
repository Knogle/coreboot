> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VS isolated ICH10 AHCI port-enable experiment

## Purpose

B06VS is the one-register successor to B06VR. It retains the complete
vendor-assisted memory/QPI handoff, X58 routing, selective PCI topology,
LAPIC, B06VQ USB field setup, Radeon physical VBIOS, SeaBIOS and iPXE path.
B06VR first changes the ICH10 SATA component to AHCI mode before enumeration;
B06VS changes only the supported-port enable fields after PCI resource
allocation has completed successfully.

This is not complete AHCI initialization. SATA clock policy, AHCI MMIO, PHY
state, controller reset and disk discovery remain outside this image.

## Evidence and sole new write

The identical MSI vendor-booted reference exposes:

```text
00:1f.2 8086:3a22 class 010601
00:1f.5 absent
00:1f.2 MAP 90=0060
00:1f.2 PCS 92=a23f
00:1f.2 SCGC 94=00000193
```

Intel ICH10 Family Datasheet 319973 section 14.1.31 documents PCS and requires
supported ports to be enabled before control is passed to an AHCI-aware
operating system. B06VS therefore performs exactly this selected-field
transition:

```text
PCS[5:0]: 00 -> 3f
```

If a retained run already has `PCS[5:0]=3f`, the write is skipped. The helper
uses an 8-bit read-modify-write at PCI configuration offset `0x92`. It clears
and replaces only bits 5:0. Thus reserved low-byte fields are preserved, and
the read-only port-presence fields in the high byte are not a write target.
Reserved bit 14 must read zero, while OOB Retry Mode (ORM) bit 15 is preserved
as a separate policy field. B06VS does not copy the complete vendor value
`a23f`.

## Fail-closed admission

The write is intentionally delayed until the `BS_DEV_RESOURCES` exit hook,
after the inherited domain/resource, leaf-resource and non-overlap audits have
all passed and coreboot has stored the assigned BARs. At that boundary B06VS
requires:

- D31:F2 exact ID/class/prog-if `8086:3a22/010601`, normal header;
- D31:F5 absent and MAP exactly `0060`;
- hardware I/O, memory and bus-master command bits all clear;
- a D31:F2 BAR5 resource of exactly `0x800` bytes, granularity 11 and
  alignment of at least 11 bits, with an actually `0x800`-aligned base;
- the exact resource flag word `MEM|ASSIGNED|STORED`, excluding every
  unexpected PCI, bridge, fixed, reserved or attribute flag;
- a base and end wholly inside the existing `c0000000..dfffffff` PCI MMIO
  aperture; and
- a raw 32-bit BAR5 whose type attributes and base agree with the coreboot
  resource object.

The PCS prestate admits only a low byte of exact `00` or exact `3f`. This also
requires bits 7:6 and reserved bit 14 to be zero. After the optional low-byte
write, the selected low byte must read back as exact `3f`, the complete SATA
identity/resource gate must still pass, reserved bit 14 must still be zero,
and ORM bit 15 must equal its pre-write value. Presence bits `13:8` may change
and are never an admission key.

The new POST codes are:

```text
4c  exact prestate admitted; PCS transition/replay begins
4e  exact selected-field and resource readback accepted
4f  terminal identity, resource, PCS or readback rejection
```

After success, five bounded read-only PCS snapshots are logged at absolute
times 0, 1, 10, 100 and 500 milliseconds. They report presence behavior only;
no sample result can pass or fail the experiment.

## Deliberate exclusions

B06VS does not write:

- MAP or BAR5 (those belong to B06VR and normal resource allocation);
- PCS presence fields, reserved bit 14 or the ORM policy in bit 15;
- SATA clock register SCGC;
- AHCI CAP, GHC, PI, VSP or any per-port MMIO register;
- IDE timing or SIDX/SDAT; or
- LPC, PIRQ, ACPI, SMM, SPI or unrelated platform policy.

The broad historical `i82801jx/sata.c` driver remains excluded. Its coupled
CAP/PI/VSP/PxCMD/indexed-register policy is not justified by this isolated
test, and its VSP bit-0 behavior disagrees with the vendor-runtime snapshot.

## Expected first-run evidence

Use the established E5645, sole SPD-`0x54` DIMM, Radeon HD 5450 and RTL8168
configuration. Capture COM1 at 115200 8N1 from before power-on. The first run
should use no SATA device so a result remains limited to controller
configuration and telemetry.

Expected new lines include:

```text
[SATA] B06VS PRE ... MAP=0060 PCS=.... EN=00 ... ORM=. RSV14=0 ... SIZE=800 ALIGN=.. GRAN=11 ...
[SATA] B06VS POST ... MAP=0060 PCS=..3f EN=3f ... ORM=. RSV14=0 ... SIZE=800 ALIGN=.. GRAN=11 ...
[SATA] B06VS PCS[5:0]=3f gate PASS (write=1); ...
[SATA] B06VS PCS_SAMPLE T_MS=0 ...
[SATA] B06VS PCS_SAMPLE T_MS=500 ...
```

The varying presence value is telemetry. Success at POST `4e` proves only the
mode identity, exact allocated ABAR contract and PCS port-enable write. It
does not prove SATA clocks, an AHCI command engine, a detected disk, data
transfer or storage boot. On POST `4f`, restore B06VR or the known-good vendor
chip and analyze the complete PRE line before changing another register.

See the [B06VR mode transition](b06vr-ich10-ahci-map.md) and the
[platform-initialization audit](../research/msi/platform-init-audit-2026-09-06.md).
