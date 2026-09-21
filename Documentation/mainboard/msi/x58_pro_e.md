# MSI X58 Pro-E (MS-7522)

This port brings coreboot to the MSI X58 Pro-E. It targets the exact hardware
described below. Keep an external programmer and a verified full-chip backup
close at hand.

## Hardware

| Component | Tested configuration |
|---|---|
| CPU | Intel Xeon E5649, CPUID `0x206c2`, stepping B1 |
| Chipset | Intel X58 IOH and ICH10R |
| Memory | One 4 GiB Crucial BLS4G3D1609DS1S00 at SPD `0x54` |
| Super I/O | Fintek F71882FG/F71883FG family at port `0x4e` |
| Flash | Socketed 16 MiB Winbond W25Q128.V |
| Graphics | Radeon HD 5450, `1002:68f9`, with legacy VGA ROM |
| Ethernet | Realtek RTL8111/8168/8411, `10ec:8168`, revision 02 |
| Storage | ICH10R in AHCI mode |
| Serial | COM1 at `0x3f8`, 115200 8N1 |
| Payload | SeaBIOS |

The memory used for bring-up is a dual-rank DDR3 UDIMM. The observed settings
were about DDR3-800 with 6-5-5-15 timings and a 1T command rate. Other CPUs,
DIMMs, graphics cards and add-in cards have not been qualified.

## Status

An earlier revision of this port reached these milestones on the same board:

- booted the Alpine Linux qualification system;
- booted Windows 11 once with the documented hardware;
- passed the earlier ACPI `_UID` failure in Windows XP text-mode setup; and
- initialized the ICH10R SATA controller in AHCI mode.

Windows XP setup later stopped with error `0x7b`. The likely cause is a missing
AHCI driver, but that has not been confirmed. The single Windows 11 boot is
useful evidence, not a claim of repeatable support. The current source still
needs a fresh hardware run.

The following areas are not supported:

- arbitrary or mixed DIMMs, XMP and configurations using several slots;
- S3, S4 and a complete SMM service path; and
- PXE or iPXE boot ROMs.

PS/2, the JMicron JMB363 controller, FireWire and other unlisted devices still
need focused testing.

## Required private firmware

CPU and cache-as-RAM initialization use native coreboot code. QPI and DDR3
initialization still need four ranges extracted from a local copy of the MSI
8.F update:

| Input | Size |
|---|---:|
| CSI wrapper | `0x837` |
| CSI helper | `0x25` |
| MINIT | `0x18700` |
| CSI | `0x75e0` |

These inputs form one board-specific interface. They are not a generic MRC or
FSP package, and files from different firmware versions must not be mixed.
Coreboot does not download or redistribute them. ACPI comes entirely from
tracked ASL and C source; no private DSDT or SSDT is needed.

## Building

Prepare the private inputs from your own local copy of the MSI 8.F update:

```sh
mkdir -p site-local/msi/x58_pro_e
python3 util/x58/scripts/prepare_x58_vendor_init.py \
  --vendor-source /path/to/7522v8F.zip \
  --output-dir site-local/msi/x58_pro_e/vendor-init
```

Configure coreboot and select `MSI` as the mainboard vendor and `X58 Pro-E` as
the mainboard model:

```sh
make menuconfig
make -j"$(nproc)"
```

Keep `General setup > Allow use of binary-only repository` enabled. The build
checks the size and SHA-256 digest of every extracted input before adding it to
the image.

## Flashing and recovery

`coreboot.rom` is a 4 MiB BIOS component, not a full 16 MiB flash image. In the
documented layout it starts at offset `0x00c00000` of the W25Q128. Treat the
lower 12 MiB as erased only after two full-chip reads produce matching files.
If they contain data, preserve it.

Do not flash until you have a tested external programmer and a verified
backup. To create a full image for the documented layout, start with 12 MiB of
erased bytes and append `coreboot.rom`. Write the resulting 16 MiB image only
after checking its size and layout. Building the image never powers, resets or
flashes the board.

## Implementation notes

The port uses the normal bootblock, romstage and ramstage flow. Standard ICH10
drivers handle the southbridge devices. Board code owns the X58 domain scan,
resource map, IOH routing, ACPI tables and the Fintek hardware monitor setup.
