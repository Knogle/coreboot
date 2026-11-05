# B565D4-V1L bring-up status

This repository board stub is not the fully integrated Native148 build source.
The qualified local build uses the frozen [Source85 overlay and parent references][source85],
the genuine [Stage71 build][stage71], and the [HOST archive and container recipe][host148].
The archive records local dependencies; it is not a portable source distribution.

Native148 has executed the genuine DRAM-first bootblock, romstage and ramstage,
then stock SeaBIOS, Transcend legacy-MBR/GRUB and Linux with twelve CPUs and the
installed 16-GB DIMM. Native AST HICR5 bit5 initialization also enabled an IRQ3
UART receive/ACK exchange without a runtime register fix. The [hardware milestone][milestone148]
preserves the wire anomaly from that exchange; clean UART operation and cold-boot
repeatability remain open. See [CURRENT_STATE][current] for later observations.

Native148 retains the vendor PSP/ABL/APCB pre-x86 initialization and required
binary providers. It is not blob-free, an upstream-ready board port, or a standard
standalone `coreboot.rom` suitable for flashing. The sections below describe the
historical console-only stub and experiments, not the complete Native148 result.

[source85]: ../../../../blobs-local/asrock-am4/resume-20261013/Native148-Source85-Stage71-fullHOST-delta-01/source85/source85-selected-freeze.json
[stage71]: ../../../../blobs-local/asrock-am4/resume-20261013/Native148-Source85-Stage71-fullHOST-delta-01/stage71/controls/Root-stage71-HOST-result.json
[host148]: ../../../../blobs-local/asrock-am4/resume-20261013/Native148-Source85-Stage71-fullHOST-delta-01/index.json
[milestone148]: ../../../../blobs-local/asrock-am4/resume-20261013/native148-native12cpu-SeaBIOS-USB-Linux-IRQ3-milestone-01/README.txt
[current]: ../../../../porting/CURRENT_STATE.md

## Historical console-only experiment

The normal device tree now declares AST LDN3 at 0x2f8/IRQ3 and disables
the two Nuvoton UART devices. `B565D4_NORMAL_UART2` selects the bounded
initializer and UART2 console defaults together. This host-qualified path
remains opt-in; it does not supply memory training or a complete board port.
Current target observations use AST UART2 at 115200 8N1. See
`porting/CURRENT_STATE.md` for the installed image and fresh boot evidence.

The following C4, C5 and C7 descriptions are historical experiments.

With `B565D4_EARLY_RAW_CONSOLE=n`, the C4 candidate initializes ASPEED UART1 through configuration ports 0x4e/0x4f,
disables UART A/B on recognized Nuvoton families, and applies the exact vendor
SCU pin masks after a valid AHB revision read. It repeats initialization and
output to accommodate late ASPEED configuration. The physical ASPEED variant
and UART signal path remain unconfirmed until fresh target output is captured.
For the installed image and hardware observations, read porting/CURRENT_STATE.md;
source and build artifacts alone do not establish either.

The C5 configuration selects `B565D4_EARLY_RAW_CONSOLE=y`.
This wraps the first C entry after the unchanged strict guard and assembly
runtime, before ECAM/cache/S3/FCH/AOAC setup. It configures LPC through CF8/CFC,
enables ACPIMMIO through PM I/O, applies the vendor LPC-clock prerequisites,
and prints bounded raw UART diagnostics in a retry loop. Only the four
vendor clock/CLKRUN/SERIRQ pins are changed; other LPC pins are reported.
It does not enter a further stage or run the normal bootblock C initialization.

The C7 configuration additionally selects `B565D4_PRE_MSR_RAW_CONSOLE=y`.
After the same strict BIST/CPU guard, it uses a local byte-oriented DATA/BSS
initializer and stack while keeping caches disabled. It avoids the common
pre-C MSR sequence, checks initialized DATA and an odd-length BSS sentinel,
and applies all eleven qualified vendor LPC mux0 writes for the eMMC/SD-off
path, excluding SPI_CS3. It retries known Nuvoton UART deactivation and reports
additional ASPEED clock/reset/routing registers without changing them.
Its distinct marker begins `B565D4-V1L coreboot pre-MSR raw probe C7 CPUID=`.

C4, C5 and C7 cold starts received no readable serial output. C7's completed
480-second capture received only one AC-edge NUL. These observations do not
locate the stop or establish coreboot execution. The separately prepared
Vendor-PeiUARTbaselineV3 preserves the original vendor reset/PEI path and
adds its own bounded UART marker; it is a vendor counterprobe. Follow its
current installation/capture status in `porting/CURRENT_STATE.md`.

This is a deliberately incomplete Cezanne bootblock target. It does
not implement a usable coreboot board port, ramstage, PCIe, ACPI or payload boot.
Do not flash a generated coreboot.rom. Only a validated bootblock ELF, repacked
into the captured vendor BIOS reset image, is a candidate for later testing.

The existing Cezanne PSP, ABL, APCB and microcode remain in the vendor image.
Its reset image ends at 0x0a000000 and enters x86 at 0x09fffff0. The early stack
is within the vendor reset-image RAM window. A reset-entry assembly guard checks BIST and AuthenticAMD/Cezanne CPUID before
the common AMD pre-C code touches any MSRs. Other CPUs halt with POST E2.
The bootblock then enables the standard Cezanne FCH path and configures
ASPEED UART1 at 0x3f8/115200. It prints through coreboot's console and remains
in bootblock. The normal FCH path currently includes unbounded AOAC waits;
C5 isolates these dependencies because the completed C4 cold start produced
no readable output. That observation does not prove where C4 stopped.

The standard AMD memory layout is retained, with the stack at 0x09c00000 and
bootblock ending at 0x0a000000. The vendor APOB destination 0x0a200000 is
recorded, but this experiment stops before consuming it. Future-stage memory
reservations are placeholders, not a validated FSP or ramstage integration.

See porting/asrock_am4 for backups, electrical timing rules and test evidence.
