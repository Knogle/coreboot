> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VR isolated ICH10 SATA-to-AHCI map transition

## Purpose

B06VR is the first storage-controller experiment after B06VP proved the
SeaBIOS/VGA/network path and B06VQ isolated the Intel-required EHCI fields.
It preserves the complete inherited memory, QPI, X58, PCIe, LAPIC, USB,
Radeon-VBIOS, SeaBIOS and iPXE path.  Its sole new policy is the documented
ICH10 SATA mode/topology transition performed before PCI enumeration.

It does not claim a working SATA disk.  Port enablement, SATA clock policy,
AHCI MMIO, PHY programming and controller reset remain later experiments.

## Evidence and exact transition

The identical MSI vendor-booted reference exposes:

```text
00:1f.2 8086:3a22 class 010601
00:1f.5 absent
00:1f.2 MAP 90=0060
```

Intel ICH10 Datasheet 319973 sections 14.1.16 and 14.1.30 establish the
narrow sequence used here:

```text
MAP[7:6] = 01b   select AHCI mode
MAP[5]   = 1     map all six SATA ports to D31:F2
BAR5     = 0     mandatory after the component changes type
```

The hardware does not clear BAR5 when the component type changes.  B06VR
therefore treats the MAP update and the following 32-bit BAR5 clear as one
atomic helper operation.  It never copies the vendor runtime ABAR address.
The normal coreboot PCI allocator may subsequently probe and assign the new
memory BAR.

## Fail-closed admission and readback

Before the write B06VR accepts only one of two exact quiescent states:

1. cold/reset IDE identity: MAP `0000`, F2 `8086:3a20` class `0101`, F5
   `8086:3a26` class `0101`, normal headers and no I/O, memory or bus-master
   command bits on either function; or
2. retained/replay AHCI identity: MAP `0060`, F2 `8086:3a22` class/prog-if
   `010601`, F5 absent, a normal F2 header and no hardware command bits.

The second state performs no write.  A fresh transition uses a 16-bit masked
MAP read-modify-write followed immediately by the BAR5 clear.  The poststate
must have exact MAP `0060`, F2 identity/class/prog-if `3a22/010601`, F5 absent
and, when a write occurred, BAR5 exactly zero.  Any other state halts before
the generic PCI scan.

The new POST codes are:

```text
48  admitted transition/replay begins
4a  exact poststate accepted
4b  terminal rejection or readback failure
```

The successful path then executes the unchanged B06VO IOH bus route, B06VQ
EHCI field setup, IOU0 link start, PCI allocator, Radeon physical VBIOS,
SeaBIOS and iPXE.

## Deliberate exclusions

B06VR does not write:

- SATA `PCS`, `SCLKCG`, IDE timing or SIDX/SDAT;
- AHCI `CAP`, `GHC`, `PI`, `VSP` or any per-port register;
- LPC, GPIO, PIRQ, ACPI, SMM or SPI policy; or
- a fixed ABAR address.

The broad historical `i82801jx/sata.c` driver remains excluded because it
couples the mode switch to all of those unproved policies.  In particular,
the vendor runtime has VSP bit 0 set while the broad driver clears it.

## First hardware test

Use the established E5645, sole SPD-`0x54` DIMM, Radeon HD 5450 and RTL8168
configuration.  Capture COM1 at 115200 8N1 from before power-on.  Do not attach
a SATA device for the first run; this keeps the result limited to mode,
enumeration and resource allocation.

Expected new serial evidence is:

```text
[SATA] B06VR PRE F2_ID=3a208086 ... MAP=0000 ... F5_ID=3a268086 ...
[SATA] B06VR POST F2_ID=3a228086 F2_CLASSREV=010601.. ... MAP=0060 ... F5_ID=ffffffff ...
[SATA] B06VR AHCI MAP/type-transition gate PASS (write=1); ...
```

After allocation, coreboot should report a 2-KiB, 32-bit, non-prefetchable
BAR5 resource in the existing PCI MMIO aperture.  Its address is dynamic and
must not be compared to the Linux/vendor runtime address.  SeaBIOS may find
the AHCI controller, but no usable port is expected because PCS and the SATA
clock stage have not run.

On POST `4b`, missing F2, retained partial MAP state, unexpected command
decode or any identity mismatch, restore B06VQ or the known-good vendor chip.
Do not combine the first B06VR run with PCS, clock or AHCI-MMIO writes.

## Proof boundary

Two clean fixed-epoch builds are byte-identical, all source contracts pass,
the final machine code contains the masked MAP write followed by a zero BAR5
write, and the local W25Q128 image has verified top placement.  No B06VR image
has yet run on target hardware.  It therefore proves construction and the
fail-closed experiment design, not AHCI enumeration, storage access or boot.

See the [platform-initialization audit](../research/msi/platform-init-audit-2026-09-06.md)
and the [B06VQ USB experiment](b06vq-ich10-ehci-init.md).
