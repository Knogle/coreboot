# ASRock Rack B565D4-V1L: local source alpha

This branch is a first **downstream source snapshot**, based on coreboot
`94fab62214e6e125b52263788efdaf46c2236124` and captured on 2026-11-16.
It keeps the Cezanne, Matisse and Vermeer selectors, SeaBIOS/EDK2/CSM
integration, Board support code, 42 saved configurations, and the related
Coreboot and payload changes in one Git history. It is a starting point for
faster review and build work, not an upstream submission or a release ROM.

## Current evidence

- All 42 saved B565 configurations passed `make defconfig` in a fresh
  worktree. Each selected the B565 Board and ASRock Rack vendor without
  Kconfig warnings. This checks configuration, not full compilation.
- The earlier four-profile f358 images and their host checks belong to an
  older source snapshot. This branch has no freshly linked 32 MiB ROM.
- The older aa4 firmware has historical physical POST evidence. This source
  snapshot has not been flashed or physically qualified.
- Logical upstream patch splitting, contributor signoff, additional toolchain
  checks, and complete fresh-clone reproducibility remain open.
- Bundled EDK2, SeaBIOS and CSMWrap patch files retain historical line
  endings and whitespace; a whole-snapshot `git diff --check` flags those
  files. The ordinary Coreboot source and metadata scope passes that check.

## External firmware inputs

The Board uses original signed AMD and ASRock pre-x86 components, microcode,
and optional payload inputs. Their contents are **not** in this repository.
`3rdparty/blobs` is a Git submodule pointer; initializing it does not supply
the B565-specific private files. The local reference sizes and SHA-256 hashes
are in
[the external input manifest](Documentation/mainboard/asrockrack/b565d4_v1l_external_inputs.json).
Use only firmware acquired and verified for this exact Board. Place the
required files at the manifest paths in your own checkout, or change the
matching Kconfig paths to independently verified inputs. The manifest covers
the Board firmware files, not every external payload repository or toolchain.

To verify files already present in a checkout:

```sh
python3 -B util/b565d4/verify_external_inputs.py
```

The script reports missing or mismatched files and never copies or publishes
the binary inputs. Optional Vermeer and EDK2 input paths are included in the
manifest; unused optional files may be absent for a Cezanne/SeaBIOS build.

## First build path

The smallest saved configuration is Cezanne with SeaBIOS:

```sh
make obj=build/b565d4-smoke KBUILD_DEFCONFIG=configs/config.asrockrack_b565d4_v1l_seabios defconfig
make obj=build/b565d4-smoke -j4
```

The first command passed in this source snapshot. The second command is a
build entry point and has **not** completed here. Before running it, provide
the required private firmware files, Coreboot submodules, a compatible x86
cross-toolchain, `iasl`, payload source/toolchains, and sufficient disk space.
The local host did not meet the earlier full-build resource/toolchain gate.
The two saved `*_server_alpha` configurations carry more of the current
feature set and require additional payload sources.

A successful build would still need layout and input checks, a second clean
build for a reproducibility claim, and the established full-chip rollback and
Board regression procedure before any new firmware can be treated as
qualified. Do not infer flash readiness from this source snapshot.

The historical
[Board documentation](Documentation/mainboard/asrockrack/b565d4_v1l.md)
records earlier experimental results; its physical observations do not
qualify this branch.
