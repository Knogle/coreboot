# Experimental SeaBIOS CSM application

The [V26 review scope](REVIEW_SCOPE.md) separates the wrapper and SeaBIOS
changes, EDK2 integration, and B565-specific policy for independent review.

The B565D4-V1L `EDK2_CSMWRAP` option builds pinned open CSMWrap sources and
their SeaBIOS submodule, embeds the resulting EFI application in the EDK2
firmware volume, and adds a selectable legacy boot entry. Ordinary UEFI boot
remains the default. The application runs SeaBIOS's genuine `Csm16.bin` through
the Compatibility16 interface; it does not provide the complete historical
EDK2 `EFI_LEGACY_BIOS_PROTOCOL` service.

The option defaults on for the board's qualified `B565D4_LARGE_PAYLOAD` EDK2
configuration. The qualified CPU-specific EDK2 profiles use the normal coreboot
version path and standard BIOS vendor and board names. Historical
`config.asrockrack_b565d4_v1l_edk2_csm` retains its earlier explicit alpha suffix.

Ordinary real-mode BIOS calls use the calling processor's SeaBIOS thunk.
Protected/V86 callers, including JEMM386, temporarily use a firmware helper.
The ABI2 path keeps every configured processor in MADT and MP tables and
leaves the helper's APIC enabled. A multiprocessor OS can reclaim that CPU
through its normal INIT/SIPI startup; no logical CPU is permanently reserved.
A bounded liveness check rejects new proxy requests after the helper is lost.
OS AP startup must not race an active BIOS operation: this is not a concurrent
OS/firmware service or a general V86 BIOS provider after SMP takeover.
Legacy entry refuses to run with Secure Boot enabled.

The CSM image reserves an 8-KiB FSEG allocation budget within its 128-KiB BIOS
image for BIOS drives and related state. PrepareToBoot initializes the SeaBIOS
serial console, including BIOS keyboard input, and avoids hooking itself when
called again. These paths retain the board's ordinary CMOS serial-console
policy.

The ordinary coreboot build has physically booted native UEFI and embedded
legacy Proxmox with all twelve5600X threads, including a full AC cold start.
The former helper CPU also executes pinned host computation and a KVM vCPU
under verified load. Real-mode and JEMM386 V86 native SATA reads, RTC, ticks,
RAM-disk FAT, XMS and BIOS serial keyboard input pass. These are scoped ALPHA
results; production qualification remains open. See the architecture and evidence report (`../../../porting/asrock_am4/CSM_NO_RESERVED_THREAD_20261031.md`; external historical evidence).

When the firmware console owns its debug UART, SeaBIOS omits that port from
the BIOS COM list through its existing `etc/advertise-serial-debug-port`
policy. This prevents DOS from changing the console to2400 baud during COM
initialization. ACPI OS UART discovery remains available, and disabling the
firmware serial console leaves ordinary BIOS COM discovery enabled. Direct
UART diagnostic programs and buffered BIOS screen output may interleave;
raw captures retain that distinction. Physical application comparison has
confirmed the baud-rate correction; qualification of the newly integrated
image remains separate.


Board patches move the low stub above coreboot's retained 0x30000–0x3ffff
allocation, preserve reserved low-memory holes in E820, and select AST2500
UART2 at 0x2f8 for wrapper and BIOS console output. The application temporarily
disables IOMMUs for BIOS services; a legacy OS must reinitialize them from IVRS.
These settings are specific to the qualified AM4 memory contract.

The embedded SeaBIOS build honors coreboot's `SOURCE_DATE_EPOCH` for its dirty
build version. It retains the pinned source revision and patch status, using a
fixed host label instead of the builder's current clock and hostname. Standalone
builds without that environment variable retain upstream version behavior.

Source provenance and licenses remain those of
[CSMWrap](https://github.com/CSMWrap/CSMWrap) and its pinned submodules.
No source from the supplied proprietary AMI reference archive is incorporated.
The Python `tools/xxd` implements only the `xxd -i` conversion required by the
upstream build. Host requirements include gcc/binutils, make, nasm, Python and
git; the build initializes the source submodules over the network.
