# B06VM iPXE option ROM

This directory is the local, source-controlled configuration for the MSI X58
Pro-E onboard RTL8168 (`10ec:8168`).  It targets the iPXE revision pinned by
this coreboot tree:

```text
7c39c04a537ce29dccc6f2bae9749d1d371429c1
```

The profile retains iPXE's normal legacy-ROM defaults and adds HTTPS plus the
image verification/trust commands.  It has no EFI image, embedded script,
deployment-specific trust key, or direct iPXE serial output; SeaBIOS provides
the serial terminal through SERCON, avoiding duplicate output.  This named
profile exercises the same feature code
as coreboot's built-in iPXE wrapper with
`CONFIG_IPXE_HAS_HTTPS=y`, `CONFIG_IPXE_TRUST_CMD=y`, and its serial console
disabled.  The two build routes intentionally carry separate generated build
IDs, so their complete ROM hashes are not expected to match.

## Reproducible local build

The iPXE source must already exist locally at the exact pinned commit.  Copy
these headers as a named configuration, clear ambient `DEBUG`, disable ccache,
and use the coreboot cross compiler:

```sh
ipxe_src=coreboot/payloads/external/iPXE/ipxe/src
mkdir -p "$ipxe_src/config/local/b06vm"
cp configs/ipxe-b06vm/general.h "$ipxe_src/config/local/b06vm/general.h"
cp configs/ipxe-b06vm/console.h "$ipxe_src/config/local/b06vm/console.h"
test "$(git -C "${ipxe_src%/src}" rev-parse HEAD)" = \
  7c39c04a537ce29dccc6f2bae9749d1d371429c1
env -u DEBUG CCACHE_DISABLE=1 SOURCE_DATE_EPOCH=1788602400 \
  make -C "$ipxe_src" CONFIG=b06vm \
  CROSS="$PWD/coreboot/util/crossgcc/xgcc/bin/i386-elf-" \
  bin/10ec8168.rom
sha256sum "$ipxe_src/bin/10ec8168.rom"
```

Run the build twice from `make ... veryclean` and require byte-identical ROMs.
The resulting file belongs in CBFS as raw file `pci10ec,8168.rom`; SeaBIOS then
associates it with that PCI function and registers its BEV boot entry.
SeaBIOS `CONFIG_BOOT=y` plus `CONFIG_OPTIONROMS=y` compile that BEV path;
`CONFIG_DRIVES` is not an iPXE prerequisite and is enabled in B06VM to compile
local INT 13h ATA/AHCI/USB-media support.  B06VM currently requires the two
SATA functions to remain in observed legacy-IDE class `0101`; it does not
switch the controller to AHCI.

The audited build at the pinned revision, with the command above, produced two
byte-identical 95,232-byte (`186 * 512`) ROMs:

```text
SHA256 ad94f78e2782df2ea804727cee90a6d331843594945a4eef364bfe0a6621650b
```

The ROM actually embedded by `scripts/build_x58_b06vm.sh` is produced through
coreboot's wrapper and has this pinned identity:

```text
SHA256 8b21a133a1a795bcd19f7945c23306d37b348fedc8299527e3ce3d66c906ee80
```

Both are 95,232 bytes and contain the same selected feature code.  A bytewise
comparison differs at exactly five bytes: the wrapper's four-byte generated
build ID and the corresponding one-byte ROM-header checksum.  The wrapper hash
above, not the named-profile hash, is the release gate for B06VM.

The final linker map contains the HTTPS object and the `imgtrust`/`imgverify`
commands.  The ROM header identifies PCI `10ec:8168` and marks the image
bootable.  The linker emits a warning about a missing `.note.GNU-stack` on
`eap_md5.o`; the build otherwise succeeds.  This measurement used the
coreboot i386-elf GCC 15.2.0/binutils 2.46.1 toolchain.  A different toolchain
may produce a different hash or size.

`IMAGE_TRUST_CMD` provides verification commands but does not pin the
deployment's own key.  The default build trusts the iPXE root CA; a production
provisioning flow should pass an explicitly controlled `TRUST=` certificate
and record the resulting ROM hash.  Never treat plain HTTP as authenticated.

For coreboot's built-in iPXE wrapper, stage these two headers instead as
`src/config/local/general.h` and `src/config/local/console.h`, and select:

```text
CONFIG_PXE=y
CONFIG_BUILD_IPXE=y
CONFIG_PXE_ROM_ID="10ec,8168"
CONFIG_IPXE_STABLE=y
# CONFIG_IPXE_MASTER is not set
# CONFIG_IPXE_SERIAL_CONSOLE is not set
# CONFIG_IPXE_NO_PROMPT is not set
# CONFIG_IPXE_ADD_SCRIPT is not set
CONFIG_IPXE_HAS_HTTPS=y
CONFIG_IPXE_TRUST_CMD=y
# CONFIG_IPXE_BUILD_EFI is not set
```

The wrapper currently clones when its source directory is absent, so an
offline build requires prefetching the pinned commit.  It also inherits an
ambient `DEBUG`; for example `DEBUG=release` is parsed as an object name and
breaks the build.  Invoke the enclosing build with `env -u DEBUG` (or
`DEBUG=`).  A failed wrapper build can leave its three tracked config headers
modified because restoration only happens after success; use a disposable
worktree/clone or verify it is clean before and after the build.

## Option-ROM-space gate

Flash capacity is not the limiting resource.  Legacy VGA and NIC ROMs must
also coexist in the C0000-EFFFF shadow window.  The B06VM reference VGA ROM
has a measured PCIR legacy-image length of 56,320 bytes.  The B06VM SeaBIOS
profile builds to 93,216 bytes raw (93,452-byte ELF), and init relocation puts
`final_readonly_start` at `0xf0000`, exposing the full 196,608-byte window.

Both the ROM header and PCIR structure declare those same 56,320 bytes, but
their byte sum is `0xff`, not the required zero.  SeaBIOS defaults
`etc/optionroms-checksum` to one and would therefore reject this exact card ROM
before calling it.  The B06VM-only board build hook adds the 64-bit CBFS integer
`etc/optionroms-checksum=0`.  This necessarily relaxes validation for all
option ROMs during that SeaBIOS run, so keep it scoped to the fixed-card,
default-off experiment and verify that the final image contains the zero-valued
file.

SeaBIOS otherwise attempts physical ROM-BAR sizing for every visible normal
PCI header, including functions outside coreboot's B06VM devicetree.  The same
B06VM-only hook therefore adds `etc/pci-optionrom-exec=1`: physical PCI option
ROM mapping is permitted only for VGA.  The matching CBFS
`pci10ec,8168.rom` is loaded before that policy check and remains available as
the RTL8168 BEV.  The release build requires the policy file to be exactly an
eight-byte little-endian value of one.

SeaBIOS rounds each ROM end to 2 KiB.  The VGA image therefore consumes 57,344
bytes and this local HTTPS+trust iPXE consumes 96,256 bytes.  Their aligned
total is 153,600 bytes, leaving 43,008 bytes.  Retain
`CONFIG_RELOCATE_INIT=y` and `CONFIG_MALLOC_UPPERMEMORY=n`; the final linked
boundary is the authoritative gate.  Also require SeaBIOS to log both copied
ROM sizes and no option-ROM allocation failure on hardware.

A true 128-KiB legacy VGA image would still make HTTPS+trust iPXE impossible
even in the theoretical full 192-KiB window.  The total file size of a hybrid
VGA ROM is not proof of its legacy x86 image size; use the PCIR image length
and record SeaBIOS's reported copied size.

No B06VM hardware run has yet executed the VGA ROM, established an RTL8168
link, obtained DHCP, entered iPXE, transferred an image, verified a signed
image, or booted any payload.  These are compiled first-run capabilities, not
runtime results.
