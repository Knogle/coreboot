> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VM automatic coreboot, VGA, storage and iPXE first-boot candidate

Status: **BUILT TWICE / BYTE-REPRODUCIBLE / STATICALLY AUDITED / HARDWARE
PARTIAL PASS THROUGH RAMSTAGE AND RTL8168 ENUMERATION / STOPPED BEFORE
SEABIOS AT POST 38**.

B06VM is the first image in this project intended to behave as a bootable
legacy BIOS rather than a diagnostic payload probe.  On a valid B06VL-derived
memory handoff it no longer stops in RAMMON.  It continues automatically
through coreboot ramstage, a deliberately bounded PCI/resource path, and a
boot-enabled SeaBIOS with VGA option-ROM and iPXE support.

This is a real first-boot candidate, not yet a production-complete firmware.
The distinction matters: local disk, USB and network boot code is present, but
the platform still has only one tested memory topology and no completed SMP,
ACPI, SMM, S3, generalized PCI, or operating-system validation.

## Intended automatic path

```text
reset
  -> B06VL vendor-assisted CSI/MINIT path and independent hard-return gates
  -> uncached low-memory, alias and two complete 8-MiB window tests
  -> version-9/profile-7 CBMEM handoff
  -> postcar with B06VM MTRRs
  -> coreboot ramstage (no RAMMON prompt)
  -> exact PCI preflight, scan, allocation and enable audits
  -> coreboot tables and PAM/shadow transition
  -> SeaBIOS rel-1.17.0
  -> physical GF108 VGA ROM
  -> SeaBIOS boot menu / ATA / USB / CD-ROM / RTL8168 iPXE
```

Any inherited B06VL memory/QPI/IOH gate failure still stops in the early CAR
diagnostic path.  B06VM adds fail-closed ramstage POST codes `36` through `3e`
for invalid handoff, platform state, topology, identity, command, bus,
resource, overlap, or enable state.  It does not silently fall back to generic
PCI probing.

## Selective PCI contract

The B06VL hardware trace proved only bus-0 visibility.  B06VM therefore uses
`MINIMAL_PCI_SCANNING` and admits exactly 12 root functions plus two fixed
downstream functions:

- X58 PEG `00:03.0` and downstream `10de:0f00` GF108/GeForce GT 630;
- ICH10R root port `00:1c.4` and downstream `10ec:8168` revision 2;
- UHCI/EHCI functions `00:1a.0/1/2/7` and `00:1d.0/1/2/7`;
- ICH10R SATA `00:1f.2` (`8086:3a20`) and `00:1f.5` (`8086:3a26`), both
  required to appear in the already observed legacy-IDE class.

Vendor/device ID, class and header type are checked before BAR sizing.  The
two bridges receive distinct temporary downstream bus numbers.  Coreboot does
not enumerate the X58 internal `00:0d.*` through `00:16.*` function complex.
Bus mastering is enabled only on the two forwarding bridges; endpoint BME
remains off until a payload driver claims the endpoint.

The fixed resource model is:

```text
RAM        00000000..0009ffff
RAM        00100000..bfffffff
PCI MMIO   c0000000..dfffffff
PCIEXBAR   e0000000..efffffff  (reserved ECAM)
RAM       100000000..13fffffff
PCI I/O        1000..ffff
```

IOH TOLM raw `bc000000` is an inclusive 64-MiB-block encoding: field value 47
covers blocks 0 through 47 and therefore ends exclusively at `c0000000`.
TOHM `00000001:3c000000` similarly ends exclusively at `140000000`.  The map
does not overlap the PCI aperture.

The postcar MTRRs make 0--2 GiB and 2--3 GiB write-back, overlay VGA legacy
memory `a0000..bffff` as uncacheable, and keep the top ROM window
`fffc0000..ffffffff` write-protected.  The remapped 4--5-GiB range remains UC
in this first 32-bit bring-up profile; it is reported as RAM but is not yet a
performance/stability claim.

## POST milestones

```text
2b  exact B06VL-v9 handoff and platform preflight entered
2c  selected root functions quiesced safely
2d  exact 12-root / 2-downstream topology accepted
2e  fixed RAM, I/O, MMIO and ECAM resources published
2f  BAR allocation and non-overlap audit passed; endpoint BME still off
34  decode/master enable audit passed; only bridge BME enabled
36  handoff failure
37  X58/PCIEXBAR/SAD/TOLM/TOHM platform-state failure
38  topology failure
39  PCI identity/class/header failure
3a  PCI command-state failure
3b  bridge/bus-number failure
3c  resource failure
3d  resource overlap
3e  final enable-state failure
```

The expected serial boundary after B06VL's already observed success trace is:

```text
[PAYLOAD] B06VM exact v9 handoff accepted; continuing to selective PCI enumeration
[RAMSTAGE] X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905 selective PCI preflight
[RAMSTAGE] B06VM exact 12-root/2-downstream PCI scan accepted
[RAMSTAGE] B06VM RAM=0-640K,1M-3G,4G-5G PCI_IO=1000-ffff PCI_MMIO=c0000000-dfffffff
[RAMSTAGE] B06VM PCI allocation audit PASS; endpoint BME remains off
[RAMSTAGE] B06VM decodes enabled; BME only on two forwarding bridges
SeaBIOS (version rel-1.17.0-0-gb52ca86e...)
```

VGA text output and the SeaBIOS banner are expected only after the physical
GT630 option ROM successfully runs.  Absence of video is not by itself proof
that SeaBIOS was not reached; COM1 at 115200 8N1 remains authoritative.

## SeaBIOS and option-ROM policy

SeaBIOS includes boot menu/order, INT 13h ATA PIO, AHCI code for a future
class-0106 controller state, CD-ROM boot, UHCI/EHCI USB mass storage and
keyboard, PS/2, SERCON, real hardware IRQs, RTC/PM/TSC timers, PCIBIOS,
PNPBIOS, PMM, option-ROM execution and coreboot-flash access.  ATA DMA,
parallel option-ROM threads, upper-memory malloc, S3 and VGA hooks are disabled
for the first run.

The exact GT630 legacy image is 56,320 bytes and identifies `10de:0f00`.  Its
byte sum is `0xff`, so B06VM intentionally sets the B06VM-only SeaBIOS integer
`etc/optionroms-checksum=0`.  The image is bound to one fixed-card experiment;
this checksum relaxation must not be generalized.

SeaBIOS always performs a read-only inventory of visible PCI functions.  The
additional B06VM integer `etc/pci-optionrom-exec=1` restricts physical PCI ROM
mapping/execution to VGA.  This prevents ROM-BAR sizing on unrelated visible
X58 functions.  The CBFS `pci10ec,8168.rom` remains independently matched to
the RTL8168 and provides its BEV boot entry.

The legacy C0000--EFFFF window is 196,608 bytes.  After SeaBIOS alignment the
GT630 consumes 57,344 bytes and iPXE 96,256 bytes, leaving 43,008 bytes.  This
is why SeaBIOS init relocation is enabled and upper-memory allocation is
disabled.

## iPXE profile

The embedded RTL8168 legacy ROM is built from iPXE commit
`7c39c04a537ce29dccc6f2bae9749d1d371429c1`.  It includes HTTPS and the
`imgtrust`/`imgverify` commands, but has no embedded site script, private trust
key, or direct serial console.  SeaBIOS SERCON supplies the shared console.
The exact embedded ROM is 95,232 bytes, has a valid zero checksum, PCI ID
`10ec:8168`, and SHA-256
`8b21a133a1a795bcd19f7945c23306d37b348fedc8299527e3ce3d66c906ee80`.

This is a network boot/debug transport after platform and memory init, not a
pre-DRAM TFTP loader.  Network success still depends on actual bridge/BAR/IRQ
state and RTL8168 driver execution on hardware.

## First-run configuration and recovery

Use only the configuration for which the inherited path has evidence:

```text
CPU:   Xeon E5645, CPUID 000206c2, microcode revision 0000001f
DIMM:  one BLS4G3D1609DS1S00 at the sole responding SPD address 0x54
GPU:   GF108 / GeForce GT 630, PCI ID 10de:0f00, exact measured legacy ROM
NIC:   onboard RTL8168, PCI ID 10ec:8168, revision 02
flash: socketed W25Q128.V..M, 16 MiB
UART:  COM1 0x3f8, 115200 8N1, no flow control
```

Keep the B06VL known-good diagnostic chip untouched and have an externally
verified vendor/recovery chip ready.  Program only the 16-MiB file named in
the B06VM manifest, read it back, and require the complete-image SHA-256 to
match before installing it.  Capture serial from before power-on.  A B06VM
failure is recovered by replacing/reprogramming the socketed chip; no automatic
rollback to B06VL exists.

## First hardware result and B06VN successor

The 2026-09-06 run passed the inherited memory gates, CBMEM, postcar and real
ramstage.  X58 `00:03.0` received downstream bus 1, but its static child
returned no device (`ffff:ffff`).  ICH10R `00:1c.4` received bus 2 and its
RTL8168 answered as `10ec:8168`.  B06VM therefore stopped at the exact topology
guard, POST `38`; SeaBIOS and the physical VGA ROM did not start.

The installed GPU was subsequently identified as an AMD Radeon HD 5450, not
the fixed GT630 target.  This did not itself cause POST `38`: an answering AMD
card would have reached the distinct identity failure POST `39`.  The result
instead isolated missing PEG-link/downstream visibility.  See the
[immutable hardware analysis](../research/msi/b06vm-hw-02-2026-09-06.md).

B06VN tests the next single hypothesis: start X58 IOU0/`00:03.0` in x16 mode
through `PCIE_PRTx_BIF_CTRL`, record bounded link telemetry, accept a
runtime-observed AMD VGA identity, and let SeaBIOS use the HD 5450's onboard
physical VBIOS.  See the [B06VN procedure](b06vn-iou0-physical-vbios.md).

## What remains before calling it production-complete

- ten controlled cold boots and ten applicable warm resets;
- full-DIMM and long external memory testing, including the 4--5-GiB remap;
- stable GPU option-ROM output and a repeatable SeaBIOS menu;
- verified ATA/USB device discovery and a real local boot;
- verified RTL8168 iPXE link, DHCP and image transfer;
- AP/SMP initialization (`MAX_CPUS=1` is still the bounded target);
- PIRQ/IOAPIC/ACPI tables, power management, SMM and S3;
- generalized DIMM, CPU, GPU, NIC and PCI-slot support;
- explicit AHCI-mode programming if AHCI is desired instead of legacy IDE;
- ultimately replacing the local MSI CSI/MINIT-assisted path with open init.

Those items define the next stabilization work.  They are not reasons to keep
B06VM in ROMMON: the image is intentionally built to attempt the first real
VGA/storage/network boot now.
