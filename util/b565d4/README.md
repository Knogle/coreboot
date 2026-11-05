# Rebuilding the B565D4-V1L Alpha125/126 source cohort

This branch publishes the tested source111 board implementation and ordinary
Vermeer profiles: **Alpha125-CSM / EDK2** and **Alpha126 / SeaBIOS**. The newer
private32-thread experiments are not included. Both profiles use the normal
coreboot make workflow; the helper below prepares inputs and records output
hashes without accessing hardware.

## Inputs and reproducibility boundary

`sources.lock.json` pins the public payload/library commits and every private
firmware input by SHA-256 and length. The retained pre-x86 template, AMD
microcode/ABL/Promontory firmware and Intel UNDI driver are supplied separately.
The qualified template is an exact input, not an arbitrary vendor update image.
These private binary files and device NVRAM are not published in this branch.
The branch is therefore source-reproducible **with the matching private input
package**, not a blob-free firmware distribution.

`sdk.lock.json` identifies the preserved relocatable compiler/helper SDK. Its
`tools/` directory contains coreboot crossgcc15.2.0/binutils2.46.1/iasl20251212,
the staged EDK2 image converter and the iPXE assembler/development libraries.
The qualified host is Debian12, clang19.1.7, GCC12.2, GNU make4.3 and Python3.11.
System compiler/runtime libraries remain host dependencies. This is not a
hermetic container or a cross-OS reproducibility claim. The SDK is a separate
artifact rather than a checked-in compiler binary tree.

Normal system tools plus `make crossgcc-i386 CPUS=4` can be used for development.
For comparison with the recorded recipe, supply the matching SDK with `--tools`;
the helper verifies every SDK input before building. The helper fixes
`SOURCE_DATE_EPOCH` for payload reproducibility while retaining normal coreboot
Git version/date generation (`BUILD_TIMELESS=0`). A different source commit
legitimately changes embedded Git metadata.

## Fresh clone and build

```sh
git clone --single-branch --branch am4-b565d4-v1l-alpha125-20261105 \
  git@github.com:Knogle/coreboot.git coreboot-am4
cd coreboot-am4

python3 util/b565d4/reproduce.py prepare
python3 util/b565d4/reproduce.py import-inputs ../b565d4-private-inputs.tar.gz
python3 util/b565d4/reproduce.py build --payload all --tools /path/to/sdk/tools -j 4
```

The preparation command checks out the exact commits in45 separate dependency
repositories. It preserves unexpected populated directories/revisions instead
of replacing them. An existing offline source package may be supplied with
`prepare --offline-cache /path/to/source-package`; its repository packs and
shallow boundaries are hash-checked before restoration. No previously compiled
payload or coreboot object is used by that restoration.

The input importer validates the complete archive, exact membership, paths,
lengths and hashes before installing files. It refuses links, corrupted inputs
and conflicting existing files. To export the package from the original
qualified source tree:

```sh
python3 util/b565d4/reproduce.py pack-inputs \
  --source-root /path/to/qualified-source111 \
  --output ../b565d4-private-inputs.tar.gz
```

Keep that package separate from public Git history. Its contents retain their
existing provenance and licenses; the packaging operation grants no new
redistribution rights.

The build helper executes `defconfig` and ordinary `make -j4` sequentially for
SeaBIOS and EDK2, then independently checks firmware-volume/payload layout and linked
low-memory ELF/rmodule/CBFS contracts.
It sets `GIT_ALLOW_PROTOCOL=none` during compilation, so missing source inputs
fail rather than being fetched implicitly. This disables Git fetching, not all
kernel network access. Output and compiler logs are under:

* `build/b565d4-seabios/coreboot.rom`
* `build/b565d4-edk2/coreboot.rom`
* `build/b565d4-results.json`

Each image is a complete32-MiB ROM. No image is selected for hardware by this
helper. Use a fresh checkout for another clean build; existing outputs are
preserved. Compare the resulting hashes from two fresh checkouts at the same
absolute source path, with the same SDK and host dependencies. Embedded
configuration paths can change SeaBIOS whole-ROM hashes across source roots;
equal executable content alone is a narrower comparison than byte identity.

## Ordinary make commands

The helper uses the unmodified normal configuration seeds. The equivalent
configuration/build invocation, after preparing sources and binary inputs, is:

```sh
export SOURCE_DATE_EPOCH=1793531444
export GIT_ALLOW_PROTOCOL=none
export CPATH="$AM4_BUILD_TOOLS/ipxe-host-tools/usr/include"
export LIBRARY_PATH="$AM4_BUILD_TOOLS/ipxe-host-tools/usr/lib/x86_64-linux-gnu"
export PATH="$AM4_BUILD_TOOLS/edk2-host-tools/bin:$AM4_BUILD_TOOLS/ipxe-host-tools/usr/bin:$PATH"

make obj=build/b565d4-edk2 DOTCONFIG=.config.b565d4-edk2 \
  XGCCPATH="$AM4_BUILD_TOOLS/crossgcc-own/bin/" BUILD_TIMELESS=0 \
  PYTHON=python3 UPDATED_SUBMODULES=1 HOSTCC=clang-19 HOSTCXX=clang++-19 \
  KBUILD_DEFCONFIG="$PWD/configs/config.asrockrack_b565d4_v1l_edk2_vermeer" defconfig
make obj=build/b565d4-edk2 DOTCONFIG=.config.b565d4-edk2 \
  XGCCPATH="$AM4_BUILD_TOOLS/crossgcc-own/bin/" BUILD_TIMELESS=0 \
  PYTHON=python3 UPDATED_SUBMODULES=1 HOSTCC=clang-19 HOSTCXX=clang++-19 -j4
```

Use `config.asrockrack_b565d4_v1l_seabios_vermeer` and separate output/config
names for SeaBIOS. See [the board documentation](../../Documentation/mainboard/asrockrack/b565d4_v1l.md)
for architecture, tested functionality and remaining limits.
