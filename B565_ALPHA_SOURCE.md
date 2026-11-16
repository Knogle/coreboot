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
- Code commit `05c72134d202392e3683ae13d0fab3c467795f90` adds one
  backslash to the SeaBIOS `PAYLOAD_CONFIGFILE` default. Two isolated
  checkout paths produced byte-identical Cezanne and Vermeer SeaBIOS
  configurations; Make resolved each path and read the same Board SeaBIOS
  config file. The Vermeer/EDK2 configuration was unchanged. This establishes
  configuration and path consumption only; this revision has no full ROM build.
- One network-isolated Cezanne/SeaBIOS build from ancestor source commit
  `3c1424d49d` completed before the path fix and produced a 32 MiB ROM,
  SHA-256 `342fcc5b5c46d394a698d6a17afa9ec4bbf730b64b0d67e17258fd1b6ca13538`.
  Its image report confirms the source-stage and template-preservation checks.
  The earlier four-profile f358 images and their host checks belong to an
  older source snapshot; the other current profiles have not been rebuilt.
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
git submodule update --init 3rdparty/vboot
make obj=build/b565d4-smoke KBUILD_DEFCONFIG=configs/config.asrockrack_b565d4_v1l_seabios defconfig
make UPDATED_SUBMODULES=1 obj=build/b565d4-smoke -j2
```

`3rdparty/vboot` supplies `vb2_sha.h` and the SHA source even in this Vboot-off
configuration. The B565-specific firmware inputs in the manifest are separate
from submodules; provide the three required files before building. The payload
Makefile fetches SeaBIOS unless its pinned Git source is already available.
A compatible x86 cross-toolchain, `iasl` and sufficient disk space are also
required. An isolated offline run with local SeaBIOS, Vboot and verified
firmware inputs completed the second command at ancestor source commit
`3c1424d49d`. That ROM predates the SeaBIOS path fix and does not verify this
revision. It is one host build, not an independent clean-build reproduction.
The two saved `*_server_alpha` configurations carry more of the current
feature set and require additional payload sources.

The successful host build still needs an independent clean rebuild for a
reproducibility claim, and the established full-chip rollback and
Board regression procedure before any new firmware can be treated as
qualified. Do not infer flash readiness from this source snapshot.

The historical `porting/asrock_am4` evidence archive and operator-local
`blobs-local`/`.site-local` paths referenced in the Board documentation are
not included in this source repository. Those records must accompany any
future physical-qualification claim.

The historical
[Board documentation](Documentation/mainboard/asrockrack/b565d4_v1l.md)
records earlier experimental results; its physical observations do not
qualify this branch.
