> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WH: quieter B06WG-equivalent USB/SeaBIOS image

Status: **SOURCE-, BUILD- AND HARDWARE-TESTED THROUGH WINDOWS PE BUGCHECK**.

Update 2026-09-07: the later B06WH-HW-02 run reproduced display, USB HID/MSC
and graphical Windows PE, which stopped with `ACPI_BIOS_ERROR (0xA5)`.
The four parameters were not captured. See the appended hardware record in
[the bring-up log](bringup-log.md#2026-09-07--b06wh-reaches-graphical-windows-pe-then-stops-with-acpi_bios_error-0xa5).
The original design and build-time discussion below remain historical context.

B06WH is a default-off successor to B06WG. It deliberately reuses B06WG's
coreboot platform, RAM/QPI fast-postmem and automatic GPIO57 USB code without
changing a register operation, admission gate, delay, POST code or payload
feature. Its sole functional change is the SeaBIOS console threshold:

| Setting | B06WG | B06WH |
|---|---:|---:|
| `CONFIG_DEBUG_LEVEL` | 8 | 6 |
| `CONFIG_DEBUG_USB_TRACE` | enabled | enabled |

Build selection and identity metadata necessarily differ as well: B06WH has
its own Kconfig symbol, build ID, mainboard part number, local version,
payload-config filename and artifact names. Those differences make serial
captures and flashed images unambiguous; they do not introduce a second USB
implementation.

## Motivation and limits

The B06WG target result established one automatic path through coreboot,
SeaBIOS USB HID/MSC and RTL8168 iPXE. USB-keyboard input worked, and a local
USB Windows-PE loader performed sustained transfers before reaching an
uncaptured graphical BSOD. That result justifies preserving the platform and
USB behavior while reducing nonessential SeaBIOS output during subsequent
storage and loader experiments.

SeaBIOS debug level 6 prints messages at levels 0 through 6 and suppresses
levels 7 and above. The B06WG local-USB capture contained 41,549
`ehci_send_pipe` lines emitted at level 7; B06WH suppresses that measured
sustained-transfer flood. The deferred USB journal remains compiled, is
strictly bounded to 96 events and is dumped with `dprintf(1)`, so the useful
setup evidence remains available. B06WH is therefore a materially quieter
image, but a reduction in serial traffic alone must not be described as a
throughput or BSOD fix.

B06WH also does not claim to fix the observed Windows-PE bugcheck. The failure
could be caused by RAM integrity, DMA/addressing, ACPI, interrupt routing, USB
storage, the external loader or another platform defect. Lowering a debug
threshold is a controlled diagnostic variable only.

## Preserved functional contract

B06WH compiles the same `b06wg_usb_auto.c` implementation and includes the
same B06WG fast-postmem path. It preserves all of the following:

- the recorded-result and UC-MTRR admission gates;
- the 14-address transactional CBMEM/object alias smoke;
- one zeroing pass and nine fixed sparse reads per admitted RAM window;
- the exact late platform, GPIO, EHCI and UHCI preflight;
- GPIO57 output-low, a calibrated 65.536-ms delay, then output-high;
- bounded USB over-current sampling through 500 ms;
- the read-only pre-payload GPIO57/controller/OCA gate;
- POST codes `98` through `9f` and fail-closed payload blocking;
- SeaBIOS UHCI, EHCI, hub, mass-storage and HID-keyboard support;
- the physical HD 5450 VBIOS path and RTL8168 iPXE option ROM;
- 1000-ms USB attach and PS/2 keyboard spin-up CBFS settings;
- COM1 at 115200 8N1 and the inherited recovery path.

The detailed write, rollback and failure contract remains the one documented
for [B06WG](b06wg-automatic-usb-seabios.md). B06WH does not add a generic
writer, command parser, USB-controller write or new proprietary component.

The inherited fast-postmem log text still contains the implementation-family
label `B06WG`. Firmware identity is instead provided by the distinct
bootblock/romstage/ramstage ID
`X58PROE-B06WH-AUTO-USB-SEABIOS-20260907` and the USB-stage ID
`B06WH-AUTO-GPIO57-USB1`.

## Payload configuration

The B06WH-specific SeaBIOS configuration retains:

```text
CONFIG_USB=y
CONFIG_USB_UHCI=y
CONFIG_USB_EHCI=y
CONFIG_USB_MSC=y
CONFIG_USB_HUB=y
CONFIG_USB_KEYBOARD=y
CONFIG_KEYBOARD=y
CONFIG_PS2PORT=y
CONFIG_DEBUG_USB_TRACE=y
CONFIG_DEBUG_LEVEL=6
# CONFIG_THREADS is not set
```

The source-contract test compares the complete B06WG and B06WH SeaBIOS
configurations and permits only the `CONFIG_DEBUG_LEVEL` value to differ. It
also verifies that both variants select the same platform and USB sources.

## Build and artifacts

Build with:

```bash
./scripts/build_x58_b06wh.sh
```

The public, redistributable coreboot base is:

```text
builds/experimental/msi-x58-pro-e-b06wh-coreboot-base-4MiB.rom
SHA-256 7b079fe0a962585e85d12e4d72291fb6cc4c18c6e08f845112b3d01ca5f49513
```

The image intended for the socketed `W25Q128.V..M` is the ignored local
16-MiB artifact:

```text
blobs-local/msi-x58-pro-e/b06wh/
  msi-x58-pro-e-b06wh-deterministic-w25q128-16MiB.rom
SHA-256 b5f894d4a2fc1552c141d656ef4564cc4be5d4f7ae5ca75de638bb4315024c6b
```

The local image contains bytes derived from the user's hash-pinned MSI input
and must not be redistributed. The public base contains no inserted MSI
CSI/MINIT bytes and no AMD VGA ROM.

## First hardware test

Use the same fixed E5645, sole-SPD-`0x54`, HD 5450, RTL8168 and DIMM setup as
B06WG. Attach the known USB keyboard and test medium before power-on and arm a
raw COM1 capture at 115200 8N1. Retain the socketed known-good flash chip and
the established one-second AC recovery method.

The first run should verify, in order:

1. the B06WH identity in early and ramstage output;
2. the unchanged fast-postmem, GPIO57 and OCA gates through POST `9d`;
3. SeaBIOS VGA, EHCI/UHCI, keyboard and mass-storage discovery;
4. accepted keyboard input in the boot menu or iPXE;
5. a local USB read test while recording elapsed time and serial byte count;
6. if Windows PE is retried, a photograph or video of any STOP code before
   automatic reset.

Compare a B06WH capture with the immutable B06WG hardware record. A successful
payload entry would show nonregression, not establish RAM stability or explain
the previous bugcheck. The repository validation policy still requires ten
cold boots and separate memory testing before this path is considered stable.

Exact build hashes and CBFS inventory are recorded in the
[B06WH manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
