> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI X58 Pro-E overall state, 2026-09-07

The fixed E5645 / one SPD54 DIMM / Radeon HD5450 configuration reaches real
coreboot ramstage, SeaBIOS, display, working USB input and external boot code.
B06WH-HW-02 progressed to graphical `ACPI_BIOS_ERROR (0xA5)` and reset.
The preceding run, B06WI-HW-01, passes the new IRQ/ACPI transaction and table
construction, then launches the JetFlash boot sector through SeaBIOS. The
operator's subsequent screenshot shows `ACPI_BIOS_ERROR (0xA5)` again,
without the four bugcheck parameters. Thus B06WI's changes are insufficient
to complete this Windows PE boot; they do not identify the exact ACPI defect.
No completed Windows or Linux desktop/shell boot is recorded.

Update, 8 September local time: B06WJ-HW-01 now records its new CPU/LPC/HPET
ACPI construction marker, SeaBIOS and actual JetFlash entry 2 handoff. The
operator subsequently confirms the same top-level ACPI error. B06WJ has not
fixed the Windows failure; the subtype remains unknown. See the
[new run report](../research/msi/b06wj-jetflash-hw-2026-09-08.md).

Further 8 September update: fresh reference/MSI/Intel analysis and full
target RAM table captures identified omitted IOH `_PRT` routes and a reserved
bit in the CTBL SSDT resource descriptor. A network-loaded GRUB diagnostic
applied both corrections only in RAM, verified all eleven final structures,
and launched the JetFlash MBR. The log reaches the USB key invitation;
the operator subsequently reports A5 again. The exact subtype and kernel-side
selection of the replacement tables remain unobserved. No new ROM was built
or flashed. The [RAM lab report](../research/msi/b06wj-acpi-a5-runtime-lab-2026-09-08.md)
records the exact mutations, readbacks and remaining uncertainty.

Build update, 8 September: B06WK now embeds these descriptor/routing repairs
and adds legacy VGA producer windows plus the fixed-power-button FADT
description. It changes descriptions, not the inherited GPIO/IRQ hardware
sequence or RAM/QPI/USB/payload policy. Two clean builds are byte-identical;
515 host tests, 7 native C acpigen tests and independent image/table audits
pass. **B06WK has not been flashed or hardware-tested and does not establish
an A5 fix.** The [new release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
identifies the current 16-MiB artifact; earlier hardware evidence stays valid.

Later 8 September hardware update: the operator activated WK. Its new FADT
marker, SeaBIOS and Intenso menu selection are now captured in
[B06WK-HW-01](../research/msi/b06wk-intenso-hw-2026-09-08.md). SeaBIOS rejected
the received USB boot sector's signature and fell back to iPXE, before OS
entry. Verify the intended ISO and the Intenso's BIOS boot sector next; this
does not establish another ACPI failure or fix. No reset was needed.

## What the evidence supports

| Area | Demonstrated result | Remaining work |
|---|---|---|
| CPU, DDR3, QPI | Three-phase vendor-assisted initialization, DRAM execution, CBMEM, postcar and ramstage | MSI CSI/MINIT blobs remain required; full-memory and repeated-boot qualification remain open |
| PCIe and graphics | Active HD5450 and RTL8168 paths, allocated resources, physical Radeon VBIOS and display | Broader slot/topology support remains outside the fixed experiment |
| USB | GPIO57 release, EHCI/UHCI enumeration, operator-confirmed keyboard input, mass-storage reads and local loader execution | More ports/devices and OS handoff still need qualification; physical PS/2 input is not confirmed |
| SATA | AHCI port 5 link, CT1000MX500SSD1 detection/boot-menu registration, successful LBA 0/1 reads in a prior capture | Longer data-integrity tests and a SATA OS boot |
| Network | RTL8168 link, DHCP, TFTP and netboot.xyz chainload | Later TLS/HTTP failures; reliable large OS-image transfer is not established |
| ACPI and interrupts | B06WI hardware IRQ transaction/readback and FADT/MADT/MCFG construction passed; SeaBIOS and JetFlash handoff followed, then operator-observed Windows A5 | Exact bugcheck parameters/ACPI subtype, runtime table evaluation and OS interrupt delivery remain unknown |
| CPU topology and platform services | BSP-only execution, basic PIC/LAPIC/IOAPIC path, HPET decode, watchdog halt; B06WJ CP00/LPC/HPET table construction observed | Windows acceptance, AP/SMP startup, full ACPI devices/events/power methods and additional GPIO policy |
| Reset and UEFI | Firmware reentry after the Windows reset | Retained-state guard stops warm continuation in CAR; AC-restore power-on and EDK2 hardware boot remain unresolved |

RAM and QPI are treated as working assumptions for the next OS-facing work,
as requested by the operator. This prioritization does not turn the sparse
B06WG/B06WH memory checks into a full-memory stability result. The current
successful path still explicitly uses the user's local MSI initialization
modules; it is not an entirely native open DDR3/QPI implementation.

SATA is beyond controller enumeration: B06WH serial lines 880 onward show
port 5 link-up and the 931-GiB Crucial drive, and B06WG's local-USB capture
contains successful AHCI sector reads. Conversely, SeaBIOS's
`PS2 keyboard initialized` log alone does not establish physical PS/2 key
acceptance. USB input has the separate operator confirmation.

## Next measurable result

1. Activate B06WK and boot the operator-prepared Linux live USB with ACPI
   enabled. Record the new firmware/table marker, early kernel messages,
   actual selected ACPI tables, PCI resources, `/proc/interrupts` and
   `/proc/iomem`. Do not initially disable ACPI or APIC. B06WK groups four
   evidence-backed corrections; it is not an isolated-cause A/B test.
   Keep RAM/QPI and USB bring-up as the accepted working baseline.
2. Acquire the four bugcheck parameters through a suitable crash/debug
   capture if A5 persists. The current stop-code-only screen cannot identify
   which ACPI interface Windows rejected, or whether its subtype changed
   between WH and WI. A bootable Linux environment can also provide the
   actual runtime tables for comparison.
3. Reach the first usable OS session, then test sustained USB/SATA/network I/O
   and warm reset. AP startup and broader ACPI/power services follow as
   separate changes; EDK2 remains the independent UEFI payload path.

This is a substantial firmware-to-loader milestone. An OS boot and a
reproducible UEFI release remain different, unfinished milestones, so a single
percentage would obscure more than it explains.

## Evidence

- [B06WI-HW-01 JetFlash run](../research/msi/b06wi-jetflash-hw-2026-09-07.md)
  and its immutable raw capture/selection metadata.
- [B06WH-HW-02](bringup-log.md#2026-09-07--b06wh-reaches-graphical-windows-pe-then-stops-with-acpi_bios_error-0xa5)
  and [its serial capture](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
- [B06WG hardware analysis](../research/msi/b06wg-usb-boot-hw-2026-09-07.md)
  and [local USB/AHCI capture](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
- [Vendor live correlation](../research/msi/vendor-live-acpi-irq-gpio-2026-09-07.md).
- [B06WI build manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
  and [hardware test procedure](b06wi-vendor-irq-acpi.md).
