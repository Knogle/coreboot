> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Bring-up log

Logs are append-only.  Failed experiments remain part of the record.

## 2026-08-03 — static firmware-analysis baseline

```text
build commit: repository is not yet initialized as a Git worktree
ROM hash: MSI ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8
ROM hash: Intel 4e6b895898db07e78d8627ae6fb9c81df8df9ad950f24c1617ad4bb7ac5ce52c
flash chip: unknown; no physical inspection
board revision: unknown; no physical inspection
CPU: unknown
CPU stepping: unknown
microcode revision: not measured on hardware
DIMM model: unknown
DIMM slot: unknown
GPU: unknown
PSU: unknown
boot type: none; static analysis only
POST trace: none
serial log: none
result: MSI/Intel early-init candidates mapped and implementation plan written
recovery required: no hardware action performed
notes: no coreboot image built, flashed, or hardware-tested
```

Key static observations:

- MSI bootblock invokes CSI wrapper then MINIT wrapper.
- MSI Setup identifies the F71882F Super I/O family.
- Intel's large memory/Uncore candidate is a PEI-dependent platform module.
- No technically standalone MRC/FSP was identified, and no redistribution
  permission for an extracted initialization module was established.

## 2026-08-03 — full-ROM provenance clarification and early-boot disassembly

```text
build commit: no coreboot build
ROM hash: MSI ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8
flash chip: exact part still unknown
board revision: still unknown
boot type: none; no experimental firmware boot
POST trace: none
serial log: none
result: user reports the 4 MiB MSI file is byte-identical to the complete EEPROM
recovery required: no hardware action performed by this analysis
notes: image is now classified as the full descriptorless EEPROM image, not an update slice
```

Additional verified-static observations:

- reset vector is at raw `0x3ffff0`;
- no Intel Flash Descriptor or identified ME/GbE region is present;
- MSI CAR is 64 KiB at `0xfff80000` and matches coreboot's Intel non-evict
  mechanism structurally;
- extracted `RUN_CSEG` uses Fintek configuration port `0x4e`;
- no coreboot image has been built, flashed, or hardware-tested.

## 2026-08-04 — multi-firmware corpus and Uncore capture tooling

```text
build commit: repository is not initialized as a usable Git worktree
ROM hashes: eleven releases / twenty-two archive and payload hashes in research/firmware-corpus.json
flash chip: unknown; no physical inspection
board revision: unknown; no physical inspection
CPU: no target CPU accessed
CPU stepping: unknown
microcode revision: not measured on target hardware
DIMM model: unknown
DIMM slot: unknown
GPU: unknown
PSU: unknown
boot type: none; static analysis and synthetic host-tool tests only
POST trace: none
serial log: none
result: Intel/Apple/ASUS comparison corpus mapped; read-only Uncore snapshot/diff tools implemented
recovery required: no target-hardware action performed
notes: no coreboot image built, flashed, or hardware-tested
```

New verified-static observations:

- `SO0920P.bio` is DX58SO2/DX58OG family `SOX5820J`, not original DX58SO;
- true DX58SO 5600 (`SOX5810J`) was added as a private comparison input;
- Intel GUID `63C0690C...` is named `UnCoreInitPlatform` by S55xx UI/PDB
  metadata and occurs across both Intel desktop and server lines;
- MSI `MINITDLL` shares exact 888-byte regions with both S55xx endpoints and
  890-byte regions with both Apple Uncore PEIMs;
- ASUS 0701→0702 changes a private `MINITDLL` branch aligned with the vendor's
  “Memory Recheck” release, while 0702 and 0903 init DLLs are byte-identical;
- all locally present manifest artifacts passed size/SHA-256 verification;
- sixteen host-side unit tests passed; the Uncore tool was not run on X58
  hardware.

## 2026-08-04 — Uncore code/register correlation and negative host probe

```text
build commit: repository is not initialized as a usable Git worktree
ROM hash: MSI ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8
MINITDLL hash: 217d325ef82c4af1da4a95c158c3109b1de0f7975ce0b180e562f00c51023189
flash chip: unknown; no physical inspection
board revision: unknown; no physical inspection
CPU: analysis host AMD Ryzen 9 5950X; no target CPU accessed
CPU stepping: not applicable to target
microcode revision: not measured on target hardware
DIMM model: not measured on target hardware
DIMM slot: not measured on target hardware
GPU: not applicable
PSU: not applicable
boot type: none; read-only host probe and static analysis only
POST trace: none
serial log: none
result: no X58 Uncore exposed on host; MSI MMCONFIG accesses correlated with Intel register maps
recovery required: no hardware action performed
notes: dump_nehalem_uncore.py --require-target returned status 2 with no profiles/devices
```

New verified-static observations:

- `MINITDLL` programs CPU `SAD_PCIEXBAR` through CF8/CFC, then accesses the
  Uncore through six small MMCONFIG helpers;
- 450 helper call sites were recovered, including 118 with literal recognized
  Uncore BDFs and 85 with published or explicitly neutral observed names;
- the programmed `MC_CHANNEL_MAPPER` value `0x24489` is the documented
  identity mapping for all three read/write channels;
- the DIMM reset/CKE sequence matches published `MC_RESET_CONTROL`,
  `MC_CHANNEL_DIMM_RESET_CMD`, and `MC_CHANNEL_DIMM_INIT_CMD` fields;
- ten inspected loops poll unpublished `03.4:0xf8` bit 30 without a visible
  timeout; future native code must use a bounded timeout and diagnostic;
- Intel document 321322 and X58 document 320838 were hash-pinned as the CPU
  Uncore and IOH/QPI register-layout references.

## 2026-08-04 — Vendor-hardware Uncore, SPD, and live-ROM capture

```text
build commit: repository is not initialized as a usable Git worktree
ROM hash: 75232f915d44f2180d35c6b293d21694fae8c217c8f54bbd84a9a7c8819a2db1
flash chip: Winbond W25Q32.V, 4096 KiB, SPI
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: 0x13
DIMM model: 3 x Corsair CMX8GX3M2A1333C9, 4096 MiB each
DIMM SPD SHA-256: 3d7d33481200d79ed5ec6467ac899df1196d984751bc8d8ae6755911e755bfd6
DIMM slot: vendor DMI DIMM0, DIMM2, DIMM4; SMBus 0x50, 0x52, 0x54
GPU: NVIDIA GF108 / GeForce GT 630
PSU: not recorded
boot type: unknown; capture made from an already running PartedMagic session
POST trace: not captured
serial log: not captured
result: complete enumerated Westmere-EP Uncore baseline, SPD set, E820 map, and two identical live-ROM reads
recovery required: no; temporary DEVHIDE1 mask was restored and verified
notes: no coreboot image was built or flashed; no memory-controller or QPI data register was written
```

New verified-hardware and verified-static observations:

- three 256-byte snapshots contained no changed raw PCI-config dword;
- all three channels are active, identity-mapped, triple-interleaved, and hold
  one dual-rank 4-GiB UDIMM each;
- the SPDs support DDR3-1333, while the controller deliberately runs ratio 8
  / DDR3-1066;
- SAD/TAD plus IOH TOLM/TOHM reconstruct the kernel-observed 3-GiB low and
  9-GiB high memory ranges exactly;
- CPU QPI link 0 is operational and connected to the IOH; link 1 is unused;
- live `03.4:0xf8 = 0x00001545`, so the vendor-polled bit 30 is clear after
  successful initialization;
- live `03.4:0xb0 = 0x89abcdef` matches literal writes in the running
  firmware's `MINITDLL` code;
- two flashrom reads were byte-identical; the live `MINITDLL` and public `8F0`
  `.text` sections are byte-identical at SHA-256
  `c93135af56b85c28bb6f6da6589a2f5a7a35c6a79407b21745d369bc1aa292f4`;
- clearing only IOH `DEVHIDE1` bits 26..29 read back successfully but did not
  expose Device 16/17 during normal QPI traffic; original `0x3fffef74` was
  restored and verified.

Detailed register values and evidence boundaries are in
[`research/msi/live-uncore-2026-08-04.md`](../research/msi/live-uncore-2026-08-04.md).

## 2026-08-04 — Invasive locked-runtime IMC and QPI probes

```text
build commit: repository is not initialized as a usable Git worktree
ROM hash: 75232f915d44f2180d35c6b293d21694fae8c217c8f54bbd84a9a7c8819a2db1
flash chip: Winbond W25Q32.V, 4096 KiB, SPI
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: 0x13
DIMM model: 3 x Corsair CMX8GX3M2A1333C9, 4096 MiB each
DIMM slot: vendor DMI DIMM0, DIMM2, DIMM4
GPU: NVIDIA GF108 / GeForce GT 630
PSU: not recorded
boot type: already-running PartedMagic initramfs session
POST trace: not captured
serial log: not captured
result: hard IMC lock confirmed; error injection rejected; periodic QPI L0R measured; no distinct requested QPI reset observed
recovery required: no
notes: host remained reachable; final values matched the pre-probe IMC/QPI state and injection was disabled
```

`MEMLOCK_STATUS = 0x00040401` retained hard `MEM_CFG_LOCKED` after the
documented unlock request.  Channel-0 `tRANKIDLE` and all requested EDAC
injection fields rejected writes.  `QPI_0_PH_PRT = 0x00322808` produces
observed L0R events at approximately 1.743-ms spacing, close to the documented
1.748-ms calculation.  Neither `RETRAIN_NOW` nor `PHY_RESET` produced a
distinct state beyond the already-running periodic L0R sequence.

Structured results are in
[`research/msi/captures/2026-08-04-runtime-invasive-probes.json`](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-08-30 — W25Q128 vendor boot and B00 reset/CAR image build

```text
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B00 raw 4 MiB): abac364481f077e2c922dd627cc10a0b9a0d08512d6f6d094e3b135a7f1ba3aa
ROM hash (B00 W25Q128 16 MiB): 92fed8982f25fbbb0386ffbefa33aa4a712456c59af55b565797ffda3bf15271
flash chip: Winbond W25Q128.V..M, 16 MiB, socketed (user-reported marking)
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: B00 performs no update; prior vendor capture was 0x13
DIMM model: not used by B00
DIMM slot: not used by B00
GPU: not used by B00
PSU: not recorded
boot type: no B00 hardware test yet
POST trace: none for B00
serial log: serial deliberately disabled in B00
result: user reports prior top-aligned vendor-content W25Q128 image booted; B00 built twice identically with the coreboot cross-toolchain and statically verified only
recovery required: none for vendor-content test; B00 not yet flashed
notes: B00 halts at 0xdf before ICH/IMC/QPI/DRAM/romstage; source and flash manifest are in builds/experimental
```

Static verification of B00 confirmed:

- reset vectors at raw offsets `0x003ffff0` and `0x00fffff0`;
- MSI-derived CAR base `0xfff80000`, size `0x10000`, and MSR `0x2e0` in the
  linked bootblock;
- exact POST sequence `01,10,21,21,22,28,29,2b,2c,2f,c0,df`;
- `CLI/HLT` loop immediately after `0xdf`;
- 4-MiB source placed at W25Q128 offset `0x00c00000`, with upper-region
  extraction reproducing the raw-ROM hash;
- no B00 hardware execution has yet been claimed.

## 2026-08-30 — B00 first hardware execution reaches terminal `0xdf`

```text
test ID: B00-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (intended 16 MiB image): 92fed8982f25fbbb0386ffbefa33aa4a712456c59af55b565797ffda3bf15271
flash chip: Winbond W25Q128.V..M, 16 MiB, socketed
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: no update in B00
DIMM model: not recorded for this test; not used by B00
DIMM slot: not recorded for this test; not used by B00
GPU: not recorded for this test; not used by B00
PSU: not recorded
boot type: not reported
POST trace: terminal 0xdf reached correctly; intermediate visibility not reported
serial log: serial disabled in B00
result: PASS for first reset/CAR/C-entry execution; repetition criteria still open
recovery required: not reported
notes: hardware outcome is user-reported; programmer read-back hash was not separately reported
```

This establishes on real hardware that the custom reset vector, 32-bit entry,
MSI-derived Intel no-evict CAR path, CAR stack, and the board C hook execute.
It does not yet establish ICH10R configuration access, Super-I/O access,
serial output, QPI initialization, or DRAM initialization.

## 2026-08-30 — B01 read-only ICH10R identification image

```text
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B01 raw 4 MiB): 72be74d7dccc207d8d213f05ca328702f1b9ef0cd3a038f723da99d90c9e7556
ROM hash (B01 W25Q128 16 MiB): 7700abbe76c08915245b2df97b9f73884160752c33a31a64baba48bfef50ebee
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44 stepping 2
boot type: not yet tested when this build record was written
POST trace: expected c0,c1,d0,df on exact 8086:3a16 result
serial log: serial disabled
result: two byte-identical builds and static machine-code verification; not flashed
recovery required: none; no hardware action yet
notes: active addition is only CF8 address selection 0x8000f800 plus one CFC dword read
```

The linked machine code was inspected to confirm comparison against
`0x3a168086`, terminal `e1/e2/df` halt loops, and absence of any PCI
configuration-data write.  The upper 4 MiB extracted from the W25Q128 wrapper
reproduces the raw-ROM hash.

## 2026-08-30 — B01 first hardware execution reaches terminal `0xdf`

```text
test ID: B01-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (intended 16 MiB image): 7700abbe76c08915245b2df97b9f73884160752c33a31a64baba48bfef50ebee
flash chip: Winbond W25Q128.V..M, 16 MiB, socketed
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: no update in B01
DIMM model: not recorded for this test; not used by B01
DIMM slot: not recorded for this test; not used by B01
GPU: not recorded for this test; not used by B01
PSU: not recorded
boot type: not reported
POST trace: terminal 0xdf user-reported; full trace not separately reported
serial log: serial disabled in B01
result: PASS for first read-only exact-ICH10R-ID path; repetition criteria still open
recovery required: not reported
notes: programmer read-back hash and stable-halt duration were not separately reported
```

The statically verified B01 control flow can reach `df` only after accepting
PCI ID dword `0x3a168086` and emitting `d0`.  Thus the result establishes
early CF8/CFC access and the exact ICH10R LPC identity, but it is not ICH10R
initialization.  B01 performed no PCI configuration-data write, Super-I/O,
serial, IMC, QPI, X58, DRAM, or SPI-write operation.

## 2026-08-30 — B02 narrow LPC, Fintek, and UART diagnostic image

```text
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B02 raw 4 MiB): 10e1373523929a54dc5663179a35faa16b0b8dd2314fd28b411a387f2a62b54f
ROM hash (B02 W25Q128 16 MiB): 16d372be8789653fbf9a7088b421d38eba8c294d7b3f937343111e6e89a2c755
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44 stepping 2
boot type: not hardware-tested
POST trace: expected c0,c1,d0,c2,d1,d2,d3,d4,df after the generic reset/CAR prefix
serial log: expected 115200 8N1 CAR report at 0x3f8; physical header path remains unverified
result: two byte-identical clean builds and static machine-code verification
recovery required: none; no hardware action yet
notes: full i82801jx init, SERIRQ, RCBA, PMBASE, GPIO, TCO, SPI, X58, QPI, IMC, and DRAM remain untouched
```

B02 writes only ICH10R LPC config offsets `0x80=0x0010` and `0x82=0x2001`,
then verifies exact read-back.  It compares Fintek ID bytes `05,41,19,34`,
uses the common Fintek helper to disable/rebase/re-enable only LDN 1 at
`0x3f8`, performs scratch-register and internal 16550 loopback tests, prints a
bounded diagnostic banner, and deliberately halts.  The complete active
machine-code path and all failure halts were inspected.

## 2026-08-30 — B02 first hardware execution reaches terminal `0xdf`

```text
test ID: B02-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (intended 16 MiB image): 16d372be8789653fbf9a7088b421d38eba8c294d7b3f937343111e6e89a2c755
flash chip: Winbond W25Q128.V..M, 16 MiB, socketed
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: no update in B02
DIMM model: not reported; not used by B02
DIMM slot: not reported; not used by B02
GPU: not reported; not used by B02
PSU: not reported
boot type: not reported
POST trace: terminal 0xdf user-reported; full trace not separately reported
serial log: not reported
result: PASS for first narrow-LPC/Fintek/internal-UART path; repetition criteria remain open
recovery required: not reported
notes: programmer read-back hash, stable-halt duration, and physical serial reception were not separately reported
```

For the intended artifact, the statically verified control flow can reach
`df` only after accepting `8086:3a16`, finding no active generic LPC window,
reading back `LPC_IO_DEC=0010` and `LPC_EN=2001`, accepting Fintek bytes
`05,41,19,34`, reading back LDN 1 at `0x3f8`, passing UART scratch and internal
loopback tests, and completing the bounded banner plus `TEMT` wait.  The
reported terminal code does not establish visible JCOM1 output, pinout or
level, a controlled cold boot, full trace, stable-halt duration, programmer
read-back, or repeatability.

## 2026-08-30 — B03 CBFS/XIP romstage-entry diagnostic image

```text
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B03 raw 4 MiB): d67475535f33ada906b0a0bf8567a7e9dd4061fdf58b3134f7797b660a95392c
ROM hash (B03 W25Q128 16 MiB): e36773be5f23639e4b0b3a678ba6e834e220436b966914e8b1408ded0d072091
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: no update in B03
DIMM model: not required; DRAM is not accessed
DIMM slot: not required; DRAM is not accessed
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested
POST trace: expected generic prefix, c0,c1,d0,c2,d1,d2,d3,d4,c3,d5,d6,de
serial log: expected bootblock report, generic romstage console, then the bounded B03 marker
result: two byte-identical clean builds, stable lint pass, and static linked-machine-code verification; not flashed
recovery required: none; no hardware action yet
notes: full ICH10R init, SMBus/SPD, PCIEXBAR, X58, QPI, IMC, DDR3, SPI writes, post-CAR, ramstage, and payload remain untouched
```

`fallback/romstage` is uncompressed and mapped XIP at `0xffc010e0`, with
entry `0xffc01111` and length 13,824 bytes.  The extracted final ELF—not only
the nominal `CONFIG_ROMSTAGE_ADDR=0x02000000` debug link—was inspected.  Its
entry reloads the GDT, sets `ESP=0xfff84000`, clears 80 bytes of BSS, copies 24
bytes of initialized data into CAR, and enters the generic romstage prologue.
CAR data ends at `0xfff88f70`, leaving about 28 KiB of the 64-KiB window free.

The linked bootblock returns after B02 code `d4`, performs the existing BIST
gate, emits `c3` immediately before `run_romstage()`, and then uses the normal
CBFS stage loader.  `mainboard_romstage_entry()` emits `d5`, sends a separately
bounded marker, emits `d6`, then stops permanently at unique success code
`de`.  It cannot return to post-CAR.  The existing `fill_postcar_frame()` and
`cbmem_top_chipset()` traps remain at `ee` as a second fail-closed boundary.

## 2026-08-31 — B03 first hardware execution reaches terminal `0xde`

```text
test ID: B03-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
intended ROM hash (B03 W25Q128 16 MiB): e36773be5f23639e4b0b3a678ba6e834e220436b966914e8b1408ded0d072091
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: no update in B03
DIMM model: not reported; not accessed by B03
DIMM slot: not reported; not accessed by B03
GPU: not reported; not accessed by B03
PSU: not reported
boot type: not reported
POST trace: terminal 0xde user-reported; full trace not separately reported
serial log: not reported
result: PASS for the first CBFS/XIP-romstage/CAR terminal milestone; repetition criteria remain open
recovery required: not reported
notes: programmer read-back hash, stable-halt duration, complete trace, and cold/warm provenance were not separately reported
```

For the intended artifact, the statically verified path can reach `de` only
after the B02 hardware gates, `c3`, CBFS lookup and XIP transfer, the romstage
entry/prologue, `d5`, and the bounded B03 UART/TEMT path at `d6`.  Only the
terminal value itself was directly observed in this report; the omitted data
above therefore remain unknown rather than implicitly passing.

## 2026-08-31 — B04 isolated ICH10R early-core diagnostic image

```text
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B04 raw 4 MiB): 43a360335d695475e54f3ed44a68c4293f09945fc4b8988411b0423be079f0fc
ROM hash (B04 W25Q128 16 MiB): f3cbe7ab63b18066c1bb8269e3f5692a8fbe929197cfa491b436430bc77230d8
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, family 6 model 44
CPU stepping: 2
microcode revision: no update in B04
DIMM model: not required; DRAM/SPD are not accessed
DIMM slot: not required; DRAM/SPD are not accessed
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested; first run must follow complete AC removal
POST trace: expected suffix c0,c1,d0,c2,d1,d2,d3,d4,c3,d5,c4,c5,c6,d7,dd
serial log: expected bounded ICH10/SMBus pre/post report ending POST=dd HALT
result: two byte-identical clean builds from the same local source state and static linked-machine-code verification; not flashed
recovery required: none; no B04 hardware action yet
notes: BAR/enable-register programming only; no access through those windows, SMBus transaction, SPD, X58/QPI/IMC/DDR3, SPI write, post-CAR, ramstage, or payload
```

B04 separates the reusable `i82801jx_setup_bars()` helper from the existing
full ICH10 early-init path and links that helper only into romstage.  The full
`SOUTHBRIDGE_INTEL_I82801JX` option remains off.  The only additional common
routine is `smbus_enable_iobar(0x400)`; no SMBus transaction routine is linked
or called.

The new write set is limited to five PCI configuration writes on `00:1f.0`
(RCBA, PMBASE, ACPI enable, GPIOBASE, and exact `GPIO_CNTL | 0x10`) and three
on `00:1f.3` (BAR4, HOSTC, and PCI command I/O enable).  Before any new write,
the SMBus function must be exact `8086:3a30` with cold-default
`BAR4=1/HOSTC=0/COMMAND=0`.  Already active RCBA, PMBASE, or GPIOBASE decodes
are accepted only at their intended bases.  Afterward every programmed field
is read back, while LPC `0010/2001`, SERIRQ, and all disabled generic LPC
windows must remain unchanged.

`fallback/romstage` remains uncompressed XIP at load `0xffc010e0`, entry
`0xffc01111`, now with length 16,128 bytes.  `_car_unallocated_start` remains
`0xfff88f70`, leaving 28,816 bytes free below `0xfff90000`; the largest visible
B04 local-frame allocation is `0x7c` bytes.  An observed terminal `dd` would
prove the bounded PCI-config/readback experiment only, not access through the
programmed windows.  A warm or CPU-only reset may intentionally stop at `f3` if
the non-default SMBus state programmed by B04 was retained.

## 2026-08-31 — B04 first hardware execution reaches terminal `0xdd`

```text
test ID: B04-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
intended ROM hash (B04 raw 4 MiB): 43a360335d695475e54f3ed44a68c4293f09945fc4b8988411b0423be079f0fc
intended ROM hash (B04 W25Q128 16 MiB): f3cbe7ab63b18066c1bb8269e3f5692a8fbe929197cfa491b436430bc77230d8
flash chip: not separately reported for this run; intended socketed Winbond W25Q128.V..M
board revision: not separately reported; intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: not separately reported; intended Intel Xeon E5645
CPU stepping: not separately reported; intended stepping 2
microcode revision: not reported
DIMM model: not reported; not accessed by B04
DIMM slot: not reported; not accessed by B04
GPU: not reported; not accessed by B04
PSU: not reported
boot type: not reported
POST trace: terminal 0xdd user-reported; full trace not separately reported
serial log: not reported
result: PASS for the first isolated ICH10R/SMBus PCI-configuration-register terminal milestone; repetition criteria remain open
recovery required: not reported
notes: programmer read-back hash, stable-halt duration, complete trace, and cold/warm provenance were not separately reported
```

For the intended artifact, the statically verified path can reach `dd` only
after all B03 gates, the guarded ICH10R BAR helper, the SMBus BAR/HOSTC/PCI
command helper, exact read-backs, preserved LPC/SERIRQ invariants, and the
bounded B04 UART/TEMT report.  Only the terminal value itself was directly
reported.  It establishes neither SMBus bus traffic nor SPD, QPI, IMC, DDR3,
ramstage, or payload operation.

## 2026-08-31 — B05 single SPD memory-type probe image

```text
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B05 raw 4 MiB): 2140566dd204cecf4dc72cbefea95b93c22a4b4538648034f03b1aec3dcca3af
ROM hash (B05 W25Q128 16 MiB): 1c147f8a366b0c494b0630a236758fbe053fcac823910da73d342fce384dc0b9
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: required Intel Xeon E5645, CPUID leaf-1 EAX 0x000206c2
CPU stepping: 2
microcode revision: no update in B05
DIMM model: first run should retain a population known under vendor firmware to answer at SPD address 0x50
DIMM slot: physical slot-to-address mapping remains unverified
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested; first run must follow complete AC removal
POST trace: expected suffix c0,c1,d0,c2,d1,d2,d3,d4,c3,d5,c4,c5,c6,d7,c7,c8,c9,d8,dc
serial log: expected ICH10 report followed by PRE/ARMED/DONE, status, poll count, data 0b, and POST=dc HALT
result: two clean builds are byte-identical; stable lint, checkpatch, final ELF disassembly, and independent read-only audit pass; not flashed
recovery required: none; no B05 hardware action yet
notes: exactly one SMBus read-byte-data transaction to 0x50 offset 0x02; no scan, retry, calibrated delay, SPD dump/CRC, QPI/IMC/DDR3, SPI write, post-CAR, ramstage, or payload
```

B05 preserves B04 and adds an exact runtime CPUID gate before the B04 ICH10R
experiment.  It then reads I801 `HSTSTAT` first to acquire/check the host
semaphore.  If `INUSE` or `HOST_BUSY` is already set, it stops at `f8` without
touching another SMBus host register.  With an accepted cold state it arms
`HSTCTL=0x08`, `XMITADD=0xa1`, `HSTCMD=0x02`, and zeroes/read-backs DAT0/1.
Code `c8` is emitted immediately before the image's sole START write
`HSTCTL=0x48`.

There is no verified pre-RAM clock on the current CPU port, so the transaction
is bounded by exactly 1,000,000 serialized `HSTSTAT` reads rather than a
claimed wall-clock duration.  Completion accepts `INTR` while ignoring
`BYTE_DONE`, `INUSE`, and `SMBALERT` in the result comparison, as the existing
I801 implementation does.  Observed completion flags are cleared and the
semaphore is released only after BUSY has cleared.  The final disassembly has
exactly one START and no linked common byte-read, retry, KILL, or delay path.

New terminal meanings are:

```text
c7  host/semaphore/cold state accepted; command window about to be armed
c8  command read-back passed; the single START follows immediately
c9  BUSY cleared, result was exact success, DAT0 read, and host released
d8  successful diagnostic report reached UART TEMT
dc  DAT0 was exactly 0x0b; intentional B05 success halt

e0  CPU signature was not exactly 0x000206c2
f8  initial host state or armed-command read-back was invalid
f9  one-million-read limit reached; no KILL, release, or recovery attempted
fa  exact DEV_ERR result; this can include no ACK at the selected address
fb  exact BUS_ERR result
fc  FAILED, combined, or otherwise unexpected terminal result
fd  transaction succeeded, but DAT0 was not 0x0b
fe  successful data result, but final UART report/TEMT failed
```

At `f9`, remove AC power completely; do not use reset as an implicit SMBus
recovery.  Terminal `dc` proves only one successful read of the DDR3 memory
type byte at logical address `0x50`.  It does not prove the other 255 SPD
bytes, integrity fields, physical slot mapping, or any memory initialization.
The final raw CBFS contains an uncompressed 17,632-byte romstage at load
`0xffc010e0`, entry `0xffc01111`.  The linked ELF has 17,608 bytes of text,
24 bytes of data, and a `0x8c`-byte main local frame.  CAR data ends at
`0xfff88f70`, leaving 28,816 bytes below `0xfff90000`.

## Hardware-test entry template

```text
date/test ID:
build commit:
ROM hash:
flash chip:
board revision:
CPU:
CPU stepping:
microcode revision:
DIMM model:
DIMM SPD SHA-256:
DIMM slot:
GPU:
PSU:
boot type: cold / warm / reset
POST trace:
serial log:
result:
recovery required:
notes:
```

## 2026-08-31 — B05 first hardware execution reaches terminal `0xfa`

```text
test ID: B05-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
intended ROM hash (B05 raw 4 MiB): 2140566dd204cecf4dc72cbefea95b93c22a4b4538648034f03b1aec3dcca3af
intended ROM hash (B05 W25Q128 16 MiB): 1c147f8a366b0c494b0630a236758fbe053fcac823910da73d342fce384dc0b9
programmer read-back hash: not reported
flash chip: not separately reported for this run; intended socketed Winbond W25Q128.V..M
board revision: not separately reported; intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: exact CPUID leaf-1 EAX 0x000206c2 was necessarily accepted; exact SKU not separately reported
CPU stepping: signature stepping 2; exact CPU inventory not separately reported
microcode revision: no update in B05
DIMM model: not reported
DIMM slot/population: not reported
GPU: not required; not reported
PSU: not reported
boot type: not reported
POST trace: terminal 0xfa user-reported; full trace not separately reported
serial log: not reported
result: DIAGNOSTIC — exact I801 DEV_ERR after the one 0x50:0x02 START; B05 terminal-dc gate not met
recovery required: not reported
notes: terminal duration, cold/warm provenance, and DIMM address availability are unknown
```

The statically verified B05 control flow can reach `fa` only after accepting
CPUID `0x000206c2`, completing the B04 controller path, acquiring the I801
host, arming and starting the one byte-data command, observing BUSY clear with
the masked result exactly `DEV_ERR`, clearing the observed completion flags,
and releasing the host semaphore.  It did not crash or time out.  `DEV_ERR`
does not distinguish an unclaimed address from a host-device timeout; the
unrecorded DIMM population also means that an SPD at `0x50` is not established
for this run.

## 2026-08-31 — Running-vendor I801/SPD reference at `192.0.2.203`

```text
test ID: REF-I801-01
build commit: no experimental firmware; vendor-booted Linux reference
ROM hash: not reread in this capture
flash chip: not reread in this capture
board revision: MSI X58 Pro-E / MS-7522 revision 3.0 from the established reference inventory
CPU: Intel Xeon E5645
CPU stepping: 2
microcode revision: not reread in this capture
DIMM model: 3 x Corsair CMX8GX3M2A1333C9, 4 GiB each
DIMM SPD SHA-256: 3d7d33481200d79ed5ec6467ac899df1196d984751bc8d8ae6755911e755bfd6
DIMM slot: DMI DIMM0/DIMM2/DIMM4; physical silkscreen mapping still unverified
GPU: not relevant to this capture
PSU: not recorded
boot type: unknown; already-running PartedMagic/Linux session after vendor initialization
POST trace: not captured
serial log: not captured
result: I801 8086:3a30 at base 0x400; runtime PIN_CTL=0x07; valid DDR3 SPDs at 0x50/0x52/0x54; 0x51/0x53/0x55..0x57 returned ENXIO
recovery required: none
notes: read-only PCI/pin inspection plus Linux SMBus byte-data reads; no EEPROM data write, IMC write, QPI write, or reset-state claim
```

This runtime reference confirms valid SPD devices on the board after vendor
initialization, but it neither reconstructs B05's physical DIMM population nor
proves its pre-DRAM `PIN_CTL` value.  Detailed commands and evidence boundaries
are in [`research/msi/live-i801-2026-08-31.md`](../research/msi/live-i801-2026-08-31.md).

## 2026-08-31 — B06A bounded SMBus-pin and SPD-address diagnostic image

```text
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B06A raw 4 MiB): 83100b39b15997576f0e7a599a0e8ab5c1e78fdf1cff6f204926620e206e930e
ROM hash (B06A W25Q128 16 MiB): 975fb8ff1df8ed95c2ff29b82ade4530e49a0b8d373b87ffaa568fa92b582eb3
ROM size: 4194304 bytes
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: required Intel Xeon E5645, CPUID leaf-1 EAX 0x000206c2
CPU stepping: 2
microcode revision: no update in B06A
DIMM model: first hardware run must record the population before changing it
DIMM slot: first hardware run must record every physical slot before changing it
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested; first run must follow complete AC removal
POST trace: expected common B04 suffix through d7, then 54,55,56, address-qualified 80..9f codes, and one terminal 41..53; CPUID mismatch stops at 57
serial log: expected initial/enabled/final pin values plus per-address status, poll count, type byte, and result bitmaps
result: two fresh ccache-disabled builds (final3/final4) are byte-identical; not flashed or hardware-tested
recovery required: none; no B06A hardware action yet
notes: type byte 0x02 only at 0x50..0x57; no retry, full SPD dump/CRC, slot proof, QPI/IMC/DDR3, SPI write, post-CAR, ramstage, or payload
```

B06A preserves the exact-CPUID and B04 controller gates, but assigns CPUID
mismatch its own terminal `57`.  After acquiring the I801 host, it records
`SMBus_PIN_CTL`, writes exactly `0x04`, and polls at most 100,000 pin reads for
the idle value masked as `0x07`.  It then performs at most eight SMBus
byte-data STARTs: one read of offset `0x02` at each address `0x50..0x57`, each
bounded by one million serialized status reads and with no retry.

Exact `DEV_ERR` is cleared and advances to the next address.  Successful DDR3
type `0x0b` and successful other-type results are recorded separately.  A
transaction timeout deliberately performs no KILL, host release, or recovery;
complete AC removal is required after terminal `50`.  B06A does not satisfy
the B06B full-SPD, integrity, or physical-slot milestone.

The final B06A POST groups are:

```text
41..48  exactly one DDR3 responder at 50..57, respectively
49      multiple DDR3 responders
4a      no responder; all eight completed commands returned exact DEV_ERR
4b      at least one successful response returned a type other than 0b
4c..4e  clock, data, or both pin conditions invalid
4f      initial host state or command read-back invalid
50      transaction timeout without recovery
51/52   exact BUS_ERR / other transaction failure
53      good scan result, but final UART/TEMT failure
54..56  pin phase entered, PIN_CTL=04 written, pins observed idle high
57      CPUID leaf-1 EAX was not exactly 000206c2
80..87  immediately before START at 50..57
88..8f  exact DEV_ERR at 50..57
90..97  successful DDR3 type 0b at 50..57
98..9f  successful other type at 50..57
```

## 2026-08-31 — B06A first hardware execution reaches terminal `0x45`

```text
test ID: B06A-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
intended ROM hash (raw 4 MiB): 83100b39b15997576f0e7a599a0e8ab5c1e78fdf1cff6f204926620e206e930e
intended ROM hash (W25Q128 16 MiB): 975fb8ff1df8ed95c2ff29b82ade4530e49a0b8d373b87ffaa568fa92b582eb3
programmer read-back hash: not reported
flash chip: not separately reported; intended socketed Winbond W25Q128.V..M
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: CPUID leaf-1 EAX 0x000206c2 necessarily accepted; exact SKU not separately reported
CPU stepping: signature stepping 2
microcode revision: no update in B06A
DIMM model: not reported
DIMM slot/population: not reported
GPU: not required; not reported
PSU: not reported
boot type: not reported
POST trace: terminal 0x45 user-reported; complete trace not separately reported
serial log: physical reception not reported
result: SUCCESS FOR B06A ADDRESS DIAGNOSTIC — exactly one DDR3 responder at 0x54
recovery required: not reported
notes: full SPD, CRC, physical slot, stability, cold/warm provenance, and repetition remain open
```

Terminal `0x45` implies that the inherited controller gates passed,
`PIN_CTL[2:0]` reached idle `0x07` after the controlled write, all eight
commands completed without timeout, BUS_ERR, FAILED, or combined status, only
`0x54` returned DDR3 type `0x0b`, and the other seven addresses returned exact
`DEV_ERR`.  It also implies that B06A's internal final UART/TEMT gate did not
fail; it does not prove that external serial text was received.

## 2026-08-31 — DDR-ratio-6 QPI Slow/High reference comparison

```text
test ID: REF-QPI-R6-SH-01
build commit: no experimental firmware; vendor-booted Linux reference
ROM hash: not reread
flash chip: not reread
board revision: MSI X58 Pro-E / MS-7522 revision 3.0 from established inventory
CPU: Intel Xeon E5645
CPU stepping: 2
microcode revision: not reread
DIMM model: established reference population, 3 x Corsair CMX8GX3M2A1333C9
DIMM slot: DMI DIMM0/DIMM2/DIMM4; physical silkscreen mapping still unverified
GPU: not relevant
PSU: not recorded
boot type: vendor boots into Linux; cold/warm provenance not recorded
POST trace: not captured
serial log: not captured
result: DDR ratio 6 held constant; isolated QPI Slow-to-High state transition captured reproducibly
recovery required: none
notes: read-only PCI configuration capture; no Linux register write or experimental firmware
```

All three Slow captures were byte-identical at SHA-256
`de3ec2ae667abe816cb770ad9b4ed033c8d414f82d711240dd6f85092d2e73b4`;
all three High captures were byte-identical at
`53c9f7caf24e6495f4741fa6b990a13b87cdd5d99539ef7ce35612de35b94a23`.
The link-0 PLL ratio changed from `0x10` to `0x12`, and High Speed restored
`PH_CTR=0x0040a0a8`, `PH_PIS=0x070f0f03`, and `PH_PRT=0x00322808`, exactly as
in the older DDR-ratio-8 High-Speed state.  Static DDR3-800 policy and mapping
registers stayed unchanged.  Training residues changed and must not be
hard-coded.  The snapshots prove a stable post-init state, not the paired
CPU/IOH write order, reset, calibration, or training protocol.  See
[`research/msi/qpi-ratio6-slow-high-2026-08-31.md`](../research/msi/qpi-ratio6-slow-high-2026-08-31.md).

## 2026-08-31 — B06B fixed SPD-0x54 base-block/CRC image

```text
test ID: B06B-BUILD-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B06B raw 4 MiB): 3ed10d4ee152370528ab0c70d85699fc95d0251d02817c8839b37fa437e8f2e0
ROM hash (B06B W25Q128 16 MiB): 7e36698f88b92cc4937131fff68b3cd7f8816aa5975c3a111c171239a4593e6b
ROM size: 4194304 bytes
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: required Intel Xeon E5645, CPUID leaf-1 EAX 0x000206c2
CPU stepping: 2
microcode revision: no update in B06B
DIMM model: first run must retain the B06A responder configuration; exact model not reported
DIMM slot: first run must retain address 0x54 without moving modules
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested; first run must follow complete AC removal
POST trace: expected inherited B04 suffix through d7, then 54,55,56,58,59,5a..61,62,63,64
serial log: expected 128-byte hex dump, exact offset/status/polls/pins, and calculated/stored CRC
result: two clean ccache-disabled builds are byte-identical; static source and binary audits passed; not flashed
recovery required: none; no B06B hardware action yet
notes: fixed 0x54 offsets 0x00..0x7f only; no retry, upper SPD, QPI/IMC/DDR3 write, SPI write, post-CAR, ramstage, or payload
```

B06B holds one I801 semaphore across at most 128 read-byte-data STARTs.  Each
command uses read address `0xa9`, verifies its command-register setup, polls at
most one million serialized status reads, accepts only exact `INTR`, checks the
SMBus pins, and clears completion flags while retaining ownership.  Exact
`DEV_ERR`, `BUS_ERR`, or other results halt with a distinct code.  On timeout
it performs no KILL, W1C clear, host release, pin read, or further I801 access;
complete AC removal is required after code `50`.

After offset `0x02` returns DDR3 type `0x0b`, a complete base block selects
CRC coverage from SPD byte 0 bit 7: 117 bytes when set, otherwise 126.  It uses
coreboot's existing `ddr_crc16()` (polynomial `0x1021`, initial zero) and
compares against little-endian bytes 126/127.  The reference hypothesis is
header `92 10 0b 02`, coverage 117, and calculated/stored CRC `0xe5fc`.

## 2026-08-31 — B06B first hardware execution reaches terminal `0x64`

```text
test ID: B06B-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
intended ROM hash (raw 4 MiB): 3ed10d4ee152370528ab0c70d85699fc95d0251d02817c8839b37fa437e8f2e0
intended ROM hash (W25Q128 16 MiB): 7e36698f88b92cc4937131fff68b3cd7f8816aa5975c3a111c171239a4593e6b
programmer read-back hash: not reported
flash chip: not separately reported; intended socketed Winbond W25Q128.V..M
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: CPUID leaf-1 EAX 0x000206c2 necessarily accepted; exact SKU not separately reported
CPU stepping: signature stepping 2
microcode revision: no update in B06B
DIMM model: not reported
DIMM slot/population: not reported; logical SPD responder remained fixed at 0x54
GPU: not required; not reported
PSU: not reported
boot type: not reported
POST trace: terminal 0x64 user-reported; complete trace not separately reported
serial log: physical reception not reported
result: SUCCESS FOR B06B BASE-SPD GATE — all 128 fixed-0x54 reads and the selected base CRC passed
recovery required: not reported
notes: actual bytes/hash, physical slot, cold/warm provenance, stability, and repetition remain open
```

Terminal `0x64` is reachable only after the inherited CPU/ICH10R/SMBus gates,
128 exact `INTR` completions for offsets `0x00..0x7f` at logical address
`0x54`, DDR3 type `0x0b`, and equality of the revision-selected calculated
and stored base CRC.  It also implies that B06B's internal final UART/TEMT
gate did not fail.  It does not prove the hypothesized reference bytes or
hash, external serial reception, physical-slot identity, a true cold start,
or repeatability.

## 2026-08-31 — B06C used-176-SPD/decode image

```text
test ID: B06C-BUILD-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B06C raw 4 MiB): c9ef26c479775e48a2a94e23c1ce3cc7cba0f65dd238ff8682aa688420cf35a3
ROM hash (B06C W25Q128 16 MiB): 2b783b30b6c9ef4ebe3b6d1636d2e402c0a700fbee62d5bad8f6d8e77347ac82
ROM size: 4194304 bytes
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: required Intel Xeon E5645, CPUID leaf-1 EAX 0x000206c2
CPU stepping: 2
microcode revision: no update in B06C
DIMM model: first run must retain the B06B responder configuration; exact test inventory not yet reported
DIMM slot: first run must retain logical address 0x54 without moving modules
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested; first run must follow complete AC removal
POST trace: expected B06B progress through 0x62, then 68,69,6a..6c,6d,6e..70,71,72,73,74,75
serial log: expected 176-byte dump, double-read/fingerprints, decoder tuple, and JEDEC-cycle candidate
result: two clean ccache-disabled builds are byte-identical; source/binary/semantic audits passed; not flashed
recovery required: none; no B06C hardware action yet
notes: fixed 0x54 only; no offsets b0..ff, XMP, retry, QPI/IMC/DDR3 write, SPI write, post-CAR, ramstage, or payload
```

B06C performs at most 224 byte-data reads while retaining one I801 semaphore:
128 base bytes, 48 upper used bytes, and a second verification of those same
48 bytes.  It releases the host before guarded semantic decoding.  The exact
observed raw structure and MTB values prevent malformed decoder arithmetic;
the resulting DDR3-800 tuple is explicitly a JEDEC/SPD cycle candidate rather
than an X58 register encoding.  The full build, error, and hardware-test
contract is in
[`builds/experimental/msi-x58-pro-e-b06c-manifest.md`](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-08-31 — B06C first hardware execution reaches header rejection `0x76`

```text
test ID: B06C-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
intended ROM hash (raw 4 MiB): c9ef26c479775e48a2a94e23c1ce3cc7cba0f65dd238ff8682aa688420cf35a3
intended ROM hash (W25Q128 16 MiB): 2b783b30b6c9ef4ebe3b6d1636d2e402c0a700fbee62d5bad8f6d8e77347ac82
programmer read-back hash: not reported
flash chip: not separately reported; intended socketed Winbond W25Q128.V..M
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: CPUID leaf-1 EAX 0x000206c2 necessarily accepted; exact SKU not separately reported
CPU stepping: signature stepping 2
microcode revision: no update in B06C
DIMM model: not reported
DIMM slot/population: not reported; logical SPD responder remained fixed at 0x54
GPU: not required; not reported
PSU: not reported
boot type: not reported
POST trace: terminal 0x76 user-reported; complete trace not separately reported
serial log: physical reception not reported
result: FAIL-CLOSED AT B06C HEADER POLICY — base reads/type/CRC passed, but byte 0 did not declare the one accepted 176-used/256-total tuple
recovery required: not reported
notes: raw SPD[0], failing used-vs-total subfield, physical slot, cold/warm provenance, stability, repetition, and read-back remain open
```

Terminal `0x76` is reachable only after all 128 base reads, DDR3 type `0x0b`,
and the selected base CRC have passed through code `0x62`.  The path issued
the I801 host-release write before returning `0x76`; it performed no read at
`0x80` or above and called neither the DDR3 decoder nor the policy derivation.
The code does not identify raw SPD byte 0.  A normal `0x91` declaration
(128 used, 256 total, CRC coverage through byte 116) is plausible but remains
an inference until B06H or a physical UART dump reports it.

## 2026-08-31 — B06H fixed-SPD header-telemetry image

```text
test ID: B06H-BUILD-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B06H raw 4 MiB): fb31365498cc71a4c9ee6b1418e3d5fcc45b4cfc5bcb19a44b849077564cabe6
ROM hash (B06H W25Q128 16 MiB): cd5fca5bcfdab03bb3ffaa15dd8d670280b073d185358e23fb043af3c2f23476
ROM size: 4194304 bytes
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: required Intel Xeon E5645, CPUID leaf-1 EAX 0x000206c2
CPU stepping: 2
microcode revision: no update in B06H
DIMM model: first run must retain the exact B06C population; exact inventory not reported
DIMM slot: retain logical SPD responder 0x54 without moving modules
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested; first run must follow complete AC removal
POST trace: expected inherited B06B progress through 0x62, then 0x7a and one encoded header terminal
serial log: optional raw 128-byte dump including exact B0; terminal POST remains authoritative
result: two clean ccache-disabled builds are byte-identical; source/binary audits passed; not flashed
recovery required: none; no B06H hardware action yet
notes: no additional SMBus command, upper-SPD read, decoder, policy, QPI/IMC/DDR3 write, SPI write, post-CAR, ramstage, or payload
```

B06H repeats the already successful B06B 128-byte/CRC path.  After the host
release and code `0x62`, it emits marker `0x7a`, attempts only the bounded
UART report, and halts with `0x80 | (SPD[0] & 0x7f)`.  The encoding preserves
the used/total fields while tagging the terminal as telemetry; raw bit 7 only
selects the CRC coverage that has already passed and remains visible in UART.
Thus terminal `0x91` means 128 used/256 total, `0x92` means 176/256, and
`0x93` means 256/256.  Other values remain fail-closed evidence, not a reason
to read beyond byte `0x7f`.

## 2026-08-31 — B06H hardware execution proves raw header `0x93` and serial output path

```text
test ID: B06H-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
intended ROM hash (raw 4 MiB): fb31365498cc71a4c9ee6b1418e3d5fcc45b4cfc5bcb19a44b849077564cabe6
intended ROM hash (W25Q128 16 MiB): cd5fca5bcfdab03bb3ffaa15dd8d670280b073d185358e23fb043af3c2f23476
programmer read-back hash: not reported
flash chip: not separately reported; intended socketed Winbond W25Q128.V..M
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: CPUID leaf-1 EAX 0x000206c2 necessarily accepted; exact SKU not separately reported
CPU stepping: signature stepping 2
microcode revision: no update in B06H
DIMM model: not reported
DIMM slot/population: not reported; logical SPD responder was 0x54
GPU: not required; not reported
PSU: not reported
boot type: not reported
POST trace: bootblock c3; romstage 62,7a; terminal 93 user-reported through serial text
serial log: complete bootblock and romstage output received at COM1 0x3f8, 115200 8N1
result: PASS — all 128 base reads, DDR3 type, base CRC, raw byte-0 telemetry, and bounded UART reporting succeeded; intentional terminal 0x93
recovery required: not reported
notes: one execution reported; cold/warm provenance, programmer read-back, physical DIMM inventory, and repetition remain open
```

The received bytes are preserved in
[`research/msi/b06h-spd54-base-2026-08-31.md`](../research/msi/b06h-spd54-base-2026-08-31.md).
Raw `SPD[0]=0x93` means 256 bytes used and 256 bytes total; bit 7 selects CRC
coverage of bytes `0..116`.  The independently identified 128-byte base-block
SHA-256 is `4c13dd162c0545dc692a1f5ba93b56722c6daf7555e4751f6af3546847a4bbc7`.
The reported CRC calculation and stored little-endian value both equal
`0xec4f`.  Each of the 128 SMBus commands completed after seven status polls
(`TOTAL=0x380`, `MAX=7`), and all recorded pin samples were `0x07`.

This run also changes the serial evidence boundary: physical transmission from
the bootblock and coreboot C romstage through COM1/JCOM1 is now demonstrated.
It does not yet demonstrate UART receive or an interactive console.

## 2026-08-31 — B06I full-used-SPD read/decode image

```text
test ID: B06I-BUILD-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B06I raw 4 MiB): 509328b64e97338280174fdf99443bbd9c86858a230e2d6b99797d33719c57b9
ROM hash (B06I W25Q128 16 MiB): 50cf4147fd7254e5dcd6ee0872bd9e41b4a63411970512c1d790021a274dbafd
ROM size: 4194304 bytes
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: required Intel Xeon E5645, CPUID leaf-1 EAX 0x000206c2
CPU stepping: 2
microcode revision: no update in B06I
DIMM model: retain the B06H logical responder at 0x54; exact inventory not reported
DIMM slot: do not move population before first test
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested; first run must follow complete AC removal
POST trace: expected inherited base path through 0x62, then a0,a1,a2..a9,aa,ab..b2,b3,b4,b5,b6,b7
serial log: expected all 256 bytes, fingerprints, decoder tuple, and unprogrammed DDR3-800 candidate
result: two fresh ccache-disabled builds are byte-identical; source/binary audits passed; not flashed
recovery required: none; no B06I hardware action yet
notes: at most 384 bounded I801 commands; no SPD/IMC/QPI/DDR/SPI write, DRAM, ramstage, or payload
```

B06I corrects only the rejected B06C length hypothesis.  It accepts the now
measured `0x93` declaration, double-reads the complete upper 128 bytes, and
reuses the existing fail-closed decoder/policy code.  The I801 host is released
before decoding.  B06B, B06C, and B06H regression builds remain byte-identical
to their preserved artifacts.

## 2026-08-31 — B06J interactive CAR ROMMON with SerialICE stream

```text
test ID: B06J-BUILD-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
ROM hash (B06J raw 4 MiB): 5acd985ac13ec2d6b988286de6971a3c46cebab71a614524ef95025b137cd80f
ROM hash (B06J W25Q128 16 MiB): 2a6addefd7a620e41ee9fba4480993d5372481289b44458e0dce1b1f7e6c3e15
ROM size: 4194304 bytes
flash chip: intended Winbond W25Q128.V..M, 16 MiB, socketed
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: required Intel Xeon E5645, CPUID leaf-1 EAX 0x000206c2
CPU stepping: 2
microcode revision: no update in B06J
DIMM model/slot: not consumed at monitor entry; retain B06H population for optional spd command
GPU: not required
PSU: not recorded; image not hardware-tested
boot type: not hardware-tested; first run must follow complete AC removal
POST trace: expected inherited B04 path through d7, then bc while rommon prompt waits for RX
serial log: expected ROMMON prompt at COM1 0x3f8, 115200 8N1
result: two fresh ccache-disabled builds are byte-identical; protocol/source/binary audits passed; not flashed
recovery required: none; no B06J hardware action yet
notes: UART transmit is hardware-proven by B06H; RX, interaction, command behavior, and SerialICE-QEMU forwarding are not yet runtime-proven
```

B06J exposes bounded human commands for CPUID, MSR, I/O, PCI configuration,
MMIO, memory dumps, and on-demand B06I SPD telemetry.  Human writes require
`unlock WRITE` and consume the arm before one access.  A distinct permanent
SerialICE v1.5 mode implements the documented `rm/wm`, `ri/wi`, `rc/wc`,
`ci`, `mb`, and `vi` stream commands.  Its writes are immediate because that
is required for emulator forwarding.  The final wire audit additionally
matches QEMU's initial `@` handshake, 32-character padded `mb` response, and
LF-only `vi` framing.  The first hardware test must remain in the human
read-only mode; see `Documentation/b06j-rommon.md`.

## 2026-09-01 — B06J UART RX and controlled one-channel IMC experiments

```text
test ID: B06J-HW-01
build commit: upstream coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental patch
intended ROM hash (raw 4 MiB): 5acd985ac13ec2d6b988286de6971a3c46cebab71a614524ef95025b137cd80f
intended ROM hash (W25Q128 16 MiB): 2a6addefd7a620e41ee9fba4480993d5372481289b44458e0dce1b1f7e6c3e15
programmer read-back hash: not reported
flash chip: user-reported Winbond W25Q128.V..M, 16 MiB, socketed
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0; not re-read in this run
CPU: CPUID leaf-1 EAX 0x000206c2; intended Xeon E5645
CPU stepping: signature stepping 2
microcode revision: no update in B06J
DIMM model: Crucial BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8 UDIMM
DIMM slot: physical slot not recorded; sole logical SPD responder 0x54; inferred channel 2
GPU: not required; not reported
PSU: not reported
boot type: not reported; monitor was already waiting when the session attached
POST trace: monitor entry previously expected at bc; POST board value was not independently captured in this session
serial log: interactive COM1 session through serial-gateway.example.invalid /dev/ttyUSB1, 115200 8N1, no flow control
result: PARTIAL PASS — UART RX, ROMMON reads/writes, full SPD, RCOMP, both-rank ZQCL, and bounded training-FSM completion proved; no DDR training pass bit
recovery required: complete AC removal required after the live IMC writes; not performed or observed in this session
notes: one execution only; all writes affected volatile CPU-Uncore state, not SPI flash
```

The monitor returned build ID `X58PROE-B06J-CAR-ROMMON-20260831`, CPUID
`0x000206c2`, ICH10R LPC ID `8086:3a16`, and SMBus ID `8086:3a30`.  This is
the first hardware proof of physical UART receive and interactive B06J command
execution.  Sending several commands without pacing produced one bounded UART
receive-status error; individually paced commands were reliable.

The on-demand SPD command read all 256 bytes twice and returned `RESULT=b7`.
The SPD base CRC was `0xec4f`, the two upper reads matched, and the decoder
reported 4096 MiB, two ranks, x8 devices, 15 row bits, and 10 column bits.  Its
conservative policy candidate was DDR3-800 CL5/tRCD5/tRP5/tRAS12.  The DIMM
part string was `BLS4G3D1609DS1S00.`.

The reset-state Uncore snapshot found the CPU-to-X58 link already operational
in the previously documented Slow-QPI tuple: `02.0:50=0x86000000`,
`02.1:50=0x160c0110`, `02.1:54=0x10`, and `02.1:80=0x030f0f03`.  The common
DDR clock block was likewise already at ratio 6: `03.4:50=0x0a000006` and
`03.4:54=0x6`.  In contrast, channel active bits, DIMM organization, rank
presence, MRS values, and training status were initially zero.

The experiment was deliberately restricted to the inferred populated channel
2 (`ff:06.x`).  It applied the documented MSI reset/CKE sequence, programmed
the matching dual-rank `MC_DOD_CH_DIMM0=0x2ac`, `RANK_PRESENT=0x03`, identity
channel mapper, one-channel `MC_CONTROL=0x400`, and conservative policy values
from the three identical vendor DDR-ratio-6 captures.  The DDR3-800 MRS policy
was `MR0=0x1528`, `MR1=0x0806`, `MR2=0`; direct commands were issued in
MR2/MR3/MR1/MR0 order for ranks 0 and 1.

Two independently observable calibration milestones succeeded:

- Intel-mandated `RCOMP_PARAMS=0x00013df0` followed by `DO_RCOMP` produced
  `MC_CHANNEL_DIMM_INIT_STATUS=0x00000200` (`RCOMP_CMPLT`).
- `DO_ZQCL` for each rank separately produced status `0x00000080`
  (`ZQCL_CMPLT`).

Automatic rank-0 PHY training with `STOP_ON_FAIL` completed without hanging:
the `TRAIN` action self-cleared and status became `0x00000100`
(`INIT_CMPLT`).  None of `RD_DQ_DQS_PASS`, `RD_RCVEN_PASS`, `WR_LEVEL_PASS`,
or `WR_DQ_DQS_PASS` was set.  Repeating after exact reconstruction of the MSI
three-buffer pattern-generator contents and after setting observed unknown
`03.4:5c=1` produced the same deterministic `0x100`.  QPI stayed at
`0x030f0f03`, and the unpublished busy observation `03.4:f8` stayed zero.

This run therefore does not establish usable DRAM.  It narrows the next
missing prerequisite to PHY/per-lane preparation before the first RD DQ-DQS
phase rather than SMBus, SPD policy, QPI L0, DIMM reset/CKE, RCOMP, ZQCL, or
FSM liveness.  The next image should automate the proven sequence with bounded
polls and emit separate POST/serial markers around each manual PHY-init state.

## 2026-09-01 — B06J X58 PHY scan-chain reconstruction and live training reads

```text
test ID: B06J-HW-02
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash: running B06J hash not reread during this session; build ID reported previously as X58PROE-B06J-CAR-ROMMON-20260831
flash chip: Winbond W25Q128.V..M reported by user; socketed experimental chip
board revision: not re-recorded
CPU: Intel Xeon E5645
CPU stepping: CPUID leaf-1 EAX 0x000206c2
microcode revision: no update in B06J
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8 UDIMM
DIMM slot: sole responder at SPD 0x54; inferred channel 2
GPU: unchanged; not re-recorded
PSU: not re-recorded
boot type: user reported freshly booted; AC removal was not independently confirmed
POST trace: interactive B06J prompt; training status repeatedly 0x00000100
serial log: COM1 0x3f8, 115200 8N1 through serial-gateway /dev/ttyUSB1
result: PARTIAL PASS — bounded PHY scan reads/writes and live per-lane result mutation proved; no RD or RCVEN pass bit
recovery required: AC removal required before the next clean comparison; no SPI write occurred
notes: volatile CPU-Uncore/IMC writes only; QPI remained operational
```

Static reconstruction found that `MINITDLL` initialization copies `0x23a`
three-byte PHY descriptors from `0xfffd9c11` into its workspace.  Each entry is
`{ uint16_t start_bit; uint8_t width; }`.  The indirect access engine is CPU
Uncore function `ff:03.4`: channel selection is in `0x5c[26:25]`, data is at
`0xfc`, and command/busy is at `0xf8`.  The Westmere descriptor table ends at
bit `0x1603`; MINIT uses read-chain base `0x162a`.  A write shifts two control
bits above the useful value and submits `BIT(30) | end_bit`; a read submits
`BIT(31) | (0x162a - end_bit)`.

The first correctly addressed live read, descriptor index `0x102`
(`start=0x09fa`, width 8), completed immediately.  `0xf8` returned
`0x00000c29`, `0xfc=0x80202080`, and the useful low-six-bit value was zero.
Reads of the rank-0 lane-0/lane-7 RCVEN fields likewise returned their seeded
values.  Exact MINIT writes were then applied to channel 2: the global field
was seeded to one, each primary lane field to zero, and each secondary field
to `0x15e`.  Readback returned useful values `1`, `0`, and `0x15e`
respectively, proving the descriptor mapping and write protocol.  QPI status
remained `ff:02.1:80=0x030f0f03` throughout.

After FIFO reset `0x20600`, isolated rank-0 RCVEN command `0x27301` completed
with status `0x100` but no pass bit.  Crucially, the PHY results changed:
lane 0 and lane 7 primary values became `0x11`, while both secondary values
saturated at `0x1ff`.  Increasing the global value from `1` to `0x10` produced
the same saturation.  This establishes actual training-engine activity rather
than a dead command path.

The preceding MSI coarse-RD sequence was then reconstructed further.  Nine
seven-bit fields were pulsed to value two and the rank-0 RD sweep value and
eight pairs of lane fields were reset to zero.  Isolated `0x26b01` completed
but did not set its pass bit.  Its results were nevertheless deterministic:
lane 0 reported `A=3, B=0`, and lane 7 reported `A=0, B=0x1e`.  The remaining
gap is the larger `fffd5fc5`/`fffd2bbd` setup surrounding the vendor's
`0..0x80` even-valued sweep, not basic scan-chain access.

The vendor reference at `192.0.2.203` was probed with the corrected read
command after its vendor boot.  `MC_CONTROL.INIT_DONE` still caused the access
to leave `f8=0x1545` and `fc=0x142004cd` unchanged.  Its QPI status was
`0x070f0f03`; the channel selector was restored to `0x00000001`.  Thus the
post-boot reference cannot supply early PHY values through this interface.

## 2026-09-01 — B06K PHY-aware ROMMON build

```text
test ID: B06K-BUILD-01
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash (B06K raw 4 MiB): 5d74c13f3ee908c185e4bbd1594936f6cd50a5f8ad6b1d90faed7c233589924b
ROM hash (B06K W25Q128 16 MiB): 4dd0747816a9f5206de244888a46bbfc0236f3ab8affd08b3fbe1101d9f7246b
flash chip: intended Winbond W25Q128.V..M, 16 MiB
board revision: not re-recorded
CPU: required first target Intel Xeon E5645
CPU stepping: required CPUID leaf-1 EAX 0x000206c2
microcode revision: no update in B06K
DIMM model: required first target Crucial BLS4G3D1609DS1S00.
DIMM slot: sole SPD responder 0x54 / inferred channel 2
GPU: unchanged; not part of this test
PSU: not re-recorded
boot type: not run on hardware
POST trace: expected inherited B06J trace ending at 0xbc prompt
serial log: expected COM1 0x3f8, 115200 8N1
result: BUILT/STATIC — compile, CBFS construction, hashes, and top placement verified; hardware not run
recovery required: none during build; socketed known-good chip required for first run
notes: no automatic PHY/IMC write at boot; all new write profiles remain explicitly armed commands
```

B06K adds bounded `phyr`/`phyw` primitives and fixed `rdtry`/`rcvtry` channel-2
rank-0 experiments.  Each operation polls the documented command busy bit with
a finite limit.  The profiles contain only descriptors recovered from the
pinned MSI `MINITDLL`; they do not claim established semantic field names.
`rdtry` applies the nine-field value-two pulse state, one rank sweep value, and
the sixteen non-ECC lane seeds before `0x26b01`.  `rcvtry` applies its global
coarse field, the observed channel-2 `MC_CHANNEL_ODT_PARAMS2` low word
`0x0100`, and sixteen lane seeds before `0x27301`.  Completion polling requires both the
command action bit to clear and status bit 8 to assert, so a stale `0x100`
cannot terminate the wait immediately.  Both print all eight lane pairs after
completion.  Neither profile initializes DRAM, sets
`MC_CONTROL.INIT_DONE`, continues to ramstage, or writes SPI flash.

## 2026-09-01 — DX58SO-correlated RCVEN precommand experiment

```text
test ID: B06J-HW-03
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash: running B06J image; hash not re-read during this volatile session
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: not re-recorded
CPU: Intel Xeon E5645
CPU stepping: CPUID leaf-1 EAX 0x000206c2
microcode revision: no update in B06J
DIMM model: Crucial BLS4G3D1609DS1S00.
DIMM slot: sole SPD responder 0x54 / inferred channel 2
GPU: unchanged; not part of this test
PSU: not re-recorded
boot type: same cold-boot B06J session as B06J-HW-02
POST trace: inherited B06J trace ending at 0xbc prompt
serial log: COM1 0x3f8, 115200 8N1; interactive ROMMON transcript
result: PARTIAL PASS — Intel precommand completed, but RCVEN still did not pass
recovery required: full AC removal before the next clean run; no SPI write occurred
notes: volatile channel-2 IMC/PHY writes only; QPI remained operational
```

Intel DX58SO PEI module GUID `63C0690C-5D9E-4EE3-840E-FAE0E76E291A`
(local extracted PE32 SHA-256
`aca71404ccb5eb9df65997ee887914db3231503f0cfb0c0b3b5ca2b06135e4ae`)
contains the same RD `0x26b01` and RCVEN `0x27301` commands as MSI MINIT.
Its RCVEN path writes `0x0100` to the low word of
`MC_CHANNEL_ODT_PARAMS2`, seeds the same per-lane secondary value `0x15e`,
issues FIFO reset `0x20600`, and conditionally issues `0x30200` before
`0x27301`.

On the live MSI board, `MC_CHANNEL_ODT_PARAMS2` already read `0x00000100`.
The additional `0x30200` command self-cleared to `0x00010200` and produced
status `0x00000300`, proving completion bits 8 and 9.  Reapplying all 17 RCVEN
PHY descriptors and then issuing the exact `0x20600`, `0x30200`, `0x27301`
order still ended at command `0x00007300` and status `0x00000100`.  All eight
data lanes read primary `0x011` and secondary `0x1ff`.  CPU-side QPI PHY
status remained `ff:02.1:80=0x030f0f03` throughout.  The uniform saturation
is evidence for a missing global receive/pattern prerequisite, not a lone
lane failure.  The public register correlation identifies offset `0xa0` as
`MC_CHANNEL_ODT_PARAMS2`; the earlier experimental name “training setup” was
therefore corrected.

## 2026-09-01 — B06J exact RD descriptors at sweep zero and UART halt

```text
test ID: B06J-HW-04
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash: running B06J image; hash not re-read during this volatile session
build ID: X58PROE-B06J-CAR-ROMMON-20260831
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: not re-recorded
CPU: Intel Xeon E5645
CPU stepping: CPUID leaf-1 EAX 0x000206c2
microcode revision: no update in B06J
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8 UDIMM
DIMM slot: sole SPD responder 0x54 / inferred channel 2
GPU: unchanged; not part of this test
PSU: not re-recorded
boot type: continuation of the B06J-HW-02/B06J-HW-03 session
POST trace: interactive B06J; likely terminal 0xbe after bounded UART error
serial log: COM1 0x3f8, 115200 8N1 through serial-gateway.example.invalid /dev/ttyUSB1
result: PARTIAL PASS — exact recovered RD descriptors produced lane-specific results; no RD pass
recovery required: endpoint no longer responds; full AC removal is required before another run
notes: volatile CPU-Uncore/IMC/PHY writes only; no SPI write occurred
```

An isolated attempt to change CPU-Uncore register `ff:03.4:0x4c` from
`0x030140e7` to the vendor-observed `0x030100e7` was acknowledged by ROMMON,
but immediate readback remained `0x030140e7`.  CPU-side QPI PHY status stayed
`ff:02.1:0x80=0x030f0f03`.  This establishes only that bit 14 is not directly
writable in the observed state; no semantic name is assigned to it.

The exact MSI coarse-RD descriptor set was then replayed at sweep value zero:
the nine recovered pulse fields used value two/mode one, the global field at
`0x09c7` used zero/mode one, and all sixteen non-ECC lane fields were seeded
with zero/mode zero.  Every scan-chain busy poll completed.  The old B06K
comparison order, FIFO reset `0x20600` followed by rank-0 RD command
`0x26b01`, ended with command `0x00006b00` and status `0x00000100`; the RD
pass bit was clear.  The lane results were:

```text
lane 0  A=0x00  B=0x1f
lane 1  A=0x00  B=0x1f
lane 2  A=0x00  B=0x20
lane 3  A=0x00  B=0x1d
lane 4  A=0x00  B=0x22
lane 5  A=0x00  B=0x21
lane 6  A=0x00  B=0x22
lane 7  A=0x00  B=0x1e
```

The lane-specific secondary values show that the result path distinguishes
lanes, but they do not establish correct sampling windows or usable memory.
No further hardware writes followed.  A subsequently sent unpaced
multi-command SerialICE batch triggered B06J's bounded UART error path; both
a direct serial probe and a fresh 115200-8N1 terminal received no response.
The most likely terminal is the documented `POST=0xbe` safe halt, but the POST
board value was not independently captured.  Complete AC removal is required.

Static reconstruction after this run found that MSI function `fffd5fc5`
does not use the B06K FIFO-reset order.  For each coarse-RD point it calls the
recovered descriptor-seeding function, invokes `fffd2bbd` to perform
`0x20200`, both-rank MRS and ZQCL handling followed by `0x20300`, and then
writes `0x26b01` directly.  B06L is the bounded opt-in experiment for that
new hypothesis; it has not yet run on hardware.

## 2026-09-01 — B06M automated proven base state and exact-RD build

```text
test ID: B06M-BUILD-01
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash (raw 4 MiB): d7b550cee48b4aadd8b80f9141bc7c9eea32c4ac8bb39e2e6a428ff9cd0deacb
ROM hash (W25Q128 16 MiB): 56b8263b408c4c104460141101f7681776a1375ddea52c0fab73c2e5714256b8
flash chip: intended socketed Winbond W25Q128.V..M, 16 MiB
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: exact Intel Xeon E5645 CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: no update in B06M
DIMM model: required Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8
DIMM slot: sole responder at SPD 0x54 / inferred channel 2
GPU: unchanged; not part of this test
PSU: not re-recorded
boot type: not run; first execution requires full AC removal
POST trace: expected inherited monitor entry at 0xbc; no base write at boot
serial log: expected COM1 0x3f8, 115200 8N1 with explicit BASE phases
result: BUILT/STATIC — two byte-identical builds, wrapper and machine data audited; hardware pending
recovery required: none during build; socketed recovery path required for first run
notes: baseinit is armed only; no INIT_DONE, ordinary DRAM, QPI retraining, ramstage, payload or SPI write
```

B06M converts the exact B06J-HW-01 manual base sequence into one fail-closed
monitor command.  Its preflight accepts only the observed cold reset state,
E5645 signature, ratio 6 and Slow-QPI tuple.  The 35-entry policy table and six
pattern buffers match the prior live writes.  Every policy write is read back;
MRS, RCOMP and both ZQCL ranks use bounded polls.  A later static re-audit found
that the serialized UART reset/CKE markers do not explicitly wait for their
final character to reach TEMT, and MRS has only an unmeasured serialized PCI
read as a gap.  B06M was superseded by B06N before hardware execution.

The image retains B06L's `rdexact` and `rdsweep`; the first test must run only
`baseinit` followed by isolated `rdexact 2`.  Hardware success is not claimed.

## 2026-09-01 — B06N explicit timed-MRS baseinit build

```text
test ID: B06N-BUILD-01
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash (raw 4 MiB): afd610d21692be65ce11bfef253ee3318f187208363f173e9e4be2085b6fc696
ROM hash (W25Q128 16 MiB): 1c1f60b9155bf9374f453ce80e3879bfcfc512f698794efa6122d588eb7e895c
flash chip: intended socketed Winbond W25Q128.V..M, 16 MiB
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: exact Intel Xeon E5645 CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: no new update in B06N
DIMM model: required Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8
DIMM slot: sole responder at SPD 0x54 / inferred channel 2
GPU: unchanged; not part of this test
PSU: not re-recorded
boot type: not run; first execution requires full AC removal
POST trace: expected inherited monitor entry at 0xbc; no base write at boot
serial log: expected COM1 0x3f8, 115200 8N1; eight dots after MRS_GAPS
result: BUILT/STATIC — three byte-identical builds, wrapper and machine code audited; hardware pending
recovery required: none during build; socketed recovery path required for first run
notes: baseinit is armed only; no INIT_DONE, ordinary DRAM, QPI retraining, ramstage, payload or SPI write
```

B06N preserves B06M and changes timing only when the separate
`X58_PRO_E_B06N_TIMED_MRS` option is selected.  After each MRS-valid poll has
completed, it transmits one dot and waits boundedly for UART LSR TEMT.  A
115200-8N1 frame takes about 86.8 microseconds, conservatively exceeding the
one-microsecond delay observed in the vendor path without assuming a TSC
frequency.  Reset/CKE phase markers receive the same explicit final TEMT
check.  Machine-code inspection found `TEMT=0x40`, poll limit `0x0f4240`, the
dynamic per-rank `0x282xx` ZQCL command followed by `0x20300`, and direct
`0x26b01` RD.  Hardware success is not claimed.

## 2026-09-01 — CSI caller ratio field and staged QPI policy reconstruction

Static analysis of the hash-pinned MSI `CSI_INITDLL` established that caller
input byte `+0x07` is an operational QPI-ratio selector.  Function
`0xfffe7d85` maps selections 1, 2, and 3 to literal ratios `0x12`, `0x16`,
and `0x18`; `0xfffe7d21` then writes the selected seven-bit value to physical
register offset `0x54` for each discovered link.  The public Xeon 5500
register map identifies that field as `NEXT_PLL_RATIO` in
`QPI_[0,1]_PLL_RATIO`.

The MSI fallback derives the selector from CPU `ff:02.1:53`, the maximum-ratio
byte of `QPI_0_PLL_STATUS`, rather than from its current-ratio byte.  Its
subsequent stages separately set `PH_CTR.LINK_SPEED`, apply the per-link
scrambler policy, disable automatic compliance entry, update the peer-side
link structure, and only later enter trigger-oriented code.  CSI also checks
the existing `PH_PIS.LINKUP_IDENTIFIER` before marking the link active.

This reconstruction agrees with the captured MSI endpoints: the tested Slow
setting has current/next ratio `0x10` and `PH_PIS=0x030f0f03`, while the
tested High setting has ratio `0x12` and `PH_PIS=0x070f0f03`.  It proves a
multi-stage CPU/IOH transition and rules out replaying only the terminal
`PH_CTR` value.  No live register was written and no QPI image was built from
this partial sequence.  Detailed address evidence is retained in
`research/msi/csi-pci-accesses-2026-09-01.md`.

## 2026-09-01 — B06J live base-state replay reaches a stuck DDR3 command

```text
test ID: B06J-HW-02
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash: unchanged B06J image; hash not re-read during this session
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: no update in B06J
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8
DIMM slot: sole responder at SPD 0x54 / inferred channel 2
GPU: unchanged; not part of this test
PSU: not re-recorded
boot type: remote reboot with warm-state residue; no verified full AC removal
POST trace: ROMMON remained responsive; external POST display not independently recorded
serial log: COM1 0x3f8, 115200 8N1 through serial-gateway.example.invalid /dev/ttyUSB1
result: PARTIAL PASS — policy/reset state accepted; first direct MRS2 never issued
recovery required: channel-local reset recovered the DDR3 command latch twice; no full reset yet
notes: volatile CPU-Uncore/IMC/test-engine writes only; no SPI write or ordinary DRAM access occurred
```

The initial live state contained `MC_CONTROL=0x00000040`, so this was not the
cold-reset state required by B06M/B06N.  After clearing that observed
`DIVBY3EN` residue, all 35 recovered one-channel policy values and all six
test-pattern buffers accepted exact readback.  The documented DIMM reset and
CKE actions completed, but writing rank-0 MRS2 command `0x00820000` to
`ff:06.0:0x60` left `MRS_VALID` set across repeated reads.  Channel status
remained zero and Slow-QPI remained `ff:02.1:0x80=0x030f0f03`.

Setting the documented `MC_CONTROL.CHANNELRESET2` bit and then clearing it
reset only channel 2: the stuck command and the channel configuration at
`0x54..0x7c` returned to reset values, while CAR, ROMMON, global IMC state,
test-engine state and Slow-QPI remained live.  The entire channel policy was
then rebuilt with exact readback.  A second attempt used the statically
reconstructed MSI order `fffcc015 -> fffc614b -> fffd2dfd`: `IGNORE_RX`,
`BIOS_RESET_ENABLE`, DIMM-init parameters, DIMM reset, and
`ASSERT_CKE|IGNORE_RX`, without the earlier experimental `BLOCK_CKE` writes.
The same `0x00820000` command again remained stuck.  This excludes UART pace,
the extra CKE sequence, and the recoverable channel-local warm residue as the
immediate cause.

Static re-audit also established that MSI's helper `fffcc67a` constructs the
same `0x00820000` encoding; therefore the failed command was not caused by an
incorrect MRS-valid, bank, rank, or address field.  The vendor function issues
an additional unexplained bank-4 MRS-like command before the ordinary
MRS2/MRS3/MRS1/MRS0 series.  Its configuration-dependent address value and
the earlier stateful clock/command-path prerequisites remain under analysis.
B06N would currently stop at its first MRS-valid poll and is not a useful
flash candidate for this state.

## 2026-09-01 — B06J broad-PHY-prefix SerialICE trials

```text
test ID: B06J-HW-05
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash: unchanged running B06J image; hash not re-read
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: no update in B06J
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8 UDIMM
DIMM slot: sole SPD responder 0x54 / inferred channel 2
GPU: unchanged; not part of this test
PSU: not re-recorded
boot type: continuation of the same B06J session; not a fresh cold boot
POST trace: ROMMON/SerialICE remained responsive
serial log: COM1 0x3f8, 115200 8N1 through serial-gateway.example.invalid /dev/ttyUSB1
result: PARTIAL PASS — fixed broad-PHY writes execute and change lane results; no DDR pass
recovery required: none; every trial restored saved PHY/IMC values; no SPI access
notes: QPI remained ff:02.1:80=0x030f0f03 throughout
```

Static comparison found MSI broad PHY initializer `0xfffca5fa` and its Intel
DX58SO structural counterpart `0xffe5686c` in the locally extracted
`63C0690C...` PEIM.  Their corresponding writers are `0xfffc7e18` and
`0xffe5578c`.  The first four descriptors are common-PHY fields and therefore
use selector 3/read-chain base `0x2fb`; treating them as channel-2 fields was
an initial experimental error.  That mistaken four-write trial produced
readback `[0,7,0,8]`, no RD pass, and restored original channel-2 values
`[0,0,0,8]`.  It caused no QPI change.  Its log is
`/tmp/x58-b06j-b7-prefix-rd20-20260901.jsonl` on serial-gateway.example.invalid.

The corrected common-prefix trial applied `[7,7,10,4]`, produced no RD pass,
and restored four zeros.  Adding all revision-2 common fields applied
`[7,7,10,4,2,33,2,0,0,18,6]`.  A full `0x00..0x80` RD sweep with that common
profile produced no pass and the stable lane tuple `[3,0]` on all eight
lanes.  Its 68-line log SHA-256 is
`98bacf4431c99f64fe7552bb08f3343c16e6e61d024155c8777962fca672a717`.

RCVEN with only the four common-prefix fields returned `[5,365]` on every
lane.  Sweeping all coarse values `1..63` did not change that tuple or produce
a pass.  The restored 67-line log SHA-256 is
`743bc83f8cede2c1d99c3fe4287fb5fe4531b905875cbdc27a7081b95163b86b`.
The full revision-2 common profile instead returned `[3,350]`, where 350 is
the seeded `0x15e` secondary value.  These uniform signatures are evidence
that the engine runs but still lacks later lane setup.

MSI `0xfffc80f9` / Intel `0xffe5861c` exposed one previously omitted channel
field, descriptor `0x0a02/8`.  For the pinned ratio-6/CL6 configuration its
recovered formula yields 3 without channel flag bit `0x20` and 5 with it.
Both single RD trials completed without a pass and restored the original zero:

```text
d0975c71da4c134ff4d0705c7085562b24380c6bf40f54cf98c89f9323cc9834  delay03 RD 0x20
a90aad242515f98cd7bfc53a18747ba123521d812f2fe1321d5cb5fa291bb95b  delay05 RD 0x20
5196275bf41def8534d64a37462bb6b23e263792e74ced2d6ebde448e88c06eb  delay03 RCVEN 0x10
```

Finally, the complete fixed common/channel prefix plus `0x0a02=3` was held
while all 65 even RD points `0x00..0x80` were tested.  Every command completed
at status `0x100`, no pass bit appeared, and all lanes remained `[3,0]`.
The script restored all 12 channel fields and all 11 common fields to their
snapshots; QPI remained `0x030f0f03`.  Consoleio log:

```text
/tmp/x58-b06j-b7-fixed-delay03-rd-full-20260901.jsonl
SHA-256 6bffce99261fc589825b8a46a6f432a3d0a0ecfaf9b88c4cb6f71a03b2c8a97b
```

No trial set `MC_CONTROL.INIT_DONE`, accessed ordinary DRAM, changed SPI, or
claimed usable memory.  The next hypothesis is the dynamic per-DIMM/per-lane
body beginning near MSI `0xfffcaf77` and Intel `0xffe57772`; additional blind
coarse sweeps are not justified before reconstructing that body.

## 2026-09-01 — Intel MRS correction and later training-phase probes

```text
test ID: B06J-HW-06
build commit: coreboot fe3e08197177 plus local experimental X58 changes
ROM hash: unchanged running B06J image; hash not re-read
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645
CPU stepping: CPUID leaf-1 EAX 0x000206c2, stepping 2
microcode revision: no update in B06J
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8 UDIMM
DIMM slot: sole responder at SPD 0x54 / inferred channel 2
GPU: unchanged; not part of this test
PSU: not re-recorded
boot type: continuation of the same B06J session; not a fresh cold boot
POST trace: ROMMON/SerialICE remained responsive
serial log: COM1 0x3f8, 115200 8N1 through serial-gateway.example.invalid /dev/ttyUSB1
result: PARTIAL PASS — corrected sequence and four bounded phase actions complete; no DDR pass
recovery required: none; saved PHY state restored, QPI unchanged, no SPI access
notes: no INIT_DONE or ordinary DRAM access; no flash image was installed
```

Intel DX58SO function `0xffe37125` establishes that its CKE path proceeds
directly through MRS2/MRS3/MRS1/MRS0.  It has no counterpart of MSI's
conditional bank-4 call.  Reinspection of MSI `0xfffd2bbd` showed that the
extra command's low word is derived as `0x2000 | table_byte` from a runtime
15-row DIMM table.  The experimental constant `0x3308` cannot follow from
that code and is withdrawn.  `MC_CHANNEL_DDR3CMD` retaining bit 23 is also
normal: successful vendor captures retain it, so the earlier interpretation
of that bit as a stuck completion latch was incorrect.  The channel resets
remain historical experiments, but they are not evidence of a hung MRS FSM.

The fixed common and channel-2 profile, including `0x0a02=3`, was reapplied
while the Intel MRS sequence preceded every even RD value `0x00..0x80`.
All 65 actions completed with status `0x100`, no pass bit, eight identical
lane tuples `[3,0]`, and QPI `0x030f0f03`.  All 12 channel and 11 common
fields restored to their snapshots.  Because the older JSON schema did not
record whether bank 4 was issued, the deterministic 70-line output is
byte-identical to the earlier failed run and shares SHA-256
`6bffce99261fc589825b8a46a6f432a3d0a0ecfaf9b88c4cb6f71a03b2c8a97b`.

The Intel main path statically orders stage commands as RD `0x26b01`, RCVEN
`0x27301`, write-level `0x25b01`, then write-DQ/DQS `0x23b01`.  Isolated,
bounded probes of the last two commands both self-cleared and returned
status `0x100`; neither expected pass bit appeared, and QPI remained stable.
Their serial-gateway.example.invalid log is `/tmp/x58-b06j-intel-late-phases-20260901.jsonl`,
SHA-256 `219607502ad91bcf0bb9867150512fba0533f4aee37fe15c754a7ae76e59cef1`.
All four phase families therefore share a missing earlier prerequisite.  The
next justified target remains the dynamic per-DIMM/per-lane body beginning at
MSI `0xfffcaf77` / Intel `0xffe57772`, not another direct-command sweep.

## 2026-09-01 — B06V0 guarded local vendor-assisted ROMMON build

```text
test ID: B06V0-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (proprietary-free base 4 MiB): 2e004e16e0f7e3c0d749df51567252ad7f3f2df3a26973aefdcee8d9d9b66388
ROM hash (local MSI composite 4 MiB): 66cf3c63ee3a696c9250b2a31225933c64d9c17837983ced3381e6e8082d628b
ROM hash (local W25Q128 16 MiB): ba4ec305f3b437e9325e5d557f5ec23d861f54d58ba1251f5610802980f1dc96
flash chip: intended socketed Winbond W25Q128.V..M, 16 MiB; no chip programmed in this build session
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: exact Intel Xeon E5645 CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: pre-RAM update required and gated at 0x0000001f; blob SHA-256 6d39860414c836e949761ae152c8cf53da9519ffc0636efbacd5800eb3fa6cf8
DIMM model: required Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: required exact sole responder at SPD 0x54; full 0x50..0x57 topology scan implemented
GPU: unchanged; not part of the first test
PSU: not re-recorded
boot type: not run; first hardware execution requires true AC removal
POST trace: expected passive entry bc; d0 only after armed vprep; d1/d2 CSI call/return; d3/d4 MINIT call/return; df rejection
serial log: expected COM1 0x3f8, 115200 8N1; no B06V0 hardware log exists
result: BUILT/STATIC — two clean byte-identical base builds; four local ranges and both composite layouts read back and verified
recovery required: none during build; socketed vendor recovery and external cold reset required before CSI/MINIT testing
notes: no image was flashed; no vendor entry executed; no ordinary DRAM, 1-MiB handoff, ramstage, payload, or SPI write occurs in the B06V0 monitor path
```

B06V0 is behind the default-off `X58_PRO_E_B06V0_VENDOR_INIT` option. The
public base contains no MSI bytes. The local composer verifies the full
`A7522IMS.8F0` hash and inserts the audited CSI wrapper, its CMOS helper,
`MINITDLL` and the authoritative `0x75e0`-byte `CSI_INITDLL` only into erased
ranges at their original XIP addresses. The older `0x75c0` CSI repack is
rejected.

The runtime uses pre-RAM microcode revision `0x1f`, exact CPU/BSP/APIC and
execution-mode gates, complete-range FNV fingerprints backed by offline
SHA-256 provenance, five CAR canaries and a private `0x7fc0`-byte vendor
stack. `spd` now rejects every topology except exact ACK/DDR3 maps `0x10`,
hashes all 256 bytes, and `vprep SPD_FNV` requires the operator to echo that
fingerprint. Before programming PCIEXBAR from zero to `0xe0000001`, `vprep`
revalidates the runtime, CAR and complete code ranges. CSI and MINIT remain
separate physical one-attempt calls with exact cdecl return-stack checks. CSI
acceptance additionally requires the operator to echo the complete state
digest, but this does not establish a success code. Policy reset/edit/install
are separately armed; installation requires and reads back its full digest.
MINIT's four accepted-return observations plus input/current policy and full
workspace digests are reported only as a DRAM-untested candidate; the image
never accesses ordinary memory. Any generic monitor write dirties and closes
the vendor path until a cold reset.

Nine extraction/composition regression tests, Python compilation, two clean
coreboot builds, CBFS inspection, CAR symbol inspection, composite range
verification and W25Q128 top-alignment comparison passed. Hardware success is
not claimed. The first test is limited to `id`, `resetcause`, `spd` and
`vinfo`; a second cold test may add only armed `vprep` with the reviewed SPD
digest. CSI/MINIT require
separate reviewed logs and recovery readiness. Exact commands and risks are
recorded in `Documentation/b06v0-vendor-rommon.md`.

## 2026-09-01 — B06V1 transactional register-script ROMMON build

```text
test ID: B06V1-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (proprietary-free base 4 MiB): 9a5a1d3d93a6e35180b8bfdd97d556d6149ca3aaa177e20a3cef071ad2775452
ROM hash (local MSI composite 4 MiB): 5b19653383bf224d6ea97ba3e46ff441257a59e92ad38d63171bf27875ee3ca0
ROM hash (local W25Q128 16 MiB): 39614719b49502e374536ee92f97eb087a622d8bb27dfcb58c6fdb2a325eb4a8
flash chip: intended socketed Winbond W25Q128.V..M, 16 MiB; no chip programmed in this build session
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: exact Intel Xeon E5645 CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: pre-RAM revision 0x0000001f required; CBFS object hash 6d39860414c836e949761ae152c8cf53da9519ffc0636efbacd5800eb3fa6cf8
DIMM model: required Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: required exact sole responder at SPD 0x54
GPU: unchanged; not part of the first test
PSU: not re-recorded
boot type: not run; first hardware execution requires true AC removal
POST trace: expected passive entry bc; e5 script PRE; e6/e7 failed/successful run; e8 rollback PRE; e9/ea successful/failed manual rollback
serial log: expected COM1 0x3f8, 115200 8N1; no B06V1 hardware log exists
result: BUILT/STATIC — two clean byte-identical builds; host engine/compiler tests and both local composite layouts verified
recovery required: none during build; socketed vendor recovery and external cold power path required before hardware execution
notes: no image was flashed; no script or vendor entry executed; no ordinary DRAM, ramstage, payload, or SPI write occurs automatically
```

B06V1 adds a format-version-1 table interpreter behind the default-off
`X58_PRO_E_B06V1_REG_SCRIPT` option. It accepts at most 32 validated operations
over I/O, PCI configuration, aligned MMIO and MSRs. Tables have an exact
canonical FNV, must be sealed before execution, and require a separately armed
`SCRIPT` command. Every operation prints and drains a PRE record before access,
then stores a returned POST trace. Poll and pause loops are bounded, and
scripted writes into the entire CAR range are rejected.

Write/mask operations always snapshot the prior value. Only mutations declared
`rev` can be rolled back. Rollback replays performed writes in reverse order,
reads each value back, retains failed recovery state for another explicitly
armed attempt, and counts attempts. `auto` rejects any `nr` table before access
and rolls back only after execution failure, never after success. These
mechanisms cannot undo W1C/trigger/reset/training/link side effects or recover
from a stopped CPU, fault, reset, lost QPI, or dead UART.

The portable C tests cover validation, the full 32-entry bound, true sealing,
digest mismatch rejection, mask execution, bounded poll, delay, event failure,
reverse-order rollback, automatic rollback after assert/read-back failure,
non-reversible rejection, rollback failure, updated transaction digest, and a
successful retry. They passed `-Wall -Wextra -Werror` and Valgrind. Seven Python
compiler tests, nine existing pinned vendor tests, bytecode compilation,
checkpatch, and whitespace checks also passed. The C/Python canonical digest
vector is `2bf2e012`; the seven-operation passive hardware example is
`f9482511`.

The final script state is `0xb28` bytes at `0xfff84f60`; the existing vendor
scratch begins at `0xfff85b80` and retains a guarded private stack of `0x7480`
bytes, above the `0x6000` minimum. Both 4-MiB base and composite parse with the
same CBFS. The local 16-MiB file is exactly 12 MiB erased fill plus that
composite at the top; no differing byte between public base and local composite
lies outside the four audited MSI ranges.

The first test is B06V1-P and remains fully passive. A separate cold boot may
then perform B06V1-R using only
`research/scripts/b06v1-passive-state.xrs`: compare both program digests, run
the read/assert table in `keep` mode, archive all seven trace entries and the
runtime transaction digest, then explicitly discard it. No write or rollback
is part of those first two milestones. The complete protocol and recovery
limits are in `Documentation/b06v1-register-scripting.md`.

## 2026-09-04 — B06V1 transactional ROMMON hardware exercise

```text
test ID: B06V1-HW-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (intended local W25Q128 16 MiB): 39614719b49502e374536ee92f97eb087a622d8bb27dfcb58c6fdb2a325eb4a8; programmer readback not captured in this session
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f; independently sampled through IA32_BIOS_SIGN_ID
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; ACKMAP/DDR3MAP 0x10/0x10
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: not independently established; do not count as a confirmed cold boot
POST trace: CAR ROMMON remained interactive throughout; script PRE/POST records captured
serial log: local-only blobs-local/msi-x58-pro-e/b06v1/2026-09-04-b06v1-rommon-hardware.txt, SHA-256 f0343e5be1fa5433c99488e4c1f7afc71ae0ca7fdfdb7e270467a65b1a4c2006
result: PASS for the tested script-engine paths; vendor CSI/MINIT not called
recovery required: none; board and COM1 remained responsive
notes: all retained transactions were explicitly discarded before disconnecting; final scripted CAR value remained deadbeef
```

The passive gate identified the exact E5645, microcode `0x1f`, complete
256-byte SPD fingerprint `fb66b530`, and the previously observed Slow-QPI
tuple `030f0f03/0a000006/00000006`. The seven-operation read/assert example
sealed with the host-predicted `PROGRAM_FNV=f9482511`, completed with no
mutation and retained `TXN_FNV=d7462413`. An unarmed discard and an armed
discard with the wrong transaction digest both failed closed; the exact armed
discard succeeded.

The documented 16550 COM1 Scratch Register at `0x3ff` provided a reversible
software-only target. The manual test sealed as `1eb35643`, changed the byte
`00 -> 5a -> 55`, then rolled both writes back in reverse order with verified
observations `5a` and `00`. The transaction digest changed from `f432ec4d`
to `d33d5e16`, with `RB_TRIES=1` and `ROLLED_BACK=1`. The separate automatic
test sealed as `9c473fee`; its deliberately false assertion produced
`assert-failed`, automatically restored `a5 -> 00`, and retained transaction
`5112d646`. An `auto` request for a non-reversible table (`01c8a78a`) and a
wrong program digest were rejected before any access.

Additional bounded paths also behaved as designed. A read-only poll table
(`10744cdf`) exhausted exactly `0x10` reads and returned `poll-timeout` with
`MUT=0`. A 64-bit read of MSR `0x8b` (`7df06628`) returned
`0000001f:00000000`. Finally, the platform CAR guard rejected an attempted
`fff80000: deadbeef -> 01234567` store (`5e82e0f0`) as `write-failed` with
`WROTE=0`, `MUT=0`; an independent read still returned `deadbeef`.

`vinfo` also reported `SIG WRAPPER=01 CSI=00 MINIT=01`. This was traced to a
firmware-side validation bug, not differing module bytes: B06V1 expected the
CSI DOS-header `e_lfanew` to be `0x80`, while both the active ROM dump and the
hash-pinned local CSI image contain `0xb0`. The source now uses the measured
`0xb0` and the extraction regression pins both PE-header offsets. That fix
compiled and passed the vendor-blob and ROMMON host suites, but it is not part
of the already-flashed B06V1 image. No CSI, MINIT, QPI, DDR, or IMC write was
attempted during this session.

The byte-exact transcript remains under the ignored `blobs-local/` tree
because it includes short diagnostic dumps from the proprietary CSI image.
The public record above retains the relevant addresses, hashes, decoded PE
metadata, commands, results, and transaction identifiers without copying those
vendor bytes.

## 2026-09-04 — B06V2 corrected CSI-gate image build

```text
test ID: B06V2-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (proprietary-free base 4 MiB): e3dfdf071180616b70a3098af9c0aee9a126cb71cb132a3efc9c0533e16aac7a
ROM hash (local MSI composite 4 MiB): 55bf99fae0de990563823caa66ac49728e57a6d44b75c2e9b0ce3ae9ba3e2ec8
ROM hash (local W25Q128 16 MiB): e77f087a27729cce3cfdcf3adf9f73f46e1066013faf841ab43ede490166c4ea
flash chip: intended socketed Winbond W25Q128.V..M, 16 MiB; not programmed in this build session
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0
CPU: exact Intel Xeon E5645 CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: pre-RAM revision 0x0000001f required
DIMM model: required Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: required exact sole responder at SPD 0x54
GPU: unchanged; not part of the first test
PSU: not re-recorded
boot type: not run
POST trace: expected passive CAR ROMMON at bc; no automatic vendor/script call
serial log: expected COM1 0x3f8, 115200 8N1; distinct B06V2 ID
result: BUILT/STATIC — two clean builds byte-identical; corrected compiled CSI gate and complete local flash layout verified
recovery required: none during build; socketed known-good image remains the recovery path
notes: no hardware claim; first test is passive B06V2-P only
```

B06V2 keeps the hardware-tested B06V1 monitor and changes the failed CSI PE
validation argument from `0x80` to the observed `0xb0`. The final romstage
disassembly passes immediate `0xb0` to `pe32_header_valid`; the local composite
contains `e_lfanew=0xb0` and a PE signature at that offset. Ten vendor tests,
seven compiler tests, the portable C engine suite, two complete ccache-free
builds, CBFS parsing, four-range read-back, allowed-difference audit, and
W25Q128 top placement passed. No CSI or MINIT entry was executed.

## 2026-09-04 — B06V2 passive CSI-gate hardware test

```text
test ID: B06V2-HW-P-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (intended local W25Q128 16 MiB): e77f087a27729cce3cfdcf3adf9f73f46e1066013faf841ab43ede490166c4ea; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; ACKMAP/DDR3MAP 0x10/0x10
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: system already active at connection; cold/warm provenance not independently established
POST trace: CAR ROMMON remained responsive; passive command path only
serial log: normalized local-only command transcript at blobs-local/msi-x58-pro-e/b06v2/2026-09-04-b06v2-passive-hardware.txt, SHA-256 adfae711ce9df0278d777d0f2c1c7fc81251fa46f506c79888c94f41187a35c1
result: PASS ONCE for B06V2 passive CSI signature/runtime gate; not counted as a confirmed cold boot
recovery required: none
notes: no vprep, register script, generic write, reset, CSI call, or MINIT call was issued
```

The monitor identified the distinct B06V2 build and reproduced CPUID
`000206c2`, microcode `1f`, full-SPD FNV `fb66b530`, and the established
Slow-QPI/ratio-6 tuple `030f0f03/0a000006/00000006`. The full SPD read and
double-read verification passed with sole address `0x54`. Most importantly,
four `vinfo` commands returned
`PROBE STATUS=ok CODE=00` and
`SIG WRAPPER=01 CSI=01 MINIT=01 CANARY=01`, directly confirming the corrected
CSI `e_lfanew=0xb0` runtime gate on hardware. Three complete `spd` commands in
the same boot state also returned identical bytes, fingerprints, topology,
decoder output and `RESULT=b7`; their later PRE samples retained idle status
and pins while `CTL` reflected the completed prior transaction as `0x08`.

The phase record remained entirely clean: CSI and MINIT armed/attempted/
returned fields were zero, `ROMMON_DIRTY=00`, and the script table contained
zero operations with no valid transaction. PCIEXBAR was already
`00000000:e0000001`, but `PCIEXBAR_STEP` deliberately remained `PENDING`
because the first-test contract excludes `vprep`. This result establishes
blob visibility and validation only; it does not establish the CSI/MINIT ABI,
a successful vendor return, QPI training, DDR training, or usable DRAM.

## 2026-09-04 — B06V2 cold `vprep` and first isolated CSI call

```text
test ID: B06V2-HW-CSI-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (intended local W25Q128 16 MiB): e77f087a27729cce3cfdcf3adf9f73f46e1066013faf841ab43ede490166c4ea; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f before CSI
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; ACKMAP/DDR3MAP 0x10/0x10
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: cold boot confirmed by the user
POST trace: passive ROMMON; d0 after vprep; d1 before CSI; reset; c3 bootblock handoff; later terminal code not directly captured
serial log: local-only blobs-local/msi-x58-pro-e/b06v2/2026-09-04-b06v2-csi-reset-hardware.txt, SHA-256 f174c4079d64c87863c5720116388fec255d9fc5eea9bb885b79720bb8926499
result: vprep PASS; CSI seed 00 entered and caused a reset without returning; reboot reached romstage log but not ROMMON
recovery required: external cold reset required to clear the retained post-CSI state
notes: MINIT, policy acceptance, generic writes and register scripts were not attempted
```

The confirmed cold run reproduced the complete passive B06V2 gate. Armed
`vprep fb66b530` returned `PCIEXBAR_PREFLIGHT STATUS=ok CODE=00` and
`PCIEXBAR STATUS=ok CODE=00`; PCIEXBAR was already `e0000001`, so no relocation
occurred. The following `vinfo` retained all signature/canary bits, CPU and
Slow-QPI/ratio-6 fields, `ROMMON_DIRTY=00`, and changed only the logical
`PCIEXBAR_STEP` result from `PENDING` to `PASS`.

The separately armed `vcsi 00` passed its final `CSI_ARM` check, emitted the
call boundary for wrapper `fffc04e2` and entry `fffe7000`, and did not emit a
`d2` return. Instead, the board reset and printed the coreboot bootblock and
romstage-start messages. No further serial output followed. This is the first
real execution evidence for the MSI CSI wrapper and proves that its first
phase reaches a reset-producing path; it provides no return tuple or state
digest and does not yet prove successful QPI training.

The post-reset stop is narrowly explained by verified coreboot control flow.
B06V2's inherited B04 preflight accepts I801 only at cold defaults
`BAR4=00000001`, `HOSTC=00`, `CMD=0000`. The pre-CSI boot had intentionally
configured and verified `BAR4=00000401`, `HOSTC=01`, `CMD=0001`. A reset
retains that exact legal end state, so the next romstage reaches its initial
note and then deliberately halts at `POST_B04_SMBUS_CONFIG_ERROR` before the
ICH10 report. The terminal POST value is inferred from source until captured
from the diagnostic board. A follow-up must accept only either the cold tuple
or this exact configured tuple, leave all other values fail-closed, and log
which path was used before permitting a second CSI phase.

The early bootblock banner after the CSI reset still identified B06L even
though the coreboot romstage version string identified B06V2. This is a build
identity bug in the inherited bootblock banner, not evidence that another ROM
executed; the next build must give both stages the same unique identity.

The user then confirmed that an external cold boot completed again. No new
serial transcript or programmer read-back accompanied that recovery report,
so it is evidence that the failure is warm-state-specific, not another full
validation repetition.

## 2026-09-04 — B06V3 exact CSI warm-resume build

```text
test ID: B06V3-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (public 4 MiB base): 9af09172bafecda09df18d91e2857c70656ecab10e87ce38602132dd9ff2ee25
ROM hash (local 4 MiB composite): 48d90c834be6854ff9f5d5cc4838d68c3a22fc521c955977be9bc07669b7e0ef
ROM hash (local W25Q128 16 MiB): 1d388fcdb4c0ab697d83d467e386e0c4d37c0a75c84a572b9de80b338e469be8
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: expected 0x0000001f before vendor call
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: build only; hardware execution pending
POST trace: expected cold c4/c5/c6; after CSI reset same path instead of terminal f3
serial log: expected COM1 0x3f8, 115200 8N1; distinct B06V3 identity in both bootblock and ROMMON
result: two ccache-disabled builds byte-identical; host/static/composition checks pass
recovery required: none during build; socketed known-good B06V2/vendor chips remain the hardware recovery path
notes: no automatic script, CSI, MINIT, memory access, or payload
```

B06V3 changes one hardware decision only. Romstage accepts the original cold
I801 state exactly as before, or the exact already-configured state observed
immediately before the B06V2 CSI call: `BAR4=00000401`, `HOSTC=01`, and PCI
command `0001`. Any mixed, relocated, partially enabled, or otherwise unknown
tuple still stops at `POST_B04_SMBUS_CONFIG_ERROR` (`f3`). Existing ICH10 BAR
preflight and complete post-write read-back remain active. The chosen entry
path is printed as `COLD_DEFAULT` or `CONFIGURED_AFTER_RESET`.

The final linked disassembly contains mutually exclusive comparisons for
`00000001/00/0000` and `00000401/01/0001`, with all other paths converging on
`f3`. It also contains the corrected B06V3 identity in bootblock, ICH report,
ROMMON banner, and `id` response; the inherited B06L header is gone from the
selected path. Two complete `CCACHE_DISABLE=1` builds produced the same
4-MiB base hash. Ten pinned vendor-image tests, seven script-tool tests, and
the portable C script-engine regression suite passed.

Local composition reinserted and read back only the four hash-pinned MSI
ranges. The CSI DOS header retains `e_lfanew=0xb0`, the PE signature remains at
loaded offset `+0xb0`, the lower 12 MiB of the W25Q128 image are all `ff` with
SHA-256 `6747318c...63a7c4`, and its top 4 MiB equal the local composite byte
for byte. Hardware must first prove the cold `COLD_DEFAULT` path passively,
then execute `vprep` and one isolated `vcsi 00`; success for this build means
the reset reaches `CONFIGURED_AFTER_RESET` and a fresh ROMMON prompt. A second
CSI call is a separate experiment, and MINIT remains blocked until its result
is captured.

## 2026-09-04 — B06V3 CSI reset continuation and return

```text
test ID: B06V3-HW-CSI-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (intended local W25Q128 16 MiB): 1d388fcdb4c0ab697d83d467e386e0c4d37c0a75c84a572b9de80b338e469be8; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; ACKMAP/DDR3MAP 0x10/0x10
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: newly flashed/cold start reported by user; then one CSI-produced reset
POST trace: d1 before first CSI; reset; B06V3 warm continuation; d1/d2 around second CSI
serial log: local-only blobs-local/msi-x58-pro-e/b06v3/2026-09-04-b06v3-csi-return-hardware.raw.txt, SHA-256 54be197ca018921e0f10de196308db4fa9af01720035ae1e23b696b6ffa52c78
result: exact retained-I801 continuation and CSI return boundary passed once
recovery required: none during this sequence
notes: no MINIT call and no ordinary DRAM access
```

The first armed CSI call reset as in B06V2. The next romstage identified the
exact retained state `BAR4=00000401 HOSTC=01 CMD=0001`, logged
`ENTRY=CONFIGURED_AFTER_RESET`, and reached a fresh B06V3 ROMMON. Its passive
CPU, SPD, PCIEXBAR, QPI and memory-ratio observations matched the pre-reset
state. After repeating the gates and `vprep`, a second armed `vcsi 00` returned
with `EAX=2`, `EBX=2`, `ECX=0x106`, `EDX=EDI=0xfff8fcd8`,
`EFLAGS=0x86`, `VESP=0xfff8ffe4`, private-stack high-water `0x454`, and
complete CSI-state FNV `c4eab5a4`. All code signatures and CAR canaries passed.
The complete `0x304` state was dumped and the exact tuple/digest accepted.

This is the first real return from the MSI CSI path. The semantic meaning of
return value two remains unknown, and the unchanged `QPI80=030f0f03` is only
an observation. It does not prove new QPI training, memory initialization, or
usable DRAM.

The old `vpolicy reset` then failed closed. The initial ad-hoc read reported
standard CMOS data `0x62`; B06V4 later corrected that selector-confused value
to standard diagnostic `0x6c` and corrected extended `0x88` from `0x67` to
`0x6f`. The selector-controlled extended values are
`81/82/88/89/8e/ca/f1/f5 = 19/f7/6f/cb/6c/62/91/e9`, and byte `0x00` at
`PMBASE+0x38`. A sealed 16-operation register script reproduced all eight
extended reads, then rolled every reversible port-`0x72` selector write back
in reverse order. Program/transaction/final digests were
`54538d2c/d0cd7a2c/6c3c2814`. Generic script execution deliberately set
`ROMMON_DIRTY=1` and closed the vendor gate for the remainder of the boot.

## 2026-09-04 — B06V4 corrected policy-input build

```text
test ID: B06V4-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (public 4 MiB base): c8e3522f7c7ea41b76f944ca3e64aa58561951b0128fa8a0b6c513d21b9d0507
ROM hash (local 4 MiB composite): db6726077d968d27341e8d68799891724bdaa8864c4538bf476c6c8c9c077438
ROM hash (local W25Q128 16 MiB): c2cf7eed56db35916515fd7c759201730169ca9d839db42de25b8adf6736127a
flash chip: target socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: expected 0x0000001f before vendor call
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: build only; hardware execution pending
POST trace: no new automatic phase; terminal CAR ROMMON
serial log: expected COM1 0x3f8, 115200 8N1; distinct B06V4 identity and vinputs
result: two ccache-disabled builds byte-identical; host/static/composition/placement checks pass
recovery required: none during build; socketed recovery remains required for hardware trials
notes: policy generation remains manual; no automatic CSI, MINIT, script, DRAM, or payload
```

Full disassembly of the original MSI MINIT wrapper separated standard CMOS
diagnostic `0x0e` from extended CMOS `0x8e`. Standard diagnostic bits 7:6
select whether the wrapper runs its conditional CMOS-policy block. Extended
`0x8e` is separately bounded to `0x30` and used to derive policy byte `0x02`.
The wrapper reads the low byte at `PMBASE+0x38`, identified as ICH10
`ALT_GP_SMI_EN`, and consumes bits 6:4. B06V4's `vinputs` reports those inputs,
restores selectors, and writes no CMOS data.

For the B06V4-observed invalid diagnostic `0x6c`, B06V4 follows the MSI default
branch: policy fields `0x01`, `0x04`, `0xbc..0xbd`, and `0xc3..0xdd` remain
zero, while extended `0x8e=0x6c` is internally bounded to `0x30`. A valid
diagnostic remains restricted to the only captured `ALT_GP_SMI_EN[6:4]=1`
profile. The original optional ROM-object lookup at policy offset `0x46` is
not reconstructed and remains zero.

Both clean builds produced identical base bytes. The final CBFS contains the
expected microcode and romstage; the script engine C regression, seven Python
script-tool tests, ten pinned vendor-image tests, passive script validation,
and the standalone CMOS helper's warning-clean static compilation passed.
All four local vendor ranges passed exact composer read-back. The 16-MiB image
has 12 MiB of leading `0xff`, and its top 4 MiB equal the local composite.
Hardware must stop after `vinputs`, corrected `vpolicy reset`, and the complete
policy dump; installation and `vminit` require a later review.

## 2026-09-04 — B06V4 direct MINIT call and exact B3 early return

```text
test ID: B06V4-HW-MINIT-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (local W25Q128 16 MiB): c2cf7eed56db35916515fd7c759201730169ca9d839db42de25b8adf6736127a; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; ACKMAP/DDR3MAP 0x10/0x10
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: B06V4 active after newly flashed start reported by user; sequence includes the expected CSI-produced reset/continuation
POST trace: d1 before first CSI/reset; configured-I801 continuation; d1/d2 around returning CSI; d3/d4 around returning MINIT
serial logs: research/msi/captures/2026-09-04-b06v4-vendor-ratio6-policy-build.raw SHA-256 5adf0efd2c0e045ff5b95165f456392bc817153d61da8b55601481aa8c1dfc70; research/msi/captures/2026-09-04-b06v4-vendor-ratio6-minit-trial.raw SHA-256 411297d28ca27f2ed795ed62b9850703d067be934709fcb4953bdbb599679410
result: MINIT outer entry returned, but full workspace and static control flow prove an early return after internal phase B3; no DRAM training success
recovery required: none during the returning sequence; full AC-off/socketed known-good chip remain the recovery path
notes: no ordinary DRAM access, ramstage, payload, generic write, or register script was executed
```

B06V4's selector-controlled telemetry corrected the earlier CMOS read:
standard diagnostic `0x0e=0x6c`, extended `0x8e=0x6c`, extended
`0xca=0x62`, extended `0x88=0x6f`, and `ALT_GP_SMI_EN_LOW=0`. The monitor
made no CMOS data writes.

After the usual first CSI call/reset and configured-I801 continuation, the
second CSI call returned the known tuple `EAX/EBX/ECX=2/2/0x106`. Its complete
state FNV was `a3736e20`; this differed from the earlier `c4eab5a4`, so the
state was dumped and accepted by its actual digest. The generated policy was
then changed only at neutral offsets `04/08/24/25/bc/d9/db` to values
`01/01/80/04/85/0a/05`. The complete `0xe0`-byte candidate reconstructed to
FNV `2e0ccce9`, with `policy[0x0a]=2` and little-endian flags
`policy[0x24..0x27]=0x01060480`. Installation and runtime read-back matched.

The direct MINIT call returned `EAX=0`, `workspace[1]=0`,
`workspace[2]=0`, and changed `policy[0x0a]` to zero. Its complete workspace
was captured without gaps: FNV `267b2858`, reconstructed binary SHA-256
`63f26e92dcd55675731d00a94b5ea0718a421b94010313bbbf2b065c47b0817d`.
The earlier four-gate implementation labeled this a candidate; the complete
data disproves full initialization. `workspace[0x4f]=0x06` and the normal
completion marker `workspace[0xe79]=0`.

Static `MINITDLL` control flow shows that internal phase B3 at `0xfffc8781`
returns 1 if workspace byte 2 is nonzero or workspace byte `0x4f` bit 2 is
set. The caller around `0xfffd8bf5` skips phases B4-B8 on that result. Thus the
observed set bit explains the exact path; the outer zero return does not mean
memory was trained. B2 did scan the SPDs and correctly record only channel 2,
dual-rank x8 geometry, and workspace DOD `0x02ac`, proving that the SPD and
topology handoff reached vendor code.

Post-call read-only Uncore captures remained untrained: `FF:03.0:60=0`,
`FF:03.4:f8=0`, and the channel rank/map registers remained zero or reset-like.
See `research/msi/b06v4-minit-b3-return-2026-09-04.md` for capture hashes and
the isolated next hypothesis.

## 2026-09-04 — B06V5 exact cold-MINIT policy gate build

```text
test ID: B06V5-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (public 4 MiB base): 7adb771380b0849929534ce0930f90bde8f37434c079329591b4cf1de3d81d59
ROM hash (local 4 MiB composite): 08fa307d9f1867b51d8a384e1cbc06950d4e3f0882ed3a7b2dc9b03af8cb4556
ROM hash (local W25Q128 16 MiB): e2a24406b3007eae52fcb76653284873e25315047e9faa2100ce14c5763574cf
flash chip: target socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: expected 0x0000001f before vendor call
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: build only; hardware execution pending
POST trace: no automatic phase; expected d1/reset/d1/d2 CSI sequence followed by separately armed d3/d4 or exact failure
serial log: expected COM1 0x3f8, 115200 8N1; distinct B06V5 identity and post-MINIT completion telemetry
result: two fixed-epoch ccache-disabled builds byte-identical; host/static/policy/composition/placement checks pass
recovery required: none during build; socketed B06V4/vendor recovery remains required for hardware trial
notes: no automatic CSI, MINIT, script, DRAM, ramstage, or payload action
```

B06V5 tests only the status-derived B3 skip. The new separately armed
`vpolicy cold` command refuses the untouched generated policy and every
unknown modified policy. It requires the exact already-returning candidate
FNV `2e0ccce9`, `policy[0x0a]=2`, flags `0x01060480`, and accepted CSI tuple
`2/2/0x106`. It invalidates any installed copy, sets the status byte to zero,
clears only flags bit 18, and verifies final FNV `3c0f3a0b` with flags
`0x01020480`. Policy installation and MINIT remain separate armed operations.

After a return the monitor adds the decisive `workspace[0x4f]` and
`workspace[0xe79]` gates plus read-only Uncore mapper/completion/channel
telemetry. It never touches ordinary DRAM. `FULL_PATH_RETURN_DRAM_UNTESTED`
therefore means only that B4-B8 returned and the marker was set; actual memory
access is deliberately deferred.

The independent host reconstruction reproduced both policy FNVs. Ten pinned
vendor tests, seven script-tool tests, the portable C script suite, source
whitespace checks, compiled identity/string checks, CBFS parsing, exact
four-range vendor read-back, and W25Q128 top placement passed. Two clean builds
with `SOURCE_DATE_EPOCH=1788523200` produced identical ROMs and stage ELFs.
The lower 12 MiB are all `0xff` with SHA-256 `6747318c...63a7c4`; the top
4 MiB equal the local composite byte-for-byte.

## 2026-09-04 — B06V5 full MINIT return and UC-DRAM address tests

```text
test ID: B06V5-HW-MINIT-DRAM-01
image ID: X58PROE-B06V5-COLD-MINIT-GATE-ROMMON-20260904
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (intended local W25Q128 16 MiB): e2a24406b3007eae52fcb76653284873e25315047e9faa2100ce14c5763574cf; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; ACKMAP/DDR3MAP 0x10/0x10
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: CF9 full-reset continuation followed by the expected CSI-produced reset; not a verified AC-cold boot
POST trace: d1/reset; CONFIGURED_AFTER_RESET; d1/d2 returning CSI; d3/d4 returning MINIT; interactive bc ROMMON afterward
serial log: local-only blobs-local/msi-x58-pro-e/b06v5/2026-09-04-b06v5-full-live-through-walking-zero.raw, SHA-256 673330d8ce196f9d29582ededa60b126dc4f971aa74d6c0a5ee88513f4ec6db2
result: PASS ONCE for the full MINIT B4--B8 return path and the listed UC-DRAM smoke/address/walking-one/walking-zero tests
recovery required: none; every modified DRAM dword was restored and independently checked
notes: no SPI write, post-CAR transition, ramstage or payload; native coreboot raminit and stability are not claimed
```

The second CSI call returned the established tuple `2/2/0x106` and complete
state FNV `a3736e20`. B06V5 accepted the exact candidate policy FNV
`2e0ccce9`, changed only status byte `0x0a` and status-derived flags bit 18,
then installed and read back cold-policy FNV `3c0f3a0b`.

MINIT returned `EAX=0`, `workspace[0x4f]=0x02`, and
`workspace[0xe79]=0x01`. The latter is the statically correlated normal
dispatcher-tail marker after phases B4--B8. Hardware state now included
`MC_MAP60=00024489`, `03.4:f8=00001545`, channel-2 DOD `000002ac`, and
rank-present `00000003`. The complete `0x2bcc`-byte workspace had FNV
`6d87f4e4` and binary SHA-256
`e45f3069048a15772a96f5b339b5471421d7d27f38c30a8bb6b8e44e3b74f8c9`.
Its private raw transcript hash is
`2e094b8ca776048eaca6b736cabdbe12ccbef794d479afa2e34f63c2d6b7029a`.

The default memory type was UC; active variable MTRRs covered only CAR and
flash. Ordinary-memory reads at six addresses from `0x00100000` through
`0xb0000000` therefore did not come from a WB L3/CAR mapping; the first two
addresses were also reread with identical values. At `0x01000000`, four data
patterns (`0`, all ones, `55aa55aa`, and `aa55aa55`) were written, read back
and followed by a verified restoration. Ten distinct simultaneous patterns at
addresses spanning selected bits A2 through A30 also read back and restored
exactly.

Finally, a sealed 32-operation ROMMON table used anchor `0x03000000` and ten
XOR-selected addresses for A22 through A31. All 11 writes and subsequent
assertions completed with `PROGRAM_FNV=1447f71f`, `RUN=ok`, `MUT=0b`, and
`TXN_FNV=f127d461`. Explicit reverse-order rollback returned
`RB_RESULT=ok`, `ROLLED_BACK=01`, and new transaction FNV `cd1d74f6`; discard
then succeeded. Independent reads matched all 11 original dwords. QPI stayed
`030f0f03`, memory status stayed `00001545/00000140`, and all sampled RAS/ECC
counters remained zero.

Two further 32-operation tables exercised walking one at the single UC dword
`0x01000000`. Each contained 16 reversible write/full-mask-assert pairs.
D0--D15 passed with program/transaction/post-rollback FNVs
`08863aba/39cfb15a/6543677b`; D16--D31 passed with
`356d86da/6f1cacba/97d3ab0b`. Each run recorded `MUT=10` (hex), explicit
rollback returned `ROLLED_BACK=01`, and discard succeeded. A final independent
read restored the original `e82c939b`; QPI and memory status were unchanged,
and the six RAS/ECC counters remained zero. This is one-address walking one,
not by itself a bulk memory test.

Two final walking-zero tables each applied 16 reversible
write/full-mask-assert pairs at `0x01000000`. D0--D15 passed with
program/transaction/post-rollback FNVs
`b7db281a/7096d1a8/98a98539`; D16--D31 passed with
`2b2469ea/450cb168/33528d31`. Each run recorded `MUT=10` (hex), exact rollback
returned `RB_RESULT=ok` and `ROLLED_BACK=01`, and discard succeeded. The final
independent value was again `e82c939b`; QPI and memory status were unchanged,
and all six RAS/ECC counters remained zero.

This establishes real cache-independent DDR reads/writes, one-dword D0--D31
walking-one/walking-zero behavior, and the specifically tested non-aliasing
behavior once on the locally vendor-assisted path. It does not satisfy the
ten-AC-cold-boot rule, a multi-address walking-data suite, a 256-MiB bulk test,
a long memory test, native open initialization, or ramstage/payload execution.
Detailed hashes, register boundaries and address sets are in
[`research/msi/b06v5-minit-dram-smoke-2026-09-04.md`](../research/msi/b06v5-minit-dram-smoke-2026-09-04.md).

The bootblock banner was observed as B06V4 on both entries, although romstage
and ROMMON correctly reported B06V5. This is a source-ordering identity defect:
the bootblock checks the inherited V4 option before the more-specific V5
option. It did not affect the tested execution path, but must be corrected in
the next image.

## 2026-09-04 — B06V6 automatic post-MINIT handoff and DRAM ROMMON build

```text
test ID: B06V6-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (public 4 MiB base): 43ab0fe3f6c361f1b9c3cd9f8b2d2d2bc9f6dff0205f698f93dab552e4b7c769
ROM hash (local 4 MiB composite): 3a36a6419f0cd6a8f843a0e1e966f92923927934149b8a7dbe91c0519439b952
ROM hash (local W25Q128 16 MiB): 1898bdd50571f9c9809b9c5047825fad07c6a75bf528c7715f3b6727caab970f
flash chip: target socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: exact 0x0000001f required
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; exact full-SPD FNV fb66b530 required
GPU: unchanged; not exercised before ROMMON
PSU: not re-recorded
boot type: build only; hardware execution pending
POST trace: first pass expected 02,03,04,d1/reset; second-pass target 02,03,05,d1,d2,06,07,d3,d4,08,09,0a,0b,0c,0d,30,31,32,0e,0f
serial log: expected COM1 0x3f8, 115200 8N1; both B06V6 identities, exact automatic telemetry, DRAM ROMMON prompt
result: two fixed-epoch ccache-disabled builds byte-identical; portable tests, static/link audit, local composition and 16-MiB placement pass
recovery required: none during build; persistent guard recovery and socketed known-good flash remain mandatory for hardware trial
notes: local vendor-assisted bridge; no native raminit, normal PCI enumeration, network/TFTP, uploaded-code execution or payload
```

B06V6 removes the manual command boundary only for the exact path already
observed in B06V5. It requires the E5645/microcode/SPD tuple, complete CSI,
policy and workspace fingerprints, the returned call/result relationship,
workspace markers and the exact final Uncore tuple. Absolute MINIT EBX/ECX/
EDX/EDI/EFLAGS values are not used because at least EBX is a caller-owned CAR
address and changes with link layout. Any mismatch falls back to the retained
CAR ROMMON; an ambiguous reset-loop state stops at `1f`.

Before the first vendor call the standard CMOS diagnostic byte is changed
from observed `0x6c` to guard value `0xec`. A five-byte complemented I801
signature distinguishes CSI pass 1, pass 2 and MINIT-in-progress. The guard is
cleared only after all MINIT and Uncore gates pass. Recovery from `1f` requires
`unlock RESET`, `autoguard clear`, verified `0x6c`, and complete AC removal.

After a successful return, romstage checks the exact default-UC MTRRs, holds
14 distinct cross-window patterns simultaneously, restores them, then tests
every dword in both complete 8-MiB windows with address-derived pattern,
inverse and clear passes. CBMEM is fixed to `0x01000000..0x017fffff` and made
WB by postcar. Postcar and relocatable ramstage fit there; after the one-way
transfer, its old `0x02000000` area becomes the UC debug-object window.

The BS_PRE_DEVICE monitor validates CBMEM, handoff digest, UART and postcar
MTRRs before POST `0f`. Its XRL1 serial sink supports one read-back-verified
object up to 4 MiB and deliberately provides no execution command. A bounded
RX-tail drain prevents CRLF/CRCRLF from corrupting the binary header. Passive
`timer` and `netprobe` commands gather prerequisites for a later polling TFTP
RRQ loader into this same scratch window; TFTP is a ROMMON debug transfer, not
a boot option.

Both clean builds produced ROM SHA-256 `43ab0fe3...e4b7c769`, generated config
SHA-256 `c73a6d88...c4dd0`, and identical stage ELFs. Portable RAM-loader C,
RAM-loader Python (eleven tests), register-script C, script-tool Python (seven
tests), MSI composer Python (ten tests), whitespace, CBFS, undefined-symbol,
MTRR/rmodule-range, local vendor read-back and W25Q128 top-placement checks all
passed. No B06V6 hardware result exists yet.

## 2026-09-04 — B06V8 safe fallback and ICH10 RTC-bank alias discovery

```text
test ID: B06V8-HW-RTC-ALIAS-01
image ID: X58PROE-B06V8-HIGHQPI-3PASS-PROBE-ROMMON-20260904
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (intended local W25Q128 16 MiB): 1a56d154c68de325fd8ba8baadc6c2a2a8d795fbd3c7060c291e2d26bff6f7c4; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; full-SPD FNV fb66b530
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: manually powered; true G3 duration not independently verified
POST trace: automatic path emitted fallback 20 and entered terminal bc ROMMON; no CSI/MINIT call
serial log: local-only blobs-local/msi-x58-pro-e/b06v8/2026-09-04-b06v8-rtc-upper-bank-discovery.raw, SHA-256 efbf0e0871348e75f21b391fc78112c33c8e3033899cebff71dee71477025503
result: SAFE FALLBACK plus reversible identification of the RTC upper-bank prerequisite
recovery required: none; controlled RC/index transaction rolled back and RC read back exactly zero
notes: B06V8 is retired; its old cold-rearm table must not be executed
```

The automatic run stopped at exact reason `B06V8_CMOS_DIAGNOSTIC_GATE`; it did
not arm or invoke CSI and did not invoke MINIT. Subsequent ROMMON inspection
found ICH10 `RCBA+0x3400=00000000`. The port uses only the narrow
`I82801JX_EARLY_CORE`, so the full upstream i82801jx bootblock write which
normally sets bit 2 had not executed. Intel documents bit 2 as the upper-128
RTC enable; while it is clear, ports `0x72/0x73` alias the standard RTC bank.
Thus the earlier alleged `EXT80/81/82/88/89` tuple was actually reading RTC
seconds, seconds alarm, minutes, month, and year. The previously generated
B06V8 cold-rearm table consequently wrote clock/calendar state and is now
intentionally empty and rejected by the script compiler.

A controlled 21-operation reversible table (`PROGRAM_FNV=425ee412`) changed
only RC bit 2 and the index selector, then read the real upper bank. It returned
`TXN_FNV=c2b84eed`; explicit reverse-order rollback succeeded, produced final
transaction FNV `798c3b79`, restored RC to `00000000`, and restored selector
readback to `7f`. With U128E temporarily set, target upper-bank values were:

```text
logical 80/81/82/88/89 = a0/84/c0/a0/06
logical 8e/ca/f1/f5    = 44/0c/26/bc
```

The vendor-booted reference independently had RC exactly `00000004` and
upper-bank values `43/54/37/02/00` plus `30/00/48/c1`. Their disagreement is
additional evidence that these bytes are board-local telemetry, not a policy
which should be copied. The independently recorded Slow-to-High QPI transition
and endpoint state remain valid; only the prior RTC-input interpretation and
claimed deterministic recipe are invalidated.

## 2026-09-04 — B06V9 deterministic High-QPI three-pass build

```text
test ID: B06V9-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (public 4 MiB base): d954b0b14fcd1716594ca5aa49a1fff809d150f81b76a6c8e2e3fbcb753f97d8
ROM hash (local unpatched 4 MiB composite): f93e516eb5bc1c63c32ec87ec8364bd765a83fea07592fa009c39b2bb3127d13
ROM hash (local deterministic 4 MiB composite): 0b40713543a28ac0e85600394b17cbc2984d3b6ebcbd03c31b315e57b9c09730
ROM hash (local W25Q128 16 MiB): 0eeebf94b91b2fe060329974a69878385d2a8cb0b01721beaa73fd4b510af032
flash chip: target socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: exact 0x0000001f required
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; exact FNV fb66b530 required
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: build only; hardware execution pending
POST trace: each entry adds 10/11 for RC precheck/enable; pass 1 target 02,12,03,04,d1/reset; pass 2 target 02,12,03,05,d1,d2,ca,cb,fe/reset; pass 3 target 02,12,03,cf,d1 and, only on return, d9,da,20,bc
serial log: expected COM1 0x3f8, 115200 8N1; exact B06V9 identity and RTC_RC PRE/POST gate
result: two clean fixed-epoch builds byte-identical; source/tool tests, local composition, wrapper and W25Q128 placement checks pass
recovery required: none during build; socketed known-good flash and exact B06V8/B06V9 guard recovery remain mandatory for hardware trial
notes: terminal vendor-assisted CSI research only; no MINIT, DRAM handoff, ramstage, PCI enumeration, GPU init, or payload
```

B06V9 accepts RTC configuration only as zero or bit 2, sets bit 2, and requires
the complete readback to be exactly four before any vendor call. Six
hash-pinned wrapper regions change seven bytes. They directly establish the
previously successful logical CSI state (`06/07=1/1` and `0/1` at all four
paired fields) and bypass the time-varying RTC parser. The wrapper result is
SHA-256 `d38c0932...89e0fa9`, FNV-1a `98e2f3de`. All B06V8 phase cookies,
exact QPI predicates, one-shot MSI SYRE reset, pass-3 full telemetry and the
MINIT prohibition remain in place. At the time of this build record, no B06V9
hardware result existed yet; the later immutable hardware record is appended
below.

## 2026-09-04 — B06V7 robust Slow-QPI canonical handoff build

```text
test ID: B06V7-BUILD-01
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (public 4 MiB base): f8c4db2a2735dc49d7510071c456821924bb48995a33a3b4c2c29c81900ea466
ROM hash (local 4 MiB composite): 81568b89ba91bf2cd607ab19cfd06aa55f561067ffa3a81b0230d82f86a81eb0
ROM hash (local W25Q128 16 MiB): 91af11e89b896e001cf8eb911ea47329a87fc4f551158b06b3655cd2383a815e
flash chip: target socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: exact 0x0000001f required
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; exact full-SPD FNV fb66b530 required
GPU: unchanged; not exercised before ROMMON
PSU: not re-recorded
boot type: build only; hardware execution pending
POST trace: first pass expected 02,03,04,d1/reset; second-pass target 02,03,05,d1,d2,06,07,d3,d4,08,09,0a,0b,0c,0d,30,31,32,0e,0f
serial log: expected COM1 0x3f8, 115200 8N1; exact B06V7 identity; raw/canonical CSI and workspace telemetry; DRAM ROMMON prompt
result: two clean fixed-epoch builds byte-identical, V6 regression build, canonicalizer/evidence gates, static/link audit, local composition and 16-MiB placement pass
recovery required: none during build; persistent guard recovery and socketed known-good flash remain mandatory for hardware trial
notes: robust Slow-QPI only; separate High-QPI capture rejected; no native raminit, normal PCI enumeration, network/TFTP, uploaded-code execution or payload
```

The two successful Slow-QPI CSI captures had raw FNVs `a3736e20` and
`c4eab5a4`, differing at byte `0x2a6` (`08`/`0c`); replacing only that byte
with zero in the hash stream produced `8403ac98` for both. The separate
High-QPI state produced canonical FNV `cbddcf0e` and therefore fails closed.
The two successful MINIT workspaces had raw FNVs `6d87f4e4` and `1f354cea`;
zeroing the nine explicitly documented inclusive variable ranges only for the
hash produced `7a34f363` for both.

B06V7 never changes the raw CSI or workspace buffers. It emits their raw and
canonical digests, retains both pairs in version-2 handoff `X6HO`, and repeats
the canonical gates after MINIT. The exact policy, ABI self-consistency,
vendor stack, canary, completion-marker, Uncore, MTRR and DRAM-window gates
from B06V6 remain. No absolute MINIT EBX/EDI/EFLAGS values were introduced.

Four portable canonicalizer tests passed. Both Slow-QPI evidence pairs passed
`--require-gate`; the High-QPI evidence returned failure. All stage links have
no undefined symbols, local MSI ranges read back exactly, the lower 12 MiB of
the full-chip image are all `ff`, and its upper 4 MiB equal the composite.
A clean B06V6 configuration also built successfully with V7 disabled. No
B06V7 hardware execution has occurred.

## 2026-09-04 — B06V9 AC-cold deterministic High-QPI sequence

```text
test ID: B06V9-HW-G3-01
image ID: X58PROE-B06V9-DETERMINISTIC-HIGHQPI-3PASS-ROMMON-20260904
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (intended local W25Q128 16 MiB): 0eeebf94b91b2fe060329974a69878385d2a8cb0b01721beaa73fd4b510af032; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; full-SPD FNV fb66b530
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: one controlled 15-second Shelly AC removal followed by AC restoration and a physical power-button press
POST trace: four reset-vector/romstage segments; preliminary COLD_DEFAULT restart at PCIEXBAR PRE, repeated COLD_DEFAULT pass 1/CSI reset, CONFIGURED pass 2/CSI return/SYRE reset, CONFIGURED pass 3/fallback 20/terminal bc
serial log: local-only blobs-local/msi-x58-pro-e/b06v9/2026-09-04-b06v9-ac-cold-three-pass-highqpi.raw, 17776 bytes, SHA-256 443eb6cbe3808e2ac63481c7057c1bef369e06dea544e78c10109c49a1b549de
result: PARTIAL PASS — deterministic wrapper reached and retained the exact High-QPI endpoint; pass-3 selector then rejected the otherwise matching state because CPU A0 was 00017000 rather than its sole accepted 00017600 value
recovery required: no flash recovery; terminal CAR ROMMON remained available with CMOS 0e=ec and consumed I801 signature 08:42:bd:7a:85
notes: no pass-3 CSI call, MINIT call, DRAM initialization/access, postcar, ramstage, PCI enumeration, GPU initialization or payload
```

The capture contains four boot segments within the one AC-cold test sequence:

1. The first `COLD_DEFAULT` segment changed `RCBA+3400` from zero to four,
   revalidated the complete SPD, and ended immediately after
   `PCIEXBAR PRE ... LO=e0000001`. A new reset-vector banner then appeared.
   The cause of this cold-like restart is unknown. The last printed line is a
   boundary, not evidence that PCIEXBAR caused the restart; the expected
   enabled value was already present and no CSI call had begun.
2. The repeated `COLD_DEFAULT` segment again passed RTC/SPD/PCIEXBAR and the
   deterministic wrapper gates, entered CSI pass 1, and was followed by the
   expected CSI-produced reset. The retained I801 signature on the next entry
   was exactly `08:31:ce:68:97`.
3. Pass 2 returned from CSI with `EAX/EBX/ECX=1/0/2a6`, state FNV
   `92e228e7`, `state[06/07]=1/1`, byte `2a6=0c`, and all four paired fields
   equal to `00/01`. The exact pass-2 CPU/IOH/link predicate accepted, after
   which the one-shot outer IOH SYRE edge produced the intended second reset.
4. Pass 3 entered with exact signature `08:53:ac:6b:94`, but
   `B06V8_HIGH_CSI_PROFILE` returned `platform-state CODE=0d`. The fallback
   consumed the phase and entered terminal CAR ROMMON before an SPD read or a
   pass-3 CSI call.

At the terminal monitor, the retained endpoint was:

```text
CPU ff:02.1 50/54/6c/80 = 160c0112/00000012/0040a0a0/070f0f03
CPU ff:02.1 94/9c/a0/a4 = 00010202/00000502/00017000/00322808
IOH 00:0d.0 82c/840/854/85c/864 = 004060a0/070f0f03/00010102/00000002/00322808
LINK CPU50/58 = 86000000/00064555; IOH_C8 = 0606fc00; SYRE_CC = 00000600
```

Sixteen consecutive ROMMON sample groups kept CPU `A0=00017000`, CPU
`50=160c0112`, CPU `80=070f0f03`, and IOH `840=070f0f03` unchanged. The
independent vendor-booted High-QPI reference also had CPU `A0=00017000`.
Inspection of both B06V9 platform predicates found every other required field
matched: only the hard-coded `A0=00017600` comparison rejected this observed
stable endpoint. This is therefore a fail-closed false negative in the
pass-3 selector, not evidence of a failed QPI transition. No meaning is
assigned to the `00017000`/`00017600` difference beyond those observations.

The Shelly restored AC after the requested off interval, but the board stayed
in soft-off at about 0.9 W and did not start automatically. A physical
power-button press was required. Consequently, direct Shelly cycling is a
valid remote G3 transition but is not yet an autonomous restart mechanism for
this board configuration.

## 2026-09-04 — B06VA exact High-QPI CPU-A0-set build

```text
test ID: B06VA-BUILD-01
image ID: X58PROE-B06VA-HIGHQPI-CPU-A0-ALLOWLIST-ROMMON-20260904
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (public 4 MiB base): f23a19814c1c53115646c473c473b5c46c6f3b77d20fade73611911cf3bddce8
ROM hash (local unpatched 4 MiB composite): 3ffda66334a54d41e1d32941fbb793490844727ae30887126683347ca79e998f
ROM hash (local deterministic 4 MiB composite): 6214f4ef8c9a657059e4b46ed73253d69843bbd79ccdb98d8158463584af7248
ROM hash (local W25Q128 16 MiB): fb7a425afe9e22c85691f4ffa71d88e37baa4b44f66e9b630615aa6d21512a77
flash chip: target socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2 required
CPU stepping: 2
microcode revision: exact 0x0000001f required
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; exact full-SPD FNV fb66b530 required
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: build only; hardware execution pending
POST trace: B06V9 sequence retained; new 13 means the complete High-QPI selector accepted pass 3; terminal path remains d9,da,20,bc only if the third CSI call returns
serial log: expected COM1 0x3f8, 115200 8N1; exact B06VA identity and PASS3_PRESELECT CPU_A0 telemetry
result: two clean ccache-disabled fixed-epoch builds byte-identical; complete 68-test suite, source audit, local composition, exact wrapper patch and W25Q128 placement pass
recovery required: none during build; socketed known-good flash, guard clear and a physically recoverable true-G3 start remain mandatory
notes: no B06VA hardware result; observational third CSI only; no CSI-result acceptance, MINIT, DRAM handoff/access, postcar, ramstage, PCI enumeration, GPU initialization or payload
```

B06VA changes one hypothesis only. In both duplicated High-QPI read-only
predicates, CPU function `ff:02.1` register `0xa0` accepts exactly the two
observed values `00017000` and `00017600`; unobserved intermediate values are
not masked or accepted. The selector itself remains mandatory because it sets
the runtime High-QPI profile used by the subsequent preflight/arm/call gates
and retains the MINIT prohibition. New read-only `PASS3_PRESELECT` telemetry
prints the value and exact-set match, and POST `13` is emitted only after the
complete selector accepts.

The B06V9 configuration was rebuilt from the same source state and reproduced
its published base hash `d954b0b1...53f97d8` byte for byte. Both B06VA builds
produced base hash `f23a1981...3bddce8` and generated-config hash
`a4027b95...7849e7b`. The local MSI wrapper remains the identical six-region,
seven-byte B06V9 patch (FNV-1a `98e2f3de`). The lower 12 MiB of the full-chip
image are erased `ff`; its upper 4 MiB equal the deterministic composite.

## 2026-09-04 — B06VA third-CSI terminal return

```text
test ID: B06VA-HW-PASS3-01
image ID: X58PROE-B06VA-HIGHQPI-CPU-A0-ALLOWLIST-ROMMON-20260904
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (local W25Q128 16 MiB): fb7a425afe9e22c85691f4ffa71d88e37baa4b44f66e9b630615aa6d21512a77; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2
CPU stepping: 2
microcode revision: 0x0000001f
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; full-SPD FNV fb66b530
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: user activated the new image; exact initial power/reset type and early serial stream were not captured
POST trace: not captured; retained runtime is practically unique to pass 3, but the formal PHASE=03 marker chain remains pending
serial logs: local-only b06va terminal state 17283 bytes SHA-256 1f2352a383959c28f1cdeeb63a60388d8135d42aabba71da25b2ccb10c22566b; PCI scan 12604 bytes SHA-256 7c9bd9aa6f2f4e3bae1dfd90123da83185f4ba659f0fe1384b9248752b792501; guard clear 714 bytes SHA-256 09b74d13fc3ec1649dc46e8e1a87d5dcd75c829607a9a48f03ea8e2b1793a0c8
result: PASS WITH TRACE CAVEAT — real wrapper entry and ABI-clean third-CSI return; complete state FNV 8b38506a; non-no-op High-QPI finalization; stable CPU-side vendor-equivalent PHY state
recovery required: no flash recovery; persistent guard deliberately restored ec->2c after capture; true G3 still required before retry
notes: no CSI semantic-success claim, result acceptance, MINIT, ordinary DRAM access, postcar, ramstage, PCI enumeration, GPU initialization or payload
```

The live CAR runtime reported `CSI_ATTEMPTED/RETURNED=1/1`, valid wrapper,
CSI and MINIT signatures, intact canaries, balanced three-argument stack, and
matching EDX/EDI transient-state pointers. The complete returned state had
FNV-1a `8b38506a`; its binary extraction has SHA-256
`ed71cba07f00d61cef6cf4c132877bb323b84f32c15622d737998ea705e4d8a7`.
The observed logical return was `EAX/EBX/ECX=0/0/11`. These facts prove a
real call and return but do not by themselves define the vendor result as
success.

The corrected state summary is:

```text
06/07/2a6/2ef/301 = 01/01/0c/00/00
1c/1d 70/71 cd/ce 121/122 = 00/01 00/01 00/01 00/01
```

Compared with the exact pass-2 baseline FNV `92e228e7`, eleven bytes changed:
`030,031,1db,206,275,285,2ef,2f8,2f9,2fc,302`. The full values are recorded
in `Documentation/b06va-highqpi-a0-allowlist.md`. In particular, `2ef=00`;
the value `01` in the raw line is at `2ed`.

The returned visible High-QPI tuple was:

```text
CPU ff:02.1 50/54/6c/80 = 160c0112/00000012/0040a0a8/070f0f03
CPU ff:02.1 94/9c/a0/a4 = 00010202/00b00502/00017000/00322808
CPU ff:02.0 50/58       = 86000000/00064555
MC ff:03.4 50/54        = 0a000006/00000006
IOH 82c/840/854/85c/864 = 004060a0/070f0f03/00010102/00000002/00322808
IOH 00:10.0 c8          = 0616fc00
IOH SR0/SR1/SYRE        = 00000000/00000000/00000600
```

Pass 3 therefore changed CPU physical control `6c` from `0040a0a0` to
`0040a0a8`, CPU link-delay state `9c` from `00000502` to `00b00502`, and IOH
link `c8` from `0606fc00` to `0616fc00`. The first two changes converge
exactly on the vendor ratio-6 High-QPI snapshot. The entire 256-byte
`ff:02.1` PCI configuration space and all sampled fields of unused PHY
`ff:02.5` matched the live vendor reference byte for byte. The vendor IOH
function is hidden after POST, so no equality is claimed for `c8`.

Ten consecutive groups retained the critical CPU, IOH, link, and memory-clock
values without drift. The final CSI progress checkpoint was
`00:14.1:9c=ea000000`. CPU link offsets `02.0/02.4:80=0000fe91` equal the low
part of the expected CSI local context address `fff8fcd8+1b9=fff8fe91`, so
they are a stage residue predicted by the disassembly rather than an inferred
QPI error.

The early automatic serial stream was not running when the system entered
this session. Manual B06VA vendor calls are compiled out, and the high-profile
selector exists only in phase 3; together with the unique state and endpoint
this makes the pass-3 assignment practically conclusive. For a formally
complete trace, repeat with capture armed before G3 and retain `PHASE=03`,
`PASS3_PRESELECT`, `B06VA_HIGH_CSI_PROFILE`, `CSI_PASS=3`, and
`PASS3_RETURNED`.

The terminal probe returned `platform-state CODE=0d` because B06VA's runtime
probe still compares against the pre-pass-3 selector tuple. It does not
invalidate the returned state. `CSI_ACCEPTED=0` and all MINIT fields zero are
the intended B06VA boundary.

After the immutable captures and SPD verification, `unlock RESET` followed by
`autoguard clear` changed CMOS diagnostic authorization from `ec` to `2c`.
The SPD command had already replaced the volatile I801 signature; this cannot
authorize a non-G3 retry, and a true AC removal restores the controller's cold
defaults.

## 2026-09-04 — B06VB one-shot High-QPI MINIT observation build

```text
image ID: X58PROE-B06VB-HIGHQPI-MINIT-OBSERVE-ROMMON-20260904
base ROM SHA-256: 2dc6aaa7d8459c884fc8f6917c0e8b1233b5900cb88b0edded5ce1ca308b7254
local unpatched SHA-256: 7bc22b4f0cf67e3aa749797254df74963273a36a10c1d92df81e6a76e306d58e
local deterministic SHA-256: 310bb078e49723276cf846d30446bc4bcd0d95928fab439d463379083a2dc801
local W25Q128 SHA-256: 00a86646f59305da878924e2e0a3e8420bc7387fa5f4eec42a6292472c7c22cc
build result: two fixed-epoch builds and their local compositions byte-identical; 77/77 tests pass
hardware status: BUILT / STATIC / NOT RUN; live target still B06VA/old session
```

B06VB exact-gates the measured B06VA pass-3 CSI state (`0/0/11`, FNV
`8b38506a`), requires consumed I801 `08:42:bd:7a:85` and CMOS `ec`, transforms
policy FNV `bc4268bf` through seven reviewed edits to `3c0f3a0b`, and commits
one-shot in-progress signature `08:64:9b:5c:a3`. POST `d3` brackets call entry
and `d4` return. A return prints the complete CSI state and MINIT workspace,
then terminates in CAR. No ordinary DRAM access, postcar, ramstage or payload
is reachable. This is a static build record, not a hardware result.

## 2026-09-04 — B06VB controlled-G3 pass-3 preselector observations

```text
test IDs: B06VB-HW-G3-GATE-01 through -04
image ID: X58PROE-B06VB-HIGHQPI-MINIT-OBSERVE-ROMMON-20260904
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (local W25Q128 16 MiB): 00a86646f59305da878924e2e0a3e8420bc7387fa5f4eec42a6292472c7c22cc; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2, stepping 2, microcode 0x1f
DIMM: sole BLS4G3D1609DS1S00. responder at SPD 0x54, FNV fb66b530, 4 GiB 2R x8
GPU/PSU: unchanged from the preceding fixed test configuration; not exercised/re-recorded
boot type: four controlled G3 removals through Shelly 192.0.2.215, five seconds each, after verified CMOS 0e ec->2c recovery
POST trace: serial phase markers captured; physical POST-card trace not captured
serial: COM1 0x3f8, 115200 8N1 through serial-gateway.example.invalid /dev/ttyUSB1
result: PASS1/PASS2 REPEATED 4/4; PASS3 PRESELECTOR FAIL-CLOSED 4/4; no pass-3 CSI call or MINIT
recovery required: no flash recovery; persistent guard remains ec in the terminal CAR ROMMON
notes: A0 sequence 00017a00, 00017800, 00017a00, 00017a00; every other complete pass-3 preselector field matched
```

Every run started at exact cold defaults with CMOS `2c`, completed CSI pass 1
and its internal reset, then returned from pass 2 with
`EAX/EBX/ECX=1/0/2a6`, CSI FNV `856a313b`, an intact ABI, and the expected
outer SYRE reset. On pass 3 the complete platform predicate rejected only CPU
`ff:02.1 + 0xa0`: the build allowed `00017000|00017600`, while the four G3
runs produced `17a00/17800/17a00/17a00`. The first two retained values were
stable over 20/20 and 10/10 subsequent reads respectively.

All other pass-3 preselector inputs matched exactly, including CPU PHY
`50/54/6c/80=160c0112/00000012/0040a0a0/070f0f03`,
`94/9c/a4=00010202/00000502/00322808`, CPU link
`50/58=86000000/00064555`, MC `50/54=0a000006/00000006`, IOH ECAM
`82c/840/854/85c/864=004060a0/070f0f03/00010102/00000002/00322808`,
IOH link `c8=0606fc00`, and SYRE `cc=00000600`. Consequently B06VB emitted
`AUTO_FALLBACK=B06VB_HIGH_CSI_PROFILE_GATE` before pass-3 CSI. It never
reached POST `d3`, MINIT, ordinary DRAM, postcar, ramstage, or a payload.

The immutable local captures are:

```text
489122279ba08c8dfa18f656783b7d8a76e6dda79a29bc8ab807bd0cc16bf617  2026-09-04-b06vb-g3-run1-runtime.raw  (19096 bytes; includes preceding guarded-reset/runtime checks)
d0b648593524b0db1be3782c905da56042c25cdb8eaa1aec66df213529ccd1bc  2026-09-04-b06vb-g3-run2.raw          (10328 bytes)
f761d2672fe44dfe329cc65ea2778b7a02ff91e92f6689eeb10d3a65fc7138c9  2026-09-04-b06vb-g3-run3.raw           (9919 bytes)
d1aabbec74f1e8fa4f32e96a45aa5ea112504092c6082384c05779b5c512ef9e  2026-09-04-b06vb-g3-run4.raw           (9913 bytes)
```

## 2026-09-04 — B06VC exact-four-value High-QPI MINIT observation build

```text
image ID: X58PROE-B06VC-HIGHQPI-A0-4SET-MINIT-ROMMON-20260904
config: configs/x58-pro-e-b06vc.config
config SHA-256: 6079d3d9981b12eb0860ff61523a87799cf8f01aeb5772aa416ab8832c7d5ada
fixed SOURCE_DATE_EPOCH: 1788523200
base ROM SHA-256: 398c631b6bf68b2bfb9c344c05b3adc477c6ea67e58093efadc2e5ec1c241b1d
local unpatched SHA-256: 1d4c07875830e5a5d8e42b983cc7fd4ab213f81542c16ac5bd40119dcecd6366
local deterministic SHA-256: bac67957db1dde9a6f50619cfd69232992cadc07b339ffa7de12e54ba3d34066
local W25Q128 SHA-256: 6c1ac5710a8689d43609cfbf7e631dbd64b4f981bfd1b41bed247cfb1ce9c5af
build result: two fixed-epoch builds byte-identical; repository tests 87/87; support-tool tests 28/28; independent audit found no blocker
hardware status: subsequently run in four controlled G3 sequences on 2026-09-05; pass 1/pass 2 4/4, pass-3 CSI ABI return 2/4, no MINIT; see the hardware record below
```

B06VC tests the narrow inference from the four B06VB G3 captures: only the
pre-pass-3 CPU PCI `ff:02.1 + 0xa0` predicate now accepts the exact observed
set `{00017000,00017600,00017800,00017a00}`. It applies no mask and performs
no A0 write; no field meaning is claimed. Exact post-CSI A0 `00017000` and
all B06VB platform, result/digest, authorization, policy, one-shot, and
terminal gates remain unchanged.

The local composition retains vendor ROM SHA-256
`ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8`
and the seven-byte patched wrapper, SHA-256
`d38c093271f18cfa5f399421654fcab2e61fa07efca85323904dcb74b89e0fa9`,
FNV-1a `98e2f3de`. Its lower erased 12 MiB hash to
`6747318cfda6f6bb9e77ee1c229d37b1799610bc1d41e5165cc40ccf2363a7c4`;
the upper 4 MiB exactly equal the deterministic local composite.

The build contract allows pass-3 CSI to reset, hang, or return. POST `d3` is
possible only after a return satisfies the exact post-CSI and policy gates;
POST `d4` is possible only if MINIT returns. Any mismatch fails closed before
MINIT or at the terminal guard. There is no DRAM handoff, postcar, ramstage,
graphics, or payload path regardless of the observed branch. Recovery remains
external restore of the verified vendor image to the socketed flash.

## 2026-09-05 — B06VC four controlled-G3 pass-3 observations

```text
test IDs: B06VC-HW-G3-GATE-01 through -04
image ID: X58PROE-B06VC-HIGHQPI-A0-4SET-MINIT-ROMMON-20260904
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (local W25Q128 16 MiB): 6c1ac5710a8689d43609cfbf7e631dbd64b4f981bfd1b41bed247cfb1ce9c5af; programmer read-back not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 0x000206c2, stepping 2, microcode 0x1f
DIMM: sole BLS4G3D1609DS1S00. responder at SPD 0x54, FNV fb66b530, 4 GiB 2R x8
GPU/PSU: unchanged from the preceding fixed test configuration; not exercised/re-recorded
boot type: four controlled G3 removals through Shelly 192.0.2.215 for five seconds each, after verified CMOS 0e ec->2c recovery before each attempt
POST trace: serial phase markers captured; physical POST-card trace not captured
serial: COM1 0x3f8, 115200 8N1 through serial-gateway.example.invalid /dev/ttyUSB1
result: PASS1/PASS2 PASS 4/4; PASS3 PRESELECTOR FAIL-CLOSED 2/4; PASS3 CSI ABI-CLEAN RETURN THEN STRICT FALLBACK 2/4; no MINIT
recovery required: no flash recovery; terminal CAR ROMMON remained available
notes: pre-pass-3 A0 sequence 00017c00/00017a00/00017c00/00017000; CSI3 runs returned identical state FNV 03e3d24e; every complete preselector field matched apart from rejected A0
```

The first captured stream begins with two starts that encountered the intended
persistent reset-loop guard at CMOS `0e=ec`. From ROMMON, `unlock RESET` and
`autoguard clear` changed it to `2c`; each subsequent recorded attempt used a
five-second AC removal. The exact B06VC identity appeared on the serial
console, but the flashed-image hash above is the expected artifact hash rather
than an independently captured programmer read-back.

Pass 1 matched and performed its CSI-internal reset 4/4. Pass 2 returned and
was accepted 4/4 with:

```text
EAX/EBX/ECX = 00000001/00000000/000002a6
EDX/EDI     = fff8fcd8/fff8fcd8
EFLAGS/VESP = 00000086/fff8ffe4
CSI FNV     = 856a313b
state 06/07/2a6/2ef/301 = 01/01/08/01/00
```

The expected outer SYRE reset completed 4/4. Pass-3 preselector A0 was
`00017c00/00017a00/00017c00/00017000`. Runs 1 and 3 rejected `00017c00` with
platform-state `CODE=0d` before CSI; five direct read-only samples after run 1
retained that value. Runs 2 and 4 passed the preselector and invoked pass-3
CSI.

All other complete preselector inputs matched exactly:

```text
CPU ff:02.1 50/54/6c/80 = 160c0112/00000012/0040a0a0/070f0f03
CPU ff:02.1 94/9c/a4    = 00010202/00000502/00322808
CPU ff:02.0 50/58       = 86000000/00064555
MC  ff:03.4 50/54       = 0a000006/00000006
IOH 82c/840/854/85c/864 = 004060a0/070f0f03/00010102/00000002/00322808
IOH 00:10.0 c8          = 0606fc00
IOH SR0/SR1/SYRE        = 00000000/00000000/00000600
```

Both pass-3 calls returned ABI-clean with `EAX/EBX/ECX=0/0/11`,
`EDX=EDI=fff8fcd8`, `EFLAGS=00000082`, `VESP=fff8ffe4`, intact canaries, and
self-consistent results. Both 772-byte CSI states are byte-identical, SHA-256
`0a10266b0e65c8888b001b386ce73460075e6cc810ed58cd00338a29ce13f199`,
FNV `03e3d24e`, with byte `0x2a6=08`.

Run 2 returned CPU `6c=0040a0a8`, `9c=00a00502`, `A0=00017a00` and IOH
`c8=0616fc00`; the strict post-observation gate rejected it. Run 4 returned a
fully exact platform tuple: CPU `6c=0040a0a8`, `9c=00b00502`,
`A0=00017000`, IOH `c8=0616fc00`, stage value `ea000000`, and exact MC fields.
Only its CSI state still missed the saved B06VA state. The saved 772-byte
B06VA state has SHA-256
`ed71cba07f00d61cef6cf4c132877bb323b84f32c15622d737998ea705e4d8a7`, FNV
`8b38506a`, and differs at exactly byte `0x2a6=0c`. Read-only
canonicalization of that byte to zero yields FNV `908dabb6` for the saved
B06VA state and both B06VC returns.

The strict gate rejected both returned calls before MINIT. ROMMON `vinfo`
confirmed `CSI_ATTEMPTED/RETURNED=01/01`, `CSI_ACCEPTED=00`, and
`MINIT_ATTEMPTED=00` for runs 2 and 4; the post-call probe reported
platform-state `CODE=0d`. There was no POST `d3` or `d4`, MINIT,
ordinary DRAM access, postcar, ramstage, graphics, or payload execution. The
series proves the pass-1/pass-2/reset sequence 4/4 and an ABI-clean,
non-no-op pass-3 CSI return 2/4; it does not prove accepted High-QPI
completion, memory training, or the ten-run repetition milestone.

The immutable local captures are:

```text
320fb21908519be33b0817f345900ab4f59bed35e3591b2e47701989e0c22df7  2026-09-05-b06vc-g3-run1-a0-17c00.raw  (18087 bytes)
4be4b9e9c7151ff77cad9d165d68dd96849ed84937b9349192770d566cfc93f3  2026-09-05-b06vc-g3-run2-csi3-03e3d24e.raw  (17554 bytes)
ba18692b7d3eafa8a0c91329662fb8a83c3f738a3daebf468957208af5ea2cfb  2026-09-05-b06vc-g3-run3-a0-17c00.raw  (10049 bytes)
bfd2051fca64b5f4d58e5427ef003cbc174bfab6096f9556c78baa7c4d241938  2026-09-05-b06vc-g3-run4-csi3-03e3d24e-postexact.raw  (17554 bytes)
```

The run-1 capture also contains the two initial guarded starts and interactive
read-only confirmation.

The exact observed pre-pass-3 A0 evidence is now
`{00017000,00017600,00017800,00017a00,00017c00}`. This supersedes the
four-value evidence behind B06VC but does not justify claiming a register
meaning, a mask, or a valid range. Reset- or training-volatility remains an
inference.

The narrowest evidence-derived successor hypothesis is to add exactly
`00017c00` to the preselector and canonicalize only CSI-state byte `0x2a6`
over the observed set `{08,0c}` under FNV `908dabb6`, while retaining every
post-CSI platform gate exactly. That is a proposed experiment, not behavior
implemented by B06VC and not a claim about either field's semantics.

## 2026-09-05 — B06VD paired CSI-byte-2a6 High-QPI MINIT build

```text
image ID: X58PROE-B06VD-HIGHQPI-CSI2A6-MINIT-ROMMON-20260905
config: configs/x58-pro-e-b06vd.config
config SHA-256: 37b7c38d7e542b4766efcbf8f5a6fc6f038cd434414fc71ea1c53127a56185c1
fixed SOURCE_DATE_EPOCH: 1788523200
base ROM SHA-256: ea498ad961c8569f9a13598e1abb2bd5d9a685c2a8f05f44cb842ba2908032c1
local unpatched SHA-256: 4984cb98a6c54e2b9fb018613dfa78a4ccf4c8ff74ca25e0c3f8ee5126678201
local deterministic SHA-256: fe6e30f67c8e69edfecdbccd801205a585d30c59a8ab7647732e5ba26f8ade0e
local W25Q128 SHA-256: a2686f64b7fd3ada455498d158d7ef443d8e9a789e4fe033e1d0f7cce3340806
build result: two fixed-epoch builds byte-identical; repository tests 98/98; support-tool tests 28/28; independent 40-test source audit found no blocker; B06VC predecessor rebuild byte-exact
hardware status: NOT RUN
```

B06VD implements the narrow successor proposed by the B06VC hardware record.
Only the two pre-pass-3 predicates add exact A0 `00017c00`, producing the
finite set `{00017000,00017600,00017800,00017a00,00017c00}` without a mask,
range, or write.  After CSI returns, both independent enforcement layers
require the exact pairs `2a6=08/raw=03e3d24e` or
`2a6=0c/raw=8b38506a`, plus canonical FNV `908dabb6` when only byte `2a6` is
zero in the digest stream.  The raw buffer is not changed.  Romstage selects
the expected raw digest from a fixed constant only after pair validation.

The complete post-CSI endpoint remains unchanged: notably CPU
`6c=0040a0a8`, `9c=00b00502`, `A0=00017000`, IOH `c8=0616fc00`, IOH stage
`9c=ea000000`, and MC `50/54=0a000006/00000006`.  Thus B06VC run 2 remains a
negative endpoint despite its accepted state variant.  ABI, stack, pointer,
canary, sparse-state, I801/CMOS, policy, authorization, and one-shot MINIT
guards are retained.

The local composition used the pinned MSI image SHA-256
`ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8`.
The deterministic seven-byte wrapper patch verified as SHA-256
`d38c093271f18cfa5f399421654fcab2e61fa07efca85323904dcb74b89e0fa9`
and FNV `98e2f3de`.  The 16-MiB image has an erased lower-12-MiB SHA-256
`6747318cfda6f6bb9e77ee1c229d37b1799610bc1d41e5165cc40ccf2363a7c4`
and a byte-exact deterministic upper 4 MiB.

The first hardware test must use the fixed E5645/microcode-`1f`, sole
SPD-`0x54`, ratio-6 setup and capture COM1 at 115200 before a controlled G3
start.  Clear and verify the persistent guard only if it is `ec`.  POST `d3`
will prove only entry into MINIT; `d4` will prove only return.  Pass-3 CSI or
MINIT may reset or not return.  The build remains terminal in CAR and has no
ordinary-DRAM access, handoff, graphics, or payload path.  Recovery is the
verified vendor image on the socketed W25Q128.V..M.

## 2026-09-05 — B06VD controlled-G3 High-QPI MINIT return and UC-DRAM tests

```text
test IDs: B06VD-HW-G3-01 and B06VD-HW-G3-02
image ID: X58PROE-B06VD-HIGHQPI-CSI2A6-MINIT-ROMMON-20260905
build commit: coreboot fe3e08197177 plus the documented local X58 Pro-E work
intended local W25Q128 SHA-256: a2686f64b7fd3ada455498d158d7ef443d8e9a789e4fe033e1d0f7cce3340806
programmer read-back: not captured
flash chip: socketed W25Q128.V..M, 16 MiB
board: MSI X58 Pro-E revision 3.0
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode revision: 0000001f
DIMM: BLS4G3D1609DS1S00, one 4-GiB dual-rank x8 module, SPD address 0x54
SPD FNV: fb66b530
GPU: not exercised by this terminal-CAR test
PSU: unchanged bench configuration; not newly identified in this capture
boot type: two controlled G3 sequences after verified persistent-guard clear
recovery required: no
```

The first controlled sequence reached the third CSI return with state byte
`0x2a6=08`, raw FNV `03e3d24e`, and canonical FNV `908dabb6`, but returned CPU
`9c=00a00502` and `A0=00017a00`.  The unchanged strict post-observation gate
therefore rejected it before MINIT.  This is a useful fail-closed result, not a
failed or partial memory-init call.

The second controlled sequence completed the pass-1 reset, pass-2 CSI return,
and outer IOH SYRE reset.  Its pass-3 preselector saw `A0=00017000`; CSI then
returned with the accepted paired state `0x2a6=08`/`03e3d24e`, canonical FNV
`908dabb6`, and the exact post-CSI High-QPI endpoint.  The one-shot authorized
MINIT call crossed the `d3` entry and `d4` return boundaries.  ROMMON `vinfo`
then reported `CSI_ATTEMPTED/RETURNED/ACCEPTED=01/01/01`,
`HIGH_MINIT_AUTH=01`, `MINIT_ATTEMPTED/RETURNED=01/01`, return `EAX=00000000`,
intact canaries, and classification `FULL_PATH_RETURN_DRAM_UNTESTED`.  This is
the first B06VD full CSI-plus-MINIT return, once; the classification itself did
not claim usable DRAM.

The returned buffers were reconstructed byte-for-byte through the read-only
monitor and independently hashed:

```text
CSI state: 2026-09-05-b06vd-run2-csi-state.bin, 772 bytes
  SHA-256: 0a10266b0e65c8888b001b386ce73460075e6cc810ed58cd00338a29ce13f199
  raw FNV: 03e3d24e
  byte 0x2a6: 08
  canonical FNV with only byte 0x2a6 zeroed in the digest stream: 908dabb6
MINIT workspace: 2026-09-05-b06vd-run2-workspace-measured.bin,
  11212 bytes (0x2bcc)
  SHA-256: 843dbc4e45b734ba18dd2f36e183886d75d22e48a2a6f3a02d44965bfbe6833a
  FNV: 94299f43
  bytes 0/1/2/0x4f/0xe79: 00/00/00/02/01
policy FNV before and after the accepted path: 3c0f3a0b
```

The effective MTRR default type was UC (`MSR 2ff=00000800`); the only logged
special ranges covered CAR and flash.  Two repeated reads at `0x01000000` and
`0x02000000` were stable.  A reversible two-address write test first proved
independent data at those two locations and restored their original dwords.

The ROMMON transaction engine then ran a simultaneous 14-address alias smoke
test at:

```text
01000000 01000004 01000ffc 01001000 013ffffc 017ff000 017ffffc
02000000 02000004 02000ffc 02001000 023ffffc 027ff000 027ffffc
```

All 14 writes and all 14 full-mask assertions passed (`RUN=ok`, program FNV
`0a85f9f6`, `MUT=0e`).  Explicit reverse-order rollback also passed, changing
the transaction digest from `64a6d83a` to `f043be09`; `RB_TRIES=1`,
`ROLLED_BACK=01`.  Every saved original was restored and the exact transaction
was subsequently discarded.  Within this selected address set, the result is
evidence against the tested aliases; it is not a proof for all address lines or
the advertised DIMM capacity.

A 32-bit walking-one test followed at the single UC dword `0x01000000`.
Separate D0--D15 and D16--D31 tables, with program FNVs `08863aba` and
`356d86da`, completed every write/read assertion with `RUN=ok`.  Each half used
16 reversible mutations, rolled back successfully, and restored the original
`e8acd19b`.  The respective transaction digests changed
`f3fce070 -> 8e62eceb` and `0c825210 -> 66e9075b`; both exact tables were
discarded after verification.  This proves all 32 data bits for one tested
dword in this run, not multi-address data-bus integrity.

The clean post-MINIT sample and the final post-rollback sample agreed on the
key endpoint registers:

```text
CPU ff:02.1 QPI +80:       070f0f03
CPU ff:03.0 mapper +60:    00024489
CPU ff:03.4 common +f8:    00001545
CPU ff:06.1 channel-2 DOD: 000002ac
CPU ff:06.0 ranks +7c:     00000003
CPU ff:06.0 channel +5c:   00000140
IOH 00:14.1 stage +9c:     bf000000
sampled ff:03.2 +80..+94:  six dwords all 00000000
```

The final transaction state was invalid/discarded, the first test dword still
read `e8acd19b`, and ROMMON remained responsive.  The six zero dwords are only
the sampled RAS/ECC-register set; their zero value does not replace a complete
error-status audit.  Final `vinfo` reported `ROMMON_DIRTY=01`, as expected
after the intentional monitor writes.  Its generic pre-call platform probe
also reported `platform-state CODE=0d`: the current probe re-applies the exact
post-CSI/pre-MINIT profile after MINIT has changed platform state.  The latched
accepted/returned flags and hashes above are separate from that probe result;
the individual post-MINIT predicate producing `0d` was not instrumented.

Immutable local evidence includes:

```text
631eccca6e4273645683f089a18a55999da7d4d695b175279275561334389737  2026-09-05-b06vd-authorized-g3-run1.raw
baf59642dabfd43b8491d956a0ed57312f6ef311cd09a6c4c5a34d37b2907f7e  2026-09-05-b06vd-authorized-g3-run2-minit-return.raw
26252d9a5c06fd602a7d31c35e3f8269e07afc6bfa8dd5384c81099aef12682b  2026-09-05-b06vd-run2-post-minit-keyregs.raw
fea494a46a45f5a1e3062c2027563499266479557473c999a705f6468f080b68  2026-09-05-b06vd-run2-alias-exec.raw
53a4a2e5e807cedb31a6d989755ade68e1610d5d25054a54885ede1744109673  2026-09-05-b06vd-run2-alias-rollback.raw
7990f7a8dd05c7d9644477daeae7a118b83b3bf69db42f61b17d6f26a0688266  2026-09-05-b06vd-run2-walk1-low-exec.raw
8abb1643e5566b6c885623c285ec0844c7a80ba9d88cd77d45fd2298de130221  2026-09-05-b06vd-run2-walk1-low-trace-rollback.raw
6121fd2cee7ea967a7db3f4000c43672051eeacb15190bb06a9f0623a0e04c28  2026-09-05-b06vd-run2-walk1-high-exec.raw
1e24ed0cb3dd829aa43cbca326b629d04959a8fd750104e5f24a592be0691719  2026-09-05-b06vd-run2-walk1-high-trace-rollback.raw
f93affdd3d4c8d6e103e6d920ead3fc12946e53274166e2304ac6ea24088aeac  2026-09-05-b06vd-run2-final-state.raw
```

The main run-2 serial capture contains dropped/interleaved characters.  The
exact reconstructed buffer images and clean follow-up command captures above
provide the hashes and final values used for this record.  There is still only
one accepted B06VD MINIT return, no full-window or 256-MiB/bulk/soak test, no
multi-address walking-data test, and no ten-cold-boot stability result.  This
path executes locally held proprietary vendor-assisted code and is not a
native/open X58 memory initializer.  B06VD remains in CAR: it has no automatic
ordinary-DRAM handoff, postcar, ramstage, PCI enumeration, GPU initialization,
or payload execution.  No conclusion about the full 4-GiB DIMM capacity or a
bootable system follows from these targeted tests.

## 2026-09-05 — B06VE exact High-QPI automatic handoff build

```text
test ID: B06VE-BUILD-01
image ID: X58PROE-B06VE-HIGHQPI-AUTO-RAMSTAGE-ROMMON-20260905
config: configs/x58-pro-e-b06ve.config
config SHA-256: 1d70ed6ed9cdb5a81fc08d25b9a21b7c50b45cffffce1791c9bc5861714abd12
generated .config SHA-256: ddcb296bc97596f59c1424a1f66c454dafc7094cfcb82d142a400fcf35a94228
fixed SOURCE_DATE_EPOCH: 1788523200
base ROM SHA-256: 7d437426544aa2c0923b78c690c806022452db78cb866734b49af26c2e445ab7
local unpatched SHA-256: ff31553850547c4e748090bf4e25ee98daef9171edb72f2a6e50e02cef4f4fd0
local deterministic SHA-256: bcc31841555c54c82331428ee29882da11380a5f5f079e8a7211d30eae6b7c47
local W25Q128 SHA-256: 71ddac31fe7ec5c977c0bc7a2e072468032ac9e535afeb97cde7d56ccd33195f
payload: none
build result: two ccache-disabled fixed-epoch builds byte-identical; repository tests 107/107; support-tool tests 28/28; independent focused audit 62/62 with no blocker; B06VD predecessor ROM byte-identical
hardware status: NOT RUN
```

B06VE promotes only the exact successful B06VD run-2 observation.  The
inherited B06VD pair/canonical/ABI/platform audit still runs first, after which
the `2a6=0c`/raw-`8b38506a` branch returns to the CAR recovery ROMMON before
CSI acceptance, MINIT authorization, or MINIT.  Continuation requires
`2a6=08`, raw FNV `03e3d24e`, and canonical FNV `908dabb6`.

After MINIT returns, a new independent gate requires EAX zero, intact
canaries/stack, exact policy FNV `3c0f3a0b`, workspace FNV `94299f43` and
completion bytes, QPI `070f0f03`, IOH stage `bf000000`, plus the recorded
mapper/common/channel tuple.  Only this gate can create the version-3 handoff.
No ordinary DRAM access precedes that promotion.

The post-memory path requires the exact default-UC MTRRs, runs the simultaneous
14-address test, exercises and clears every dword in both complete 8-MiB
windows with address-derived and inverse patterns, creates CBMEM, and verifies
the handoff read-back.  It then clears the exact MINIT I801 marker, restores
CMOS `0x0e=2c`, and enters coreboot postcar.  Ramstage rechecks CBMEM, the
handoff, COM1 115200 8N1, and postcar MTRRs before presenting the DRAM ROMMON.
These later checks can still halt after guard finalization; they do not request
an automatic reset.

The expected successful third-pass suffix is
`cf,d1,d9,d3,d4,08,09,0a,[full tests],0b,0c,0d,30,31,32,0e,0f`.
POST `0a` may persist during the full tests.  Codes `14..19` are fail-closed
post-memory stops, `1f` is a persistent guard stop, and `20,bc` is a CAR
fallback.  This remains a vendor-assisted experimental bridge with no normal
PCI enumeration, GPU initialization, executable object, or payload.

The local composite was generated from MSI image SHA-256
`ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8`.
All four copied vendor ranges and the deterministic seven-byte wrapper patch
were verified.  The 16-MiB image has an all-`ff` lower 12 MiB with SHA-256
`6747318cfda6f6bb9e77ee1c229d37b1799610bc1d41e5165cc40ccf2363a7c4`
and a byte-exact deterministic upper 4 MiB.  Hardware claims must wait for a
real B06VE log.

## 2026-09-05 — B06VE hardware execution, second MINIT-workspace form, and PAM preflight

This entry appends the hardware result to the earlier immutable build record;
it supersedes that record's then-correct `hardware status: NOT RUN` statement.

```text
test ID: B06VE-HW-01
build commit: coreboot-fe3e08197177-dirty-x58-pro-e-b06ve
image ID: X58PROE-B06VE-HIGHQPI-AUTO-RAMSTAGE-ROMMON-20260905
ROM hash: local W25Q128 image 71ddac31fe7ec5c977c0bc7a2e072468032ac9e535afeb97cde7d56ccd33195f; programmer read-back not recorded
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual rank
DIMM slot: sole SPD responder 0x54; physical silkscreen not recorded here
GPU: not recorded in these captures
PSU: not recorded
boot type: three live captures requested/stored as cold; only two entered a complete three-pass sequence, and independent power/read-back provenance remains pending
POST trace: serially reported phase markers; separate complete POST-card trace not recorded
serial log: three immutable raw captures and follow-up ROMMON transaction captures listed below
result: one exact High-QPI MINIT return; B06VE rejected new raw workspace b6346533 before all post-memory code and retained the CAR recovery ROMMON
recovery required: no; the fail-closed CAR monitor remained usable
notes: no B06VE post-memory test, CBMEM, postcar, ramstage, DRAM-ROMMON, or payload execution occurred
```

The three initial captures had distinct and useful outcomes:

- `1213` saw `CMOS0E=ec` immediately and stopped on the persistent reset-loop
  guard before any automatic CSI/MINIT call.
- `1215` completed reset-separated CSI passes 1 and 2 and returned from pass 3,
  but the post-CSI endpoint remained CPU `9c=00a00502`, `A0=00017a00`.  The
  strict pre-MINIT gate rejected it.
- `1217` completed passes 1 and 2, entered pass 3 at `A0=00017000`, returned
  with CSI state `08`, raw FNV `03e3d24e`, canonical FNV `908dabb6`, and the
  exact accepted High-QPI endpoint.  MINIT returned with EAX zero.  The
  post-MINIT state was QPI `070f0f03`, IOH stage `bf000000`, mapper/common
  `00024489/00001545`, and channel-2 DOD/ranks/status
  `000002ac/00000003/00000140`.

Run 3's raw MINIT-workspace FNV was `b6346533`, whereas B06VE admitted only the
earlier raw observation `94299f43`.  It therefore printed
`AUTO_FALLBACK=B06VE_POST_MINIT_EXACT_GATE` and returned to the CAR recovery
ROMMON before POST `08` or any ordinary-memory access.  The saved 11,212-byte
workspace has SHA-256
`6549cd0c2eb04bacb5dd2a15fb000c109cca8fcb622473da6934de8d11018b1c`.
Offline comparison found 20 changed bytes relative to the earlier admitted
form: fifteen lie in previously recognized dynamic ranges, with five newly
observed bytes at `2459..245c` and `2461`.  Exact canonicalization of the
observed dynamic ranges maps both raw forms to FNV `a6f9c2e6`; B06VE did not
contain that later canonical gate and did not authorize itself from this
observation.

The initial capture provenance is:

```text
a1a03b888c68c2b09b87e9a0047b63878c315c3d5fe7e8e60bc7475d55f03b88  x58-b06ve-cold-20260905-1213.raw.log  (506 bytes)
2626d2a77b460f4470ede0c152563aaf57adda89449ef3533bf3746f2c9c6524  x58-b06ve-cold-20260905-1215.raw.log  (16445 bytes)
2ddb5bc2e71a9c87f31e65bf22f7291bb25dac9e79bbaa55e7f10549f5047b17  x58-b06ve-cold-20260905-1217.raw.log  (19247 bytes)
```

While still in run 3's post-MINIT **CAR recovery ROMMON**, transactional
scripts exercised the PAM/shadow prerequisite for the planned payload build.
PAM bytes `ff:00.1 + 0x40..0x46` changed with exact read-back from all zero to
`30/33/33/33/33/33/33` and were restored to all zero.  Reversible tests covered
representative points across all sixteen 16-KiB half-windows in
`000c0000..000fffff` without an alias among the tested locations.  The final
script kept distinct patterns at exact future SeaBIOS words `000f8800` and
`000fecd4` resident simultaneously and returned `RUN=ok`.  Reverse rollback
restored both original words and all seven PAM bytes; its transaction FNV was
`68a06b96` before and `6e389292` after rollback.  Armed discard also returned
success.

```text
718de5a02c0776c20df10e77aefacf37b9b2d57bcf013fb604762815dc6d2a08  x58-b06ve-run3-b06vf-seabios-exact-run.raw      (6844 bytes)
b8798c52aa355722cff2c9d7bdeb306d54abce57d35662a048372a89051daaca  x58-b06ve-run3-b06vf-seabios-exact-rollback.raw (4772 bytes)
1a735dc6fda14c89dfcd3162872eec5647b549d8f47fa9389017139f2cbb2693  x58-b06ve-run3-b06vf-seabios-exact-discard.raw   (553 bytes)
```

These are real PAM/shadow observations in a post-MINIT CAR state, not a
B06VE DRAM handoff.  They do not establish B06VE's complete 16-MiB tests,
postcar/ramstage, coreboot SELF loading, SeaBIOS entry, PCI enumeration, GPU
output, storage operation, full-DIMM validity, or cold-boot reproducibility.

## 2026-09-05 — B06VF minimal SeaBIOS entry-probe build

```text
test ID: B06VF-BUILD-01
image ID: X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905
config: configs/x58-pro-e-b06vf.config
config SHA-256: 508efb2c62ba492f94c331743f19e8e7d2cdf14812f84a27c8df15e2bcd423b9
generated .config SHA-256: 703a58287b58259e5cdf6b7e828e57f1f0bf2c11af354e05fc610c7a46f31544
fixed SOURCE_DATE_EPOCH: 1788602400
base ROM SHA-256: ae6b8393e8d0d2b8b9cdba6262fc1a8383f90e59c510154bb1d417d5188d9ea6
local unpatched SHA-256: 6498c562d449d87e7f55c1870a8af43add8137b0cc1250f8fdda2045b03f4d84
local deterministic SHA-256: ef1161a52f6c04f6f5e306063d01b1f23f538fbc9da01cf250d02e849b66d01f
local W25Q128 SHA-256: 7dfec8e9d93ffcb2b855dc111c5b4aed789d8a59755eea90700e87156b65317a
payload: SeaBIOS rel-1.17.0 commit b52ca86e094d19b58e2304417787e96b940e39c6
payload geometry: one segment 000f8800..000fffff, entry 000fecd6
build result: two clean fixed-epoch builds byte-identical; repository tests 121/121; focused B06VF tests 14/14; support-tool tests 28/28; portable C scenarios 8+5; CBFS/source payload segment and local composition verified
hardware status: NOT RUN
```

B06VF admits the two actually observed MINIT workspace forms only as exact
raw-FNV/byte-pattern pairs and requires their common canonical FNV
`a6f9c2e6`; all CPU, SPD, CSI, QPI, IOH, memory-controller and ABI endpoint
checks remain exact.  Before leaving romstage it tests and clears every dword
from zero through 640 KiB, repeats the inherited complete 16-MiB transition
tests, and requires a digest-bound version-4 CBMEM handoff.  Ramstage exposes
only the tested RAM apertures at 0--640 KiB, 16--24 MiB and 32--40 MiB and
does not give coreboot a PCI scan method.

At payload load, the image revalidates the actual CBMEM region and handoff,
requires SAD `8086:2d81`, opens PAM with exact read-back, and performs a
simultaneous reversible 34-dword shadow test covering both ends of all sixteen
16-KiB C--F blocks plus SeaBIOS words `f8800` and `fecd4`.  Only after this
passes is the SELF image loaded and entered.  POST `21,22,23,28,29,2a` is the
B06VF success suffix; `24..27` are fail-closed boundary errors.

The reduced SeaBIOS build prints debug output on COM1 but deliberately has no
VGA, drive, USB, option-ROM, or boot support.  It can still read PCI config and
write legacy DMA/PIC/PIT/RTC state.  Therefore a SeaBIOS banner is the next
milestone; even the intended terminal `Boot support not compiled in.` is not
an operating-system boot or general platform validation.  The exact first-run
procedure and hashes are recorded in the B06VF experiment document and image
manifest.

### B06VE persistent-guard cleanup before B06VF flashing

At the user's request, the still-active B06VE post-MINIT CAR recovery ROMMON
was contacted through `serial-gateway.example.invalid:/dev/ttyUSB1` at 115200 8N1.  Passive `id`,
`vinfo` and `vinputs` first confirmed B06VE, `MINIT_RETURNED=01`, workspace
FNV `b6346533`, and `CMOS_DIAG_0E=ec VALID=00`.  The dedicated guarded
sequence was then issued once:

```text
unlock RESET
[RESET] ARMED for one reset command
autoguard clear
[RAMINIT] persistent auto guard 0xec/0xed->0x2c cleared; remove AC power before retry
```

An immediate passive `vinputs` read-back returned
`CMOS_DIAG_0E=2c VALID=01`; the prompt returned without `[R]`, confirming that
the one-command arm was consumed.  No reset or Shelly action was issued.  The
I801 phase signature is volatile and is intentionally left for the mandatory
complete AC removal before the B06VF attempt rather than being modified by an
undocumented live write.

## 2026-09-05 — B06VF G3 coupled-state observations

```text
test ID: B06VF-HW-G3-02..05
image ID: X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905
board: MSI X58 Pro-E / MS-7522 rev 3.0
CPU: Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM: sole BLS4G3D1609DS1S00 responder at SPD 0x54, FNV fb66b530
memory policy: DDR ratio 6
boot provenance: four retained G3-labelled captures; G3-05 used a controlled
  long G3 removal with capture active before power
serial: COM1 03f8, 115200 8N1
flash: socketed W25Q128
pass 1/pass 2: completed in all four captures
pass-3 pre-A0: 17a00 / 17400 / 17400 / 17800
pass-3 CSI return: G3-02 and G3-05 only, EAX/EBX/ECX 0/0/11
returned CSI byte/raw/canonical: 0c / 8b38506a / 908dabb6
returned CPU A0: equal to the saved pre-A0 in both returned runs
returned CPU 9c: 00a00502
QPI status: 070f0f03
result: all four failed closed before MINIT; CAR ROMMON retained
```

G3-03 and G3-04 established the additional literal pre-A0 observation
`17400` and stopped before the pass-3 CSI call because it was outside the
B06VF allowlist. G3-02 and G3-05 returned ABI-clean from pass-3 CSI and prove
that A0 can remain at the exact pre-call value (`17a00` and `17800`
respectively) rather than converging to the older fixed `17000` endpoint.
Both returns couple the preserved A0 with CPU 9c `a00502` and the exact CSI
`0c/8b38506a` form. B06VF rejected them as designed before POST `d3`.
MINIT, ordinary DRAM accesses, postcar, ramstage and SeaBIOS did not run.
These samples do not establish register-field semantics or justify accepting
a mask or numeric range.

Immutable raw captures:

```text
38938c6fd0e91f78c35a04321d0726166dc87513a8732616b977828c135491a3  2026-09-05-b06vf-g3-02.raw          (19265 bytes)
7c064e799d73b3cd483ce29e109a17be5d1b7540a37e524bc7cb5df1ca159cfb  2026-09-05-b06vf-g3-03.raw          (12634 bytes)
6991dd51145cb44c24c540cbcf9431aa861b9d087c56a9a66670e74b80cdb60c  2026-09-05-b06vf-g3-04.raw          (12637 bytes)
352d63d29759be19d590bd0d265fb1090bce04c1d1c4c67a0ec32215f80bd5b3  2026-09-05-b06vf-g3-05-longoff.raw  (19267 bytes)
```

## 2026-09-05 — B06VG coupled MINIT/SeaBIOS build

```text
test ID: B06VG-BUILD-01
image ID: X58PROE-B06VG-COUPLED-MINIT-SEABIOS-20260905
config: configs/x58-pro-e-b06vg.config
config SHA-256: 42210dbdd1d0434992fc5084ac30e726ebdf8d877d7a4252ed02a91349caf7f2
generated .config SHA-256: 7f68460bf9a7629791a73ca5fb87757b7568440beaacb7f3b45d046da8fcd634
fixed SOURCE_DATE_EPOCH: 1788602400
base ROM SHA-256: e3da6541229c743c9c6589e2ebae70af6242c4dab9bcbbe8b37b7c791e26a1d6
local unpatched SHA-256: 427c3e573da5f084d5bc9036e8c8c626a1ffda8e51d04dc8166d6243556c678f
local deterministic SHA-256: 8c76a13c5283c8b43f560263d45c13b7f65d8d2070c7386d85513d905bb3198f
local W25Q128 SHA-256: 2dcc944f579383c9d82eb941187a5da4acb7c0abefea4b72c4c9078895b8a01d
payload: SeaBIOS rel-1.17.0 commit b52ca86e094d19b58e2304417787e96b940e39c6
payload source ELF SHA-256: dc41c8ecd4670f2b1be183905fe612736aa0516510e52acaf4356adb987c9029
CBFS-extracted payload ELF SHA-256: a471e4c8e1acc07e7d242d32efdb44b85117b3a99e41ee047cbbf6ccaa67d739
payload geometry: one segment 000f8800..000fffff, entry 000fecd6
build result: two clean fixed-epoch builds byte-identical; repository tests
  133/133; support-tool tests 28/28; portable C scenarios 8+5;
  CBFS/payload/local composition verified
hardware status: NOT RUN
```

B06VG replaces the fixed post-CSI assumption with two exact, disjoint
profiles. PRIMARY remains the B06VF promotion tuple: pre/post A0 `17000`,
CPU 9c `b00502`, and CSI byte/raw `08/03e3d24e`. OBSERVATION requires one
of the other five literal A0 observations to survive unchanged, CPU 9c
`a00502`, and exact CSI `08/03e3d24e` or `0c/8b38506a`. Crossed
byte/digest pairs, A0 drift, masked/range-derived values, and every other
endpoint fail closed. The pre-A0 value is sampled twice and independently
rechecked by the vendor-call layer immediately before CSI.

An OBSERVATION may execute exactly one persistently guarded MINIT call, emits
complete returned CSI and 11212-byte workspace telemetry, and then returns
to CAR ROMMON through `B06VG_OBSERVATION_MINIT_TERMINAL`. That branch cannot
record a handoff, test ordinary DRAM, enter postcar/ramstage, or start the
payload. Only PRIMARY can call the unchanged B06VF promotion function and
potentially produce POST `21,22,23,28,29,2a` plus a SeaBIOS serial banner.
Exact procedure, failure boundary, recovery commands, and hashes are in the
B06VG experiment document and manifest.

## 2026-09-05 — B06VF guard cleared before B06VG

```text
test ID: B06VF-PRE-B06VG-GUARD-CLEAR
image ID: X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905
transport: operator@serial-gateway.example.invalid, /dev/ttyUSB1, 115200 8N1
initial state: CMOS_DIAG_0E=ec VALID=00
command: unlock RESET
command: autoguard clear
read-back: CMOS_DIAG_0E=2c VALID=01
result: PASS; one-command reset arm consumed; ROMMON prompt returned
reset/power action: none
next requirement: complete G3/AC removal before B06VG
```

No CF9 reset, Shelly request, AC-power action, vendor call, or undocumented
register write was issued. The full short transcript is retained as
`research/msi/captures/2026-09-05-b06vf-pre-b06vg-guard-clear.txt`:

```text
06e81d1611a7f53715c91bb8764b824292703351d76b51958559d146927cb77a  2026-09-05-b06vf-pre-b06vg-guard-clear.txt  (1101 bytes)
```

## 2026-09-05 — B06VG G3-01..05 coupled MINIT observations

This entry appends the B06VG hardware result. It supersedes only the
`hardware status: NOT RUN` statement that was correct when B06VG was built;
the earlier build record and all artifact hashes remain unchanged.

```text
test ID: B06VG-HW-G3-01..05
image ID: X58PROE-B06VG-COUPLED-MINIT-SEABIOS-20260905
build commit: coreboot-fe3e08197177-dirty-x58-pro-e-b06vg
ROM hash: local W25Q128 image 2dcc944f579383c9d82eb941187a5da4acb7c0abefea4b72c4c9078895b8a01d; programmer read-back not recorded
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual rank
DIMM slot: sole responder at SPD 0x54; physical silkscreen not recorded here
GPU: unchanged single test GPU; exact model not recorded
PSU: not recorded
memory policy: DDR ratio 6; no XMP/overclocking
boot type: five user-directed G3-labelled attempts; G3-02..05 logs begin COLD_DEFAULT and retain both CSI reset continuations; G3-01 was observed live without a retained initial/raw log; independent power telemetry remains pending
POST trace: serial phase/call/fallback markers retained; separate complete POST-card trace not recorded
serial log: G3-02..05 immutable captures listed below; G3-01 has no retained file
result: one OBSERVATION MINIT return and two PRIMARY MINIT returns; all automatic paths remained fail-closed in CAR; no payload
recovery required: no chip recovery; CAR ROMMON remained available; the persistent guard was manually cleared after the G3-02/G3-04/G3-05 evidence and requires full AC removal before another attempt
notes: manual selected-address DRAM tests are distinct from and occurred after the automatic fallback
```

The five pass-3 observations were:

| Attempt | Saved/post A0 | CPU 9c | CSI byte/raw/canonical | Classifier and automatic result |
| --- | --- | --- | --- | --- |
| G3-01 | `17400/17400` | `b00502` | `0c/8b38506a/908dabb6` | crossed combination rejected before MINIT; live-only observation, no raw capture |
| G3-02 | `17a00/17a00` | `a00502` | `08/03e3d24e/908dabb6` | OBSERVATION; MINIT EAX zero; deliberate terminal after complete CSI/workspace dump |
| G3-03 | `17400/17400` | `b00502` | `08/03e3d24e/908dabb6` | crossed combination rejected before MINIT |
| G3-04 | `17000/17000` | `b00502` | `08/03e3d24e/908dabb6` | PRIMARY; MINIT EAX zero; post-MINIT exact gate rejected workspace P and retained CAR |
| G3-05 | `17000/17000` | `b00502` | `08/03e3d24e/908dabb6` | PRIMARY; MINIT EAX zero; post-MINIT exact gate rejected workspace N and retained CAR |

Thus the exact coupled classifier accepted its intended OBSERVATION and
PRIMARY combinations and rejected both observed crossed combinations. G3-02
reported `FULL_PATH_RETURN_DRAM_UNTESTED` but then, as required for an
OBSERVATION, printed `B06VG_OBSERVATION_TERMINAL` and did not call the
promotion function. Its complete `0x2bcc`-byte workspace has FNV-1a
`64e3c821` and SHA-256
`43c9e94ea2e13a42762aeb01ee837022d7ebfb2b901405cefc6f5d254099191e`.

G3-04 and G3-05 independently reached PRIMARY. In both runs pass-3 CSI
returned EAX/EBX/ECX `0/0/11`, MINIT returned EAX zero with the expected
self-consistent ABI state, and the exact platform endpoint after MINIT was:

```text
CPU ff:02.1 50/54/6c/80=160c0112/00000012/0040a0a8/070f0f03
CPU ff:02.1 94/9c/a0/a4=00010202/00b00502/00017000/00322808
IOH 00:0d.0 82c/840/854/85c/864=004060a0/070f0f03/00010102/00000002/00322808
LINK CPU50/58=86000000/00064555 IOH_C8=0616fc00 SR0/SR1=00000000/00000000 SYRE_CC=00000600
CPU ff:02.0 80/d0=0000fe91/00000501 PTR_LOW16_EXPECT=fe91 PTR_MATCH=01
post-MINIT IOH 00:14.1 9c=bf000000 MC50/54=0a000006/00000006
MC_MAP60=00024489 MC_F8=00001545 CH2_DOD=000002ac CH2_RANKS=00000003 CH2_STATUS=00000140
```

The complete workspaces introduced two new neutral forms. `P` and `N` are
only local labels and do not assert a register or algorithm meaning:

```text
P / G3-04:
  raw FNV-1a       eb15c076
  B06VF canonical c313e060
  SHA-256          37f2cf136cd7549f0558eed7e24cc68ddc89485887d48331bd01eb0e697e8681
  2459..2460       ff ff ff ff ff ff ff ff
  2461             ff
N / G3-05:
  raw FNV-1a       5fb636d9
  B06VF canonical b3fafec0
  SHA-256          dba44459fa45e5f6c3c16c7b2dd5908a59cba41cdd00e50e1cfb130b2eef219f
  2459..2460       00 00 00 00 00 00 00 00
  2461             ff
```

Offline parsing reconstructed exactly 11,212 bytes (`0000..2bcb`) from 701
records in each dump, with no gaps, overlaps, or conflicts. Neither workspace
matched B06VG's inherited raw-pattern/canonical gate. Both PRIMARY runs also
reported runtime probe `platform-state`, code `0d`, and therefore
`PROBE_STATUS_ADMITTED=00`.

All three MINIT returns printed
`POST_MINIT_GUARD I801_EXACT=00 CMOS0E=ec`. G3-04 and G3-05 then passively
read I801 offsets `402..406`, without touching read-to-clear host status at
`400`, and returned the same post-MINIT tuple twice:

```text
08:00:5d:01:a3
```

G3-02 did not perform that passive byte read. The two PRIMARY measurements
are direct evidence that vendor MINIT overwrites the committed pre-call
I801 marker `08:64:9b:5c:a3`; the CMOS `ec` guard did persist. The result is a
false negative for the current I801 post-return equality predicate, but it is
not authorization to bypass persistence. A successor should re-arm and verify
a dedicated return marker only after every other accepted MINIT-return check
and before any possible promotion.

### Separate manual bounded DRAM tests

After the automatic path had already stopped at a CAR prompt, G3-02, G3-04,
and G3-05 each ran the same sealed script with 14 reversible writes followed
by 14 exact assertions (`PROGRAM_FNV=0a85f9f6`). The addresses were:

```text
01000000 01000004 01000ffc 01001000 013ffffc 017ff000 017ffffc
02000000 02000004 02000ffc 02001000 023ffffc 027ff000 027ffffc
```

All writes and assertions completed, and reverse-order rollback verified every
saved original:

| Attempt/profile | Run transaction | Rollback transaction | Result |
| --- | --- | --- | --- |
| G3-02 / OBSERVATION | `4e9a06e3` | `636a402c` | `RUN=ok`, `ROLLBACK=ok`, `ROLLED_BACK=01` |
| G3-04 / PRIMARY P | `16f13368` | `120afb90` | `RUN=ok`, `ROLLBACK=ok`, `ROLLED_BACK=01` |
| G3-05 / PRIMARY N | `efec928d` | `9566b3b0` | `RUN=ok`, `ROLLBACK=ok`, `ROLLED_BACK=01` |

This is three manual, bounded selected-address tests and evidence for lack of
alias among those fourteen simultaneous values. G3-02 additionally read
`IA32_MTRR_DEF_TYPE=0000000000000800`, establishing the effective UC default
for that run; G3-04 and G3-05 did not repeat the MSR read, so cache-independent
access is not independently claimed for them. These are not B06VG's automatic
0--640-KiB or complete 16-MiB tests and do not establish full-DIMM stability.

Immutable raw captures:

```text
b45dc52b13930c43a6558710da5981f839aa1c3f7216ee9e77c99ba1ce6048c3  2026-09-05-b06vg-g3-02-minit-return.raw         (79502 bytes)
4d6333b4fd098997986f4609ca7b4d72237b2c1cf53deb6ca44452a1a2a84cfe  2026-09-05-b06vg-g3-02-minit-dram-complete.raw  (98514 bytes)
0864a1a0fa2cbbb1bc15025b9dfde3134642ec99b06ce04d6f3b985296de22c4  2026-09-05-b06vg-g3-02-final.raw                (99088 bytes)
f8efe15f7c084b0466a46932999acd6da751da59c8dd47a852a2cd531ffc2902  2026-09-05-b06vg-g3-03-cross-174-b-08.raw       (17864 bytes)
3eebb29f37708c750749b306cc1d9e53e7848082d5122e5d2241c6f731feb9f9  2026-09-05-b06vg-g3-04-primary-new-workspace.raw (63018 bytes)
50f410efbff9df6f85f4e6017cfd2f1d2d0aa1538cc2e5071ffdbd946f0d8544  2026-09-05-b06vg-g3-04-final.raw                (81419 bytes)
07f3df18dd9b7868ab545d548ca626fd5aac88f1ddf7808746afbfad58de96ae  2026-09-05-b06vg-g3-05-workspace.raw            (57905 bytes)
2821f5c4d3984608c98e2f0b3931042cc117b81413cc4d75a4c57b43aa98d404  2026-09-05-b06vg-g3-05-final.raw                (75943 bytes)
```

No automatic ordinary-DRAM test, CBMEM handoff, postcar, ramstage, coreboot
table construction, PAM/shadow proof, SELF load, SeaBIOS entry, PCI
enumeration, GPU output, storage path, or payload ran in any B06VG attempt.
The B06VG payload milestone therefore remains open.

## 2026-09-05 — B06VH exact PRIMARY workspace/rearm SeaBIOS build

```text
test ID: B06VH-BUILD-01
image ID: X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
config: configs/x58-pro-e-b06vh.config
config SHA-256: 8b80b11661bfc909bdcd7984a9f8f2c7f9dc524657ff095fb91f0a9aac46594e
generated .config SHA-256: d5fb26c18a6bfe5d5079be0a798cec54475653180ad6daeffe573d4258d29abb
fixed SOURCE_DATE_EPOCH: 1788602400
ROM hash: base d5f8420ccd997dd0ddbfb83ff53d73c877b7366bbcd5685d08cf11cedc6f8df4; local W25Q128 97a69436bead7f8b4732a9e7f2fb6e6b694a9012581a4c2239d070adf055a452
flash chip: intended spare socketed W25Q128.V..M, 16 MiB; not programmed for this record
board revision: intended MSI X58 Pro-E / MS-7522 revision 3.0; not run
CPU: intended Intel Xeon E5645, CPUID 000206c2; not run
CPU stepping: intended stepping 2; not run
microcode revision: intended 0000001f; embedded object statically verified, not run
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual rank; not run
DIMM slot: intended sole responder at SPD 0x54; physical silkscreen not recorded
GPU: intended unchanged single test GPU; exact model not recorded; no graphics expected
PSU: not recorded
boot type: NOT RUN
POST trace: not observed; expected PRIMARY suffix d3,d4,08,09,0a,0b,0c,0e,0f,21,22,23,28,29,2a
serial log: none; expected COM1 03f8, 115200 8N1
result: BUILT TWICE / BYTE-REPRODUCIBLE / STATIC VERIFIED / NOT HARDWARE TESTED
recovery required: none during build; known-good spare/full-AC procedure remains mandatory for a hardware run
notes: no flash, reset, power action, vendor call, ordinary-DRAM access, ramstage, or payload execution occurred for B06VH
```

Two complete `make clean`, `CCACHE_DISABLE=1`, fixed-epoch builds produced
byte-identical base ROMs, generated configurations, and SeaBIOS source ELFs.
The final Python suite passed 149/149 tests, including 10/10 focused B06VH
contracts. CBFS, payload geometry and loaded bytes, microcode, the four local
vendor ranges, seven-byte wrapper patch, and 16-MiB top placement were
verified.

B06VH promotes only B06VG PRIMARY and admits exactly four coupled workspace
forms: A `94299f43/a6f9c2e6`, B `b6346533/a6f9c2e6`, P
`eb15c076/c313e060`, and N `5fb636d9/b3fafec0`, each with its exact observed
marker/pattern bytes. OBSERVATION C remains terminal. Immediately after
MINIT, every candidate must freshly equal I801 `08:00:5d:01:a3` with CMOS
`ec`. Only after all PRIMARY CPU/SPD/CSI/policy/workspace/IMC/QPI/IOH/ABI and
return-state gates pass may the code write/read back `08:64:9b:5c:a3`, record
the version-5 result, and enter automatic memory tests. The rearmed guard is
finalized only after all destructive read-backs and the v5 CBMEM handoff
read-back.

The embedded SeaBIOS is the unchanged reduced serial-only probe: one
`000f8800..000fffff` load segment, entry `000fecd6`, no VGA/storage/USB/boot
support, and intended terminal message `Boot support not compiled in.` This
does not provide PCI enumeration or a first boot. Exact hypothesis, expected
trace, failure boundary, recovery sequence, and verified hashes are in the
[B06VH experiment document](b06vh-primary-workspace-rearm-seabios.md) and
[manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-05 — B06VH crossed PRIMARY/CSI safe fallback

```text
test ID: B06VH-HW-G3-01
image ID: X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 artifact 97a69436bead7f8b4732a9e7f2fb6e6b694a9012581a4c2239d070adf055a452; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0 target; not independently restamped in capture
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not recorded
GPU: unchanged test GPU; exact model not recorded; no graphics path executed
PSU: not recorded
boot type: retained sequence starts COLD_DEFAULT and contains both intended CSI reset continuations; initial user power action and complete G3/AC-off duration not independently captured
POST trace: no separate POST-card record; serial shows bootblock c3 on all three legs, PHASE=01/02/03, then B06VG_COUPLED_HIGH_CSI_GATE fallback
serial log: research/msi/captures/2026-09-05-b06vh-g3-01-cross-170-b-0c-final.raw, 18184 bytes, SHA-256 db83af5cc0aa58ef5e95d532ba02bc6daafc583a34dffc59f8445ef222f7d361
result: SAFE PRE-MINIT FALLBACK ONCE; High-QPI retained; no automatic DRAM or payload path
recovery required: no flash recovery; CAR ROMMON remained available; CMOS 0e remained ec and must be deliberately cleared before another true-G3 attempt
notes: a mistyped passive command `vinp` returned an error; later vinfo reported ROMMON_DIRTY=00, so no interactive register mutation occurred
```

The sequence completed pass 1 and its CSI-internal reset, then pass 2 with the
required `EAX/EBX/ECX=1/0/2a6` return and intended outer IOH SYRE reset. Pass
3 returned ABI-clean with the stable High-QPI endpoint but an unaccepted
coupling:

```text
pre/post CPU A0:         00017000 / 00017000
post-CSI CPU 9c:         00b00502
CSI byte/raw/canonical:  0c / 8b38506a / 908dabb6
QPI status:              070f0f03
MC 50/54:                0a000006 / 00000006
CSI return EAX/EBX/ECX:  0 / 0 / 11
```

A0 `17000` and CPU 9c `b00502` select B06VH PRIMARY, but its exact contract
requires CSI `08/03e3d24e/908dabb6`. The observed `0c/8b38506a/908dabb6`
belongs to the other coupled profile, so the firmware rejected the crossed
combination at `B06VG_COUPLED_HIGH_CSI_GATE` and returned to CAR before
MINIT. Follow-up `vinputs`/`vinfo` showed CMOS `0e=ec`, QPI
`070f0f03`, `MINIT_ATTEMPTED=00`, `MINIT_RETURNED=00`,
`WORK_FNV=00000000`, and `ROMMON_DIRTY=00`.

This run proves once that B06VH executes, completes both intended CSI reset
continuations, reaches High-QPI, and fails closed on this crossed state. It
does not test the A/B/P/N workspace gates, post-MINIT I801 return/re-arm,
ordinary DRAM, CBMEM, postcar, ramstage, or SeaBIOS. Because initial power
provenance is incomplete, it is not counted as a confirmed cold-boot
repetition.

## 2026-09-05 — B06VH controlled-15-second-G3 crossed-profile repetition

```text
test ID: B06VH-HW-G3-02
image ID: X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 artifact 97a69436bead7f8b4732a9e7f2fb6e6b694a9012581a4c2239d070adf055a452; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0 target
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not recorded
GPU: unchanged test GPU; exact model not recorded; no graphics path executed
PSU: not recorded
boot type: confirmed controlled G3 — guard 2c verified, Shelly 192.0.2.215 local timer held AC off 15 s, output restored, board remained S5 at about 0.8 W, then user pressed physical power button; serial armed throughout
POST trace: no separate POST-card record; one preliminary COLD_DEFAULT/PHASE=01 stopped after SPD54_TARGET_GATE, then a second COLD_DEFAULT completed PHASE=01/02/03 and both intended CSI resets before exact fallback
serial log: research/msi/captures/2026-09-05-b06vh-g3-02-controlled15s-cross-170-b-0c-final.raw, 20613 bytes, SHA-256 918e0db571dd6ab5a93e093238b0475b753300193dd9a9e6b24b512b179c9b6d
result: SAFE PRE-MINIT FALLBACK REPRODUCED; first confirmed controlled B06VH G3 attempt; no automatic DRAM or payload path
recovery required: no flash recovery; CAR ROMMON remained available; guard was subsequently cleared ec->2c for the next G3, outside the immutable capture
notes: later passive D31F0 offset a4 byte read returned 02 outside the final capture; bit 0 was already clear despite no automatic power-on after AC restoration
```

The initial power-return bytes include serial garbage. The first unusual
`COLD_DEFAULT`/phase-1 start completed `SPD54_TARGET_GATE=PASS` and then ended
without PCIEXBAR preflight or CSI arm. Its cause remains unexplained, and it
is neither counted as a second cold boot nor as one of the intended CSI
continuations. The subsequent start completed the expected three-phase
sequence and both CSI-produced reset continuations.

The full path repeated the exact G3-01 crossed endpoint: A0 remained
`00017000`, CPU 9c became `00b00502`, CSI byte/raw/canonical was
`0c/8b38506a/908dabb6`, QPI was `070f0f03`, MC 50/54 was
`0a000006/00000006`, and pass-3 returned `EAX/EBX/ECX=0/0/11`. The exact
classifier again stopped at `B06VG_COUPLED_HIGH_CSI_GATE` before MINIT.
Passive follow-up in the capture retained CMOS `0e=ec` and reported
`MINIT_ATTEMPTED=00`, `MINIT_RETURNED=00`, `WORK_FNV=00000000`, and
`ROMMON_DIRTY=00`.

This brings B06VH to one confirmed controlled G3 run out of ten and two total
retained safe crossed-profile fallbacks. It still provides no execution
evidence for MINIT, I801 return/re-arm, A/B/P/N workspace admission, DRAM,
CBMEM, postcar, ramstage, or SeaBIOS. The `a4=02` read confirms only the live
register byte; it does not identify why AC return left the board in S5.

## 2026-09-05 — B06VH second controlled-G3 crossed-profile fallback

```text
test ID: B06VH-HW-G3-03
image ID: X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 artifact 97a69436bead7f8b4732a9e7f2fb6e6b694a9012581a4c2239d070adf055a452; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0 target
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not recorded
GPU: unchanged test GPU; exact model not recorded; no graphics path executed
PSU: not recorded
boot type: confirmed controlled G3 — guard 2c verified, preceding Shelly cycle held AC off for 15 s, board remained S5 after restoration, user pressed physical power button, and serial capture was armed
POST trace: no separate POST-card record; preliminary COLD_DEFAULT/PHASE=01 stopped after SPD54_TARGET_GATE, second COLD_DEFAULT completed PHASE=01/02/03 and both intended CSI resets before exact fallback
serial log: research/msi/captures/2026-09-05-b06vh-g3-03-controlled15s-cross-174-b-0c-final.raw, 20614 bytes, SHA-256 a27d6dee458f2846a6912cf31f8d3318cf34198690dab18ce708ecc76a48c104
result: SAFE PRE-MINIT FALLBACK; second confirmed controlled B06VH G3 attempt; no automatic DRAM or payload path
recovery required: no flash recovery; CAR ROMMON remained available with CMOS 0e=ec
notes: same unexplained preliminary post-SPD restart as G3-02; complete sequence used A0 17400 rather than 17000
```

The preliminary phase-1 start ended before PCIEXBAR or CSI arm and is not
counted as another cold boot or one of the two intended CSI continuations. The
second start completed the full three-phase path. Pass 3 retained
saved/pre/post A0 `00017400`, returned CPU 9c `00b00502`, CSI
`0c/8b38506a/908dabb6`, QPI `070f0f03`, MC 50/54
`0a000006/00000006`, and `EAX/EBX/ECX=0/0/11`.

The exact coupled selector rejected this crossed state at
`B06VG_COUPLED_HIGH_CSI_GATE` before MINIT. Passive state remained CMOS
`0e=ec`, `MINIT_ATTEMPTED=00`, `MINIT_RETURNED=00`,
`WORK_FNV=00000000`, and `ROMMON_DIRTY=00`. The B06VH total is now two
confirmed controlled G3 attempts out of ten and three retained executions;
all three ended safely before MINIT. G3-03 adds a second observed A0 value for
the crossed endpoint but does not justify a selector change or prove I801
re-arm, DRAM, ramstage, or payload execution.

## 2026-09-05 — B06VH controlled-G3 OBSERVATION MINIT return

```text
test ID: B06VH-HW-G3-04
image ID: X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 artifact 97a69436bead7f8b4732a9e7f2fb6e6b694a9012581a4c2239d070adf055a452; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0 target
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not recorded
GPU: unchanged test GPU; exact model not recorded; no graphics path executed
PSU: not recorded
boot type: confirmed controlled G3 — guard 2c verified, Shelly held AC off for 60 s, board remained S5 after restoration, user pressed the physical power button, and serial capture was armed
POST trace: no separate POST-card record; recurring preliminary COLD_DEFAULT/PHASE=01 stopped after SPD54_TARGET_GATE, second COLD_DEFAULT completed PHASE=01/02/03 and both intended CSI resets; MINIT d3/d4 path returned, then mandatory OBSERVATION terminal in CAR
serial log: research/msi/captures/2026-09-05-b06vh-g3-04-controlled60s-observation-17c-0c-minit-return-final.raw, 74807 bytes, SHA-256 996117eb0992761bf1f4b195a28769f5bc573e34265d2ccb13db1ad062538621
result: OBSERVATION MINIT FULL-PATH RETURN; exact post-MINIT I801 tuple; terminal CAR by design; no ordinary DRAM or payload path
recovery required: no flash recovery; CAR ROMMON remained available and persistent guard remained retained
notes: this supersedes the prior shorthand that all B06VH non-PRIMARY outcomes stopped before MINIT; B06VH inherits B06VG's one-shot OBSERVATION diagnostic MINIT terminal
```

The recurring preliminary cold start again stopped after the SPD54 target
gate and before PCIEXBAR or CSI arm. It is recorded separately and is not
counted as another G3 run or intended CSI continuation. The following start
completed all three phases and both intended CSI resets. Pass 3 returned with:

```text
saved/pre/post CPU A0:   00017c00 / 00017c00 / 00017c00
post-CSI CPU 9c:         00a00502
CSI byte/raw/canonical:  0c / 8b38506a / 908dabb6
QPI status:              070f0f03
MC 50/54:                0a000006 / 00000006
CSI return EAX/EBX/ECX:  0 / 0 / 11
classification:          OBSERVATION
```

This exact non-PRIMARY profile did not enter B06VH's PRIMARY workspace/re-arm
promotion. Instead, the retained B06VG OBSERVATION path authorized its single
diagnostic MINIT invocation. MINIT returned `EAX=00000000`; the immediate
post-call gate observed I801 `08:00:5d:01:a3` with an exact match and CMOS
`0e=ec`. Policy remained unchanged at `3c0f3a0b`, the returned workspace FNV
was `50f67315`, and the firmware explicitly reported `DRAM_ACCESSES=00` and
`FULL_PATH_RETURN_DRAM_UNTESTED`.

The path then emitted `B06VG_OBSERVATION_TERMINAL` and
`AUTO_FALLBACK=B06VG_OBSERVATION_MINIT_TERMINAL` before returning to CAR. It
performed no I801 re-arm, ordinary-DRAM access or test, CBMEM, postcar,
ramstage, or SeaBIOS execution. In particular, MINIT returning is not evidence
that DRAM is trained or usable.

B06VH now has three confirmed controlled G3 attempts out of ten plus the
earlier run of incomplete power provenance. G3-02 and G3-03 stopped before
MINIT on crossed profiles; G3-04 exercised the intentionally retained
OBSERVATION MINIT terminal. The PRIMARY/re-arm/payload milestone remains open.

### B06VH-HW-G3-04-MANUAL-UC14 — reversible selected-address follow-up

This is a separate manual experiment performed from the retained CAR ROMMON
after G3-04's automatic OBSERVATION terminal. It does not alter the controlled
G3 count or convert the diagnostic MINIT return into an automatic DRAM claim.

```text
capture: research/msi/captures/2026-09-05-b06vh-g3-04-manual-uc14-rollback.raw
capture SHA-256: 66da964b8286e669f044829912fb37f84d72690ecd79b2f9c0a646d31b266764
capture size: 27470 bytes
execution context: same B06VH-HW-G3-04 CAR session, after OBSERVATION MINIT return
MTRR default: IA32_MTRR_DEF_TYPE 0000000000000800, effective default UC
result: 14 selected addresses wrote and asserted successfully; reverse rollback and independent anchor restoration passed
recovery required: none; transaction discarded after verified rollback
```

Before the memory transaction, direct reads reproduced the exact established
one-channel/High-QPI endpoint:

```text
MC mapper ff:03.0 +60:        00024489
MC common ff:03.4 +f8:        00001545
channel-2 DOD ff:06.1 +48:    000002ac
channel-2 ranks ff:06.0 +7c:  00000003
channel-2 status ff:06.0 +5c: 00000140
QPI ff:02.1 +80:              070f0f03
IOH 00:14.1 +9c:              bf000000
```

Two consecutive passive reads at each anchor were stable:

```text
01000000 = e82c919b / e82c919b
02000000 = 20e492d9 / 20e492d9
```

The sealed script replayed the established 14-address table across the
16--24-MiB and 32--40-MiB windows. It contained 14 reversible writes followed
by 14 full-mask assertions:

```text
PROGRAM_FNV=0a85f9f6
RUN=ok
MUT=0e
TXN_FNV=fc7d3978
assertions passed: 14/14
```

One reverse-order rollback attempt restored all recorded pre-values and
reported:

```text
ROLLBACK=ok
TXN_FNV=468d79b8
RB_TRIES=00000001
ROLLED_BACK=01
DISCARD=ok
```

Independent reads after discard restored the anchors to `e82c919b` and
`20e492d9`. QPI `070f0f03`, MC common `00001545`, and channel status
`00000140` were unchanged. The six sampled `ff:03.2` dwords at offsets
`0x80..0x94` were all zero.

The final `vinfo` correctly recorded `ROMMON_DIRTY=01` after the manual
writes. Its generic summary unexpectedly changed to `SPD_GATE=FAIL`,
`SPD_FNV=00000000`, and `PCIEXBAR_STEP=PENDING`. That mutable probe-summary
state is a bookkeeping caveat, not contradictory DRAM evidence: the direct
endpoint reads, memory transaction, all assertions, reverse rollback, and
independent restored-anchor reads are the evidence for this experiment.

This permits a next image to admit only the exact G3-04 OBSERVATION workspace
as a separately named O profile for a bounded automatic post-memory/payload
attempt. It does not prove complete-window or full-DIMM integrity, sustained
stability, native RAM initialization, or payload execution, and it must not be
described as validated DRAM training.

### B06VH-HW-G3-04-MANUAL-WALK1-D0-D31 — one-address data-bit test

This is another separate manual follow-up in the retained G3-04 CAR session.
It exercises all 32 data bits at one dword address only and adds no boot to
the controlled-G3 count.

```text
capture: research/msi/captures/2026-09-05-b06vh-g3-04-manual-walk1-d0-d31-rollback.raw
capture SHA-256: c72f080014609538ba26eec0dc27c2176445082d73bcda5be04f148a8a9ee3a4
capture size: 40659 bytes
target address: 01000000
pattern: walking one, D0 through D31, split into two 16-bit groups
result: both groups passed every immediate full-dword assertion and one-attempt reverse rollback
recovery required: none; both transactions were discarded after rollback
```

The first half covered D0--D15 with 16 reversible writes and 16 immediate
full-mask assertions:

```text
PROGRAM_FNV=08863aba
RUN=ok
MUT=10
run TXN_FNV=62282405
ROLLBACK=ok
post-rollback TXN_FNV=9ec9d1e2
RB_TRIES=00000001
ROLLED_BACK=01
```

For transparency, the first discard was intentionally or accidentally issued
with the wrong digest `8f2a4be6`. ROMMON rejected it safely with
`DISCARD=digest-mismatch`; the anchor already read back as `e82c919b`. The
subsequent discard using the exact `9ec9d1e2` digest returned `DISCARD=ok`.

The second half covered D16--D31 under the same write/assert/rollback shape:

```text
PROGRAM_FNV=356d86da
RUN=ok
MUT=10
run TXN_FNV=568a2f65
ROLLBACK=ok
post-rollback TXN_FNV=29046832
RB_TRIES=00000001
ROLLED_BACK=01
DISCARD=ok
```

The final independent read restored `01000000=e82c919b`. QPI
`070f0f03`, MC common `00001545`, and channel status `00000140` remained
unchanged. All six sampled `ff:03.2` dwords at offsets `0x80..0x94` remained
zero.

This is evidence for immediate walking-one behavior of D0--D31 at exactly one
selected address under the existing UC context, plus working transactional
rollback and digest protection. It is not an address-line test, full-window
test, retention test, sustained stability test, full-DIMM validation, or proof
of generally completed DRAM training.

## 2026-09-05 — B06VI coupled profile-O SeaBIOS release build

```text
test ID: B06VI-BUILD-01
image ID: X58PROE-B06VI-COUPLED-PROFILE-O-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
config: configs/x58-pro-e-b06vi.config
public base ROM: builds/experimental/msi-x58-pro-e-b06vi-coreboot-base-4MiB.rom
public base SHA-256: 745ea8744966f9a50c49be5265f091d8ccb294e7e68b23293247ae213a004752
flashable local W25Q128 ROM: blobs-local/msi-x58-pro-e/b06vi/msi-x58-pro-e-b06vi-deterministic-w25q128-16MiB.rom
flashable local SHA-256: 738b5a67c299eeb8862ab2e753d1801a2819966f7cbf25cd47a090574aaf28d8
flash chip target: W25Q128.V..M, 16 MiB, 4-MiB firmware top-aligned at 0x00c00000
board revision: not recorded for build-only event
CPU target: Intel Xeon E5645, CPUID 000206c2 stepping 2
microcode target: revision 0000001f
DIMM target: one known 4-GiB DDR3 DIMM, sole SPD responder 0x54, SPD FNV fb66b530
GPU: unchanged fixed test GPU; exact model must be recorded at execution
PSU: unchanged fixed test PSU; exact model must be recorded at execution
boot type: NOT RUN
POST trace: none
serial log: none
result: BUILT TWICE / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED / LOCAL COMPOSITE VERIFIED / NOT HARDWARE TESTED
recovery required: no
notes: no flash, programmer read-back, reset, power action, vendor call, DRAM access, postcar, ramstage, or payload execution occurred for B06VI
```

B06VI preserves the four exact B06VH PRIMARY rows and adds only profile O,
coupling saved/post-MINIT A0 `00017c00`, CPU 9c `00a00502`, CSI
`0c/8b38506a/908dabb6`, workspace `50f67315/92c70df4`, and profile ID 5.
Crossed rows, 40 single-field mutations, false IDs, and the VI-disabled VH
wire contract are executable negative tests. The O-only provisional
`PLATFORM_STATE` exception requires the exact raw O digest, canonical O
digest, and marker pattern before the complete common gate is reevaluated.

The version-6 handoff is 160 bytes and carries the selected profile plus saved
and post-MINIT CPU endpoints. For O, I801 remains at MINIT's measured
`08:00:5d:01:a3` until complete low-memory, alias, two full 8-MiB window,
CBMEM, and handoff read-back tests pass. Only then is `08:64:9b:5c:a3`
recreated and verified; the common finalizer clears it and restores CMOS
diagnostic `0x0e=2c` before postcar.

Two independent ccache-disabled fixed-epoch clean builds produced identical
base ROMs and SeaBIOS source ELFs. The final 157/157 Python suite passed. CBFS
extraction reconstructed an ELF32/i386 payload with one RWE segment at
`000f8800..000fffff`, entry `000fecd6`; all 30,720 loaded bytes match the
source SeaBIOS segment. The local composer and wrapper verifiers passed. The
16-MiB image has an all-`ff` lower 12 MiB (SHA-256
`6747318cfda6f6bb9e77ee1c229d37b1799610bc1d41e5165cc40ccf2363a7c4`)
and an upper 4 MiB byte-identical to the deterministic composite.

The user explicitly authorized treating RAM as provisionally stable for this
first postcar/ramstage/reduced-SeaBIOS attempt. This does not supersede the
normal ten-cold/ten-warm/extended-memory validation requirement. See the
[B06VI manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) and
[experiment procedure](b06vi-coupled-profile-o-seabios.md).

## 2026-09-05 — retained B06VH guard cleanup before B06VI

```text
test ID: B06VH-G3-04-GUARD-CLEAR
active image ID: X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905
active full-chip artifact SHA-256: 97a69436bead7f8b4732a9e7f2fb6e6b694a9012581a4c2239d070adf055a452 (not a programmer read-back)
flash chip: socketed W25Q128.V..M
board revision: not re-read in retained session
CPU: Intel Xeon E5645, CPUID 000206c2 stepping 2
microcode revision: 0000001f
DIMM: unchanged sole SPD-0x54 test DIMM
GPU: unchanged
PSU: unchanged
boot type: no new boot; retained B06VH-HW-G3-04 CAR ROMMON session
POST trace: none added
serial result: CMOS_DIAG_0E ec -> 2c, VALID=01; I801 402..406 08:00:5d:01:a3 -> 08:00:00:00:00
result: persistent CMOS guard and idle I801 phase cookie cleared and read back
recovery required: no
notes: no reset, CF9 request, AC action, flash write, vendor call, SMBus START, DRAM write, or payload entry
```

The existing ROMMON command sequence `unlock RESET` followed by `autoguard
clear` returned the explicit `0xec/0xed->0x2c` success line. A subsequent
`vinputs` reported `CMOS_DIAG_0E=2c VALID=01`. The idle I801 host registers
still contained MINIT's return cookie, so three separately armed one-byte
writes cleared only `XMITADD`, `HSTDAT0`, and `HSTDAT1`; `HSTCTL` remained
`0x08` with START clear and `HSTCMD` was already zero. Independent reads of
ports `0x402..0x406` then returned the source-defined clear signature
`08:00:00:00:00`. A final `vinputs` again confirmed `2c/VALID=01`.

This prepares persistent state only. The B06VI first attempt still requires
the documented complete G3/AC removal; this retained live session is not a
cold-boot substitute.

## 2026-09-05 — B06VI first hardware execution: crossed pre-MINIT tuple

```text
test ID: B06VI-HW-01
image ID: X58PROE-B06VI-COUPLED-PROFILE-O-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
active full-chip artifact SHA-256: 738b5a67c299eeb8862ab2e753d1801a2819966f7cbf25cd47a090574aaf28d8 (expected local artifact; no programmer read-back recorded)
flash chip: socketed W25Q128.V..M, 16 MiB target; not re-read in this session
board revision: MSI X58 Pro-E target; not re-read in this session
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: unchanged; exact model not restamped; no B06VI graphics path
PSU: unchanged; exact model not restamped
boot type: user reported the new build active; serial labelled COLD_DEFAULT, but the initial AC/G3 and programmer-read-back provenance were not independently captured
POST trace: no separate POST-card trace; source-defined fail-closed code is 20, while the complete serial trace is authoritative
serial log: research/msi/captures/2026-09-05-b06vi-hw-01-cross-170-0c.raw, 21547 bytes, SHA-256 82f986fce343e443d71dd7d27ae13b0bd1e2348c1caf0cf088841dcdb152b77e
result: SAFE FAIL-CLOSED CAR FALLBACK at B06VG_COUPLED_HIGH_CSI_GATE; no MINIT, ordinary-DRAM access, CBMEM, postcar, ramstage, or payload
recovery required: no flash recovery; CAR ROMMON remained available
notes: one unexplained preliminary COLD_DEFAULT phase-1 start ended after the SPD target gate; the following complete execution performed both intended CSI reset continuations and reached phase 3
```

The complete execution reproduced the intended first two legs: phase 1
entered from zeroed I801 state and requested the CSI-internal SYRE reset;
phase 2 returned `EAX/EBX/ECX=1/0/2a6`, passed the exact acceptance gate, and
issued the one IOH SYRE edge. Phase 3 then returned ABI-clean with
`EAX/EBX/ECX=0/0/11`, QPI `070f0f03`, CSI byte/digests
`0c/8b38506a/908dabb6`, CPU A0 `00017000`, CPU 9c `00b00502`, IOH stage 9c
`ea000000`, and memory-clock registers `0a000006/00000006`.

That is a crossed row: B06VI permits the `17000/b00502` PRIMARY endpoint only
with CSI `08/03e3d24e`, while CSI `0c/8b38506a` belongs to O only with the
`17c00/a00502` endpoint. The automatic path therefore rejected it before
policy construction or MINIT. Passive ROMMON evidence confirmed valid module
signatures and canaries, no accepted CSI profile, no MINIT attempt, zero
policy/workspace digests, `ROMMON_DIRTY=00`, and unchanged endpoints. The
persistent guard was subsequently cleared from `ec` to `2c` and read back,
outside the immutable capture.

A controlled Shelly cycle then removed AC for 60 seconds and restored it.
The board remained in S5 at approximately 0.8 W, matching the already
documented board-specific power-restore limitation, so this preparation is
not counted as a second B06VI execution until a physical power-button start
occurs.

## 2026-09-05 — B06VI controlled-G3 exact-O MINIT return, new workspace rejected

```text
test ID: B06VI-HW-G3-02
image ID: X58PROE-B06VI-COUPLED-PROFILE-O-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
active full-chip artifact SHA-256: 738b5a67c299eeb8862ab2e753d1801a2819966f7cbf25cd47a090574aaf28d8 (expected local artifact; no programmer read-back recorded)
flash chip: socketed W25Q128.V..M, 16 MiB target; not re-read in this session
board revision: MSI X58 Pro-E target; not re-read in this session
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: unchanged; exact model not restamped; no B06VI graphics path
PSU: unchanged; exact model not restamped
boot type: controlled Shelly 60-second G3, AC restore into S5, then user-confirmed physical power-button start; serial armed before start
POST trace: no separate POST-card trace; complete serial trace is authoritative
serial log: research/msi/captures/2026-09-05-b06vi-g3-02-controlled60s-o-workspace-69b4c386.raw, 69300 bytes, SHA-256 69cfca6197da1f5d5db379c69b4adf5b25a3246b30c50de085297efe7cd222ff
result: EXACT O PRE-MINIT PROFILE AND GUARDED MINIT RETURN; NEW EXACT WORKSPACE REJECTED FAIL-CLOSED BEFORE ANY ORDINARY DRAM ACCESS
recovery required: no flash recovery; CAR ROMMON remained available; persistent guard subsequently cleared through the dedicated command
notes: one recurring unexplained preliminary COLD_DEFAULT phase-1 start ended after the SPD gate and is not counted separately
```

The complete counted sequence performed both designed CSI/reset
continuations. Phase 3 returned ABI-clean with CSI raw/canonical FNVs
`8b38506a/908dabb6`, CSI byte `2a6=0c`, saved/post-CSI CPU A0 `00017c00`,
and CPU 9c `00a00502`; B06VI therefore selected exact profile O. The exact
seven-edit policy `3c0f3a0b` was installed, the guarded MINIT call returned
with EAX zero, and I801 changed from the consumed tuple
`08:42:bd:7a:85`, through `08:64:9b:5c:a3`, to the measured return tuple
`08:00:5d:01:a3`. Wrapper, CSI, MINIT, ABI, stack, and canary checks remained
valid.

The post-MINIT endpoint was the expected High-QPI/one-channel form: QPI
`070f0f03`, CPU A0/9c `00017c00/00a00502`, IOH stage `bf000000`, MC50/54
`0a000006/00000006`, mapper/f8 `00024489/00001545`, and channel
DOD/ranks/status `000002ac/00000003/00000140`. The vendor probe still
returned the known O-family `platform-state` status.

The returned workspace was a previously unadmitted exact variant: raw FNV
`69b4c386`, B06VF-canonical FNV `0b161f01`, and full SHA-256
`6c29393a9bdde219319a4f15bf724c005843cc53e3ade6671568c75ceaaeb529`.
The full `0x2bcc` reconstruction has no gaps or duplicates and retains the
completion bytes `workspace[1]=00`, `[2]=00`, `[4f]=02`, and `[e79]=01`.
Compared with the older O workspace, 58 bytes differ: 49 inside the existing
canonical ranges and nine outside them. B06VI therefore kept
`RAW_PATTERN_GATE=00`, refused the provisional `platform-state` exception,
and stopped at `B06VI_COUPLED_POST_MINIT_EXACT_GATE` with
`DRAM_ACCESSES=00`. No low-memory, alias, 8-MiB window, CBMEM, postcar,
ramstage, or payload code ran.

ROMMON then captured all 11,212 workspace bytes, all 772 CSI bytes, all 224
policy bytes, the I801 tuple, CPU/QPI/IOH/MC endpoints, and finished with
`ROMMON_DIRTY=00`. After the immutable boot capture was closed,
`unlock RESET` plus `autoguard clear` changed CMOS `0xec -> 0x2c` and
`vinputs` read it back. The I801 return tuple was deliberately left in place
for the required complete AC removal. The separate cleanup transcript is
`research/msi/captures/2026-09-05-b06vi-g3-02-postcapture-guard-clear.txt`,
514 bytes, SHA-256
`dc2bccab9a3a0eed77358cdb0541ed99862a367124924ee6b646a8c99b13c1be`.

## 2026-09-05 — B06VJ exact profile-Q SeaBIOS release build

```text
test ID: B06VJ-BUILD-01
image ID: X58PROE-B06VJ-COUPLED-PROFILE-Q-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
config: configs/x58-pro-e-b06vj.config
public base ROM: builds/experimental/msi-x58-pro-e-b06vj-coreboot-base-4MiB.rom
public base SHA-256: 246b141930ba9b3f792558d6c785e58b85a36f10ba6f14269d5113d27a5f9eaa
local unpatched 4-MiB SHA-256: 596ea4c12c2691cd6e20f6f3a736a3f18e4a6629b3c434601989b26988108279
local deterministic 4-MiB SHA-256: 4073ffdfcbd94c56448e5407b6c3ef1e7414eb54e94a049ab59593772c0a1132
flashable local W25Q128 ROM: blobs-local/msi-x58-pro-e/b06vj/msi-x58-pro-e-b06vj-deterministic-w25q128-16MiB.rom
flashable local SHA-256: f1b282319e6c6a3289ed59933fcf2eff788da4a4fe70e9e3db91360055e7805c
flash chip target: W25Q128.V..M, 16 MiB, 4-MiB firmware top-aligned at 0x00c00000
board revision: not recorded for build-only event
CPU target: Intel Xeon E5645, CPUID 000206c2 stepping 2
microcode target: revision 0000001f
DIMM target: BLS4G3D1609DS1S00., 4096 MiB dual-rank x8, sole SPD responder 0x54, SPD FNV fb66b530
GPU: unchanged fixed test GPU; exact model must be recorded at execution
PSU: unchanged fixed test PSU; exact model must be recorded at execution
boot type: NOT RUN
POST trace: none
serial log: none
result: BUILT REPEATEDLY / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED / B06VI REGRESSION BUILT / LOCAL COMPOSITE AND PAYLOAD VERIFIED / NOT HARDWARE TESTED
recovery required: no
notes: no flash, programmer read-back, reset, power action, vendor call, DRAM access, postcar, ramstage, or payload execution occurred for B06VJ
```

B06VJ preserves the five exact B06VI rows and adds only profile Q, coupling
saved/post-CSI/post-MINIT A0 `00017c00`, CPU 9c `00a00502`, CSI
`0c/8b38506a/908dabb6`, workspace `69b4c386/0b161f01`, 26 exact workspace
marker bytes, and profile ID 6. The canonicalization ranges were not changed.
The same six-row table is consumed by romstage and ramstage. Cartesian
crosses, all 48 single-field tuple mutations, all 26 Q-marker mutations, and
wrong IDs fail closed. With B06VJ disabled, Q is rejected and the complete
B06VI version-6 contract still compiles and passes its tests.

The version-7 handoff remains 160 bytes. Both O and Q retain MINIT's measured
`08:00:5d:01:a3` through the exact-result/MTRR gates, complete low-memory test,
14-address alias transaction, two complete 8-MiB tests, CBMEM creation, and
handoff read-back. Only then may the I801 marker be rearmed and the common
finalizer restore CMOS `0x0e=2c` before postcar.

Two complete ccache-disabled fixed-epoch B06VJ clean builds produced identical
public and local artifact hashes; a separate B06VI regression build also
completed. The final 163/163 Python suite and 14/14 focused B06VI+B06VJ suite
passed. CBFS extraction reconstructed an ELF32/i386 payload with one RWE
segment at `000f8800..000fffff`, entry `000fecd6`; all 30,720 loaded bytes
match the source SeaBIOS segment. The local composer and seven-byte wrapper
verifiers passed. The 16-MiB image has an all-`ff` lower 12 MiB (SHA-256
`6747318cfda6f6bb9e77ee1c229d37b1799610bc1d41e5165cc40ccf2363a7c4`)
and an upper 4 MiB byte-identical to the deterministic composite.

The retained B06VI session already cleared CMOS `0xec -> 0x2c`, but its idle
I801 return tuple remains. The B06VJ hardware attempt therefore still requires
a complete G3/AC removal after flashing. See the
[B06VJ manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) and
[experiment procedure](b06vj-coupled-profile-q-seabios.md).

## 2026-09-05 — B06VJ exact O-family MINIT return, new raw workspace rejected

This entry appends the first B06VJ hardware result. It does not rewrite the
earlier build-only record, which was correct when produced.

```text
test ID: B06VJ-HW-01
image ID: X58PROE-B06VJ-COUPLED-PROFILE-Q-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 artifact f1b282319e6c6a3289ed59933fcf2eff788da4a4fe70e9e3db91360055e7805c; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB target; not re-read in this session
board revision: MSI X58 Pro-E / MS-7522 target; revision not re-read in this session
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: unchanged; exact model not restamped; no B06VJ graphics path ran
PSU: unchanged; exact model not restamped
boot type: user reported the new build active; serial labelled COLD_DEFAULT and retained both CSI-produced continuations, but initial AC/G3 provenance was not independently captured
POST trace: no separate POST-card trace; serial phase/call/fallback trace retained
serial log: research/msi/captures/2026-09-05-b06vj-hw-01-o-workspace-dd61cb51.raw, 68509 bytes / 1285 lines, SHA-256 088740f2e693d059487198b93d4e49f1a139085b330d79698efd90b51255dfbf
result: EXACT O-FAMILY CPU/CSI/POLICY AND GUARDED MINIT RETURN; NEW RAW WORKSPACE REJECTED FAIL-CLOSED BEFORE ANY ORDINARY DRAM ACCESS
recovery required: no chip recovery; CAR romstage ROMMON remained available and clean
notes: recurring preliminary phase-1 fragment not counted separately; serial slice begins one byte after the first opening bracket; no Shelly/reset/power action during capture or cleanup
```

The complete counted path performed both designed CSI reset continuations.
Pass 3 returned ABI-clean with EAX/EBX/ECX `0/0/11`, saved/post-CSI A0
`00017c00`, CPU 9c `00a00502`, and CSI byte/raw/canonical
`0c/8b38506a/908dabb6`. B06VJ therefore selected the O-family endpoint. The
exact policy `3c0f3a0b` was installed and MINIT returned with EAX zero, intact
wrapper/CSI/MINIT signatures, ABI, stack, and canaries. Its measured I801
return tuple was `08:00:5d:01:a3`; CMOS diagnostic byte `0x0e` was `0xec`.

The post-MINIT platform endpoint was internally consistent with the earlier Q
observation:

```text
QPI ff:02.1 +80:           070f0f03
IOH 00:14.1 +9c:           bf000000
MC common 50/54:           0a000006 / 00000006
MC mapper 60 / common f8:  00024489 / 00001545
channel-2 DOD/ranks/status: 000002ac / 00000003 / 00000140
```

The complete workspace instead had raw FNV `dd61cb51`, canonical FNV
`0b161f01`, and SHA-256
`370ca2366b44bfc66882f286686600851a60bf834ae5c79c249ba365ad845cef`.
The offline parser reconstructed all 11,212 bytes from 701 records without a
gap, duplicate, or conflict. The same capture also reconstructs the complete
CSI state and policy:

```text
CSI:    772 bytes, FNV 8b38506a, SHA-256 ed71cba07f00d61cef6cf4c132877bb323b84f32c15622d737998ea705e4d8a7
policy: 224 bytes, FNV 3c0f3a0b, SHA-256 6de724da5a46e5602e7b7c22d4f190101a22425ae343b3b7ab8c270cabc8ec29
```

Against B06VJ's admitted Q workspace `69b4c386/0b161f01`, the new workspace
differs at exactly 16 offsets:

```text
1336 1338 1346 134c 1350 1358 1360 136a 136c
13c8 13ce 13f0 13fc 242d 24bd 252d
```

All 16 lie inside the existing canonicalization ranges; there are zero changes
outside. The four common completion bytes remained `00/00/02/01`, and all 26
Q-specific marker bytes were byte-identical to the admitted Q workspace. The
runtime's `RAW_PATTERN_GATE=00` is a combined short-circuit result after the
raw-digest miss and therefore does not independently report a marker mismatch.
Offline comparison shows that the non-enumerated raw digest was the only
Q-row discriminator that changed.

B06VJ consequently retained `PROBE_STATUS_ADMITTED=00` for the vendor probe's
known `platform-state` result and stopped at
`B06VJ_COUPLED_POST_MINIT_EXACT_GATE`. It explicitly reported
`DRAM_ACCESSES=00`. No low-memory test, alias transaction, 8-MiB window test,
CBMEM creation, I801 post-memory re-arm, postcar, DRAM-backed ramstage, SeaBIOS
or graphics path ran. The surviving console was still coreboot's CAR romstage
ROMMON. Final `vinfo` and `script status` showed all module/canary/return flags
intact, `ROMMON_DIRTY=00`, and an empty transaction engine.

The immutable hardware file is a byte slice from the cumulative console log.
Its selected start omits only the first opening `[` of the initial B06VJ
banner, so it is not called byte-complete from the opening marker. This does
not affect the independently complete 701-record workspace, 49-record CSI, or
14-record policy reconstructions.

After the immutable boot capture, a first batched UART injection fragmented
`unlock RESET` and `autoguard clear` into partial commands. ROMMON rejected the
fragments without action. Sending each command individually then succeeded:

```text
unlock RESET
autoguard clear
vinputs -> CMOS_DIAG_0E=2c VALID=01
```

The cleanup changed the persistent guard from `0xec` to `0x2c`; it issued no
reset or power action. Its immutable 573-byte transcript is
`research/msi/captures/2026-09-05-b06vj-hw-01-postcapture-guard-clear.raw`,
SHA-256
`9ee226a8d40563a5f45850c18a4f13223a98becba8f01feb176705f4e61c4d7d`.

No Shelly request or AC cycle was used for B06VJ-HW-01 or its cleanup. For any
future lab power cycle, the current operator constraint is to address the
Shelly directly at `192.0.2.215` and use `OFF -> 1 second -> ON`, not the
older five-second timing. Power-on-after-AC-loss is not presently reliable;
the board may remain in S5 and require a physical power-button start.

## 2026-09-05 — B06VK profile-Q canonical-class SeaBIOS release build

```text
test ID: B06VK-BUILD-01
image ID: X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
config: configs/x58-pro-e-b06vk.config
config SHA-256: cc1d16f94c93f79f2124a16918caf3e303f1a0b75e5d150613994ea8c6a3f779
generated .config SHA-256: 05ab4d35dcd02c51b40df31e943ab6144dcf1128d054f77674dce851cc57de21
public base ROM: builds/experimental/msi-x58-pro-e-b06vk-coreboot-base-4MiB.rom
public base SHA-256: 594ff8ed582c81799269e66edb1c896c9909f65bbb42e29dc7e616aa58d1e53d
local unpatched 4-MiB SHA-256: 9864055f60f81880f36e0b4c6877c95bd10e503ed89eea1f42fd2cb7bade8328
local deterministic 4-MiB SHA-256: 9863beac69de51a473d99ffbbf60a2c05f7a7aae3cebbc5f12887782a1c2435e
flashable local W25Q128 SHA-256: 51db5981f881e0cac7155543a8e8e620ac6cad9803137bfdc44dce8a1cbd3864
flash chip target: W25Q128.V..M, 16 MiB, 4-MiB composite top-aligned at 0x00c00000
board revision: not recorded for build-only event
CPU target: Intel Xeon E5645, CPUID 000206c2 stepping 2
microcode target: revision 0000001f
DIMM target: BLS4G3D1609DS1S00., 4096 MiB dual-rank x8, sole SPD 0x54, SPD FNV fb66b530
GPU: unchanged fixed test GPU; exact model must be recorded at execution
PSU: unchanged fixed test PSU; exact model must be recorded at execution
boot type: NOT RUN
POST trace: none
serial log: none
result: BUILT REPEATEDLY / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED / B06VI+B06VJ REGRESSIONS BUILT / LOCAL COMPOSITE AND PAYLOAD VERIFIED / NOT HARDWARE TESTED
recovery required: no
notes: no flash, programmer read-back, reset, Shelly request, AC/power action, vendor call, DRAM access, postcar, ramstage, or payload execution occurred for B06VK
```

B06VK preserves B06VJ's six-row producer/consumer truth table and changes one
Q/ID-6 predicate only. Q's raw workspace FNV is still calculated, logged as
telemetry, stored in the handoff and integrity-covered, but it no longer
selects or validates Q. Canonical workspace FNV `0b161f01`, all 26 Q markers,
the four common completion bytes, and every CPU, CSI, policy, I801/CMOS, MTRR,
QPI, IOH and MC gate remain exact. PRIMARY A/B/P/N and O retain exact raw
workspace digests. No canonical range, raw row or wildcard was added.

The handoff advances to wire version 8. If the exact Q canonical class recurs,
the first novel hardware action is the bounded UC sequence covering low
memory, the reversible alias table, both complete 8-MiB windows, CBMEM creation
and exact v8 handoff read-back. Only success throughout may re-arm I801,
finalize the guard, enter postcar, relocate ramstage and start the reduced
serial-only SeaBIOS probe. That payload is deliberately non-booting and has no
graphics, storage, USB, network, TFTP, SSH or OS-boot support.

The completed release passed 170/170 full tests and 21/21 focused
B06VI/B06VJ/B06VK tests. Complete B06VI and B06VJ regression builds and
repeated byte-identical clean B06VK builds succeeded. Public/private boundary,
CBFS payload, wrapper patch, lower-12-MiB erase state and W25Q128 top placement
were verified. These are static and artifact results only.

The retained B06VJ cleanup verified CMOS diagnostic byte `0x0e=0x2c`, but no
B06VK start or G3 provenance exists. If a future test needs a Shelly cycle,
use only direct `192.0.2.215` control with `OFF -> exactly 1 second -> ON`.
Power-on-after-AC-loss is not reliable; a physical power-button start may be
required. See the [B06VK experiment](b06vk-q-canonical-class-seabios.md) and
[manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-05 — B06VK seven-run hardware campaign, no admitted handoff

This entry supersedes only B06VK's earlier build-only hardware status. The
release artifact and static validation record above remain unchanged.

```text
test IDs: B06VK-HW-01 through B06VK-HW-07
image ID: X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 artifact 51db5981f881e0cac7155543a8e8e620ac6cad9803137bfdc44dce8a1cbd3864; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not re-read
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: unchanged; exact model not restamped; no graphics path ran
PSU: unchanged; exact model not restamped
boot type: HW-01..04 Shelly 1-second interruptions, not claimed as discharged G3; HW-05 controlled 15-second G3 plus manual S5 start; HW-06..07 controlled 60-second G3 plus manual S5 start
POST trace: serial phase markers retained; no independent POST-card trace
serial logs: immutable files and hashes listed below
result: SEVEN SAFE CAR TERMINALS; ONE OBSERVATION MINIT RETURN; ZERO ORDINARY DRAM ACCESSES; ZERO CBMEM/POSTCAR/RAMSTAGE/SEABIOS ENTRIES
recovery required: no flash-chip recovery; CAR ROMMON survived every admitted terminal
notes: guard cleared and read back as CMOS 0x0e=0x2c after HW-07; no reset followed cleanup
```

The exact run matrix was:

| Run | Power | CPU/CSI result | MINIT and terminal |
|---|---|---|---|
| HW-01 | Shelly 1 s | `17a00/a00502`, CSI `08/03e3d24e` | EAX-zero return, workspace `0395518f/95abbb4b`; mandatory OBSERVATION terminal |
| HW-02 | Shelly 1 s | `17600/b00502`, CSI `08/03e3d24e` | crossed row rejected before MINIT |
| HW-03 | Shelly 1 s | `17600/b00502`, CSI `08/03e3d24e` | crossed row rejected before MINIT |
| HW-04 | Shelly 1 s | `17600/b00502`, CSI `08/03e3d24e` | crossed row rejected before MINIT |
| HW-05 | 15 s G3, manual start | `17000/b00502`, CSI `0c/8b38506a` | crossed row rejected before MINIT |
| HW-06 | 60 s G3, manual start | stable pre-CSI `A0=17200` | old pre-A0 set rejected before pass-3 CSI |
| HW-07 | 60 s G3, manual start | `17000/b00502`, CSI `0c/8b38506a` | crossed row rejected before MINIT |

HW-01's MINIT return retained EAX zero, exact policy and I801 return, clean
signatures/ABI/canaries, High-QPI `070f0f03`, and the known MC endpoint. Its
workspace nevertheless was not profile Q: canonical FNV was `95abbb4b`, and
12 of Q's 26 marker bytes differed. It therefore supplies no evidence for the
B06VK Q-canonical relaxation. Runs 02--05 and 07 show that independently known
CPU and CSI values frequently recombine into rows excluded by the exact
coupled truth table. HW-06 adds one stable `17200` pre-CSI observation but no
post-CSI or MINIT evidence for that value.

```text
41d57ef04f3363e92d21f18386b105a1e6d2f055e25b6f4a436861174ce5ce24  2026-09-05-b06vk-hw-01-observation-17a-workspace-0395518f.raw  (71877 bytes)
2c5bfbed4d95b5de488ed4decd75959993d2dd791bb19e3ce3ea8596e0074f37  2026-09-05-b06vk-hw-02-cross-176-b-csi08.raw                    (16494 bytes)
5a7c390b25b1b1dc17eda1b492b79a0cee39bc20c2340a5b9abd9d5fa58241e9  2026-09-05-b06vk-hw-03-cross-176-b-csi08.raw                    (16493 bytes)
f225bea3bde7ba85153da752bb220683d0c2c605fde13ece6b05f14722927329  2026-09-05-b06vk-hw-04-cross-176-b-csi08.raw                    (16491 bytes)
b3770a9fba2678a7476470b4dab5120984d5ea8b7003fbbb02d98bb872484d30  2026-09-05-b06vk-hw-05-g3-15s-cross-170-b-csi0c.raw             (19498 bytes)
266a57546feabd00d1e09dc9ff6e64170e245eb76a7cd8af7752b157ea821cf3  2026-09-05-b06vk-hw-06-g3-60s-pre-a0-172-gate.raw               (12784 bytes)
f2e9f01c1bdad7043e6e239d9143a4406bc62da36d7d7a65cdf57e0d87741dad  2026-09-05-b06vk-hw-07-g3-60s-cross-170-b-csi0c.raw             (19498 bytes)
```

All paths reported or structurally guaranteed `DRAM_ACCESSES=00`. No run
created CBMEM, entered postcar, relocated ramstage, started SeaBIOS or touched
graphics. The operator subsequently requested a separate, explicitly broader
successor which may admit observed crossed CPU/CSI rows to one guarded MINIT
call and use destructive DRAM read-back, rather than workspace/profile hashes,
as the final pre-payload discriminator. That request does not retroactively
turn any B06VK result into a memory-training or payload success.

## 2026-09-05 — B06VL broad hard-gated SeaBIOS release build

```text
test ID: B06VL-BUILD-01
image ID: X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base dd9567411ccd4abdbe5b3efdaadef63d1192cef20d3dbfa421c9962df05c7a58; local W25Q128 candidate 9f8e1085d09207073983bbb3e26ce51474ced604e020a4206406e1a485c3f5e7
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not re-read
CPU: intended Intel Xeon E5645, CPUID 000206c2; not run
CPU stepping: intended stepping 2; not run
microcode revision: embedded target 0000001f; not executed
DIMM model: intended BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54; physical silkscreen not restamped
GPU: unchanged target; exact model not restamped; graphics disabled in this build
PSU: unchanged target; exact model not restamped
boot type: NOT RUN
POST trace: none; expected sequence documented separately
serial log: none
result: BUILD/STATIC/COMPOSITE PASS; NOT HARDWARE TESTED
recovery required: none during construction; socketed known-good recovery chip remains required for first run
notes: no flash, programmer read-back, reset, Shelly request, AC/power action, vendor call, DRAM access, postcar, ramstage, or payload execution occurred for B06VL
```

B06VL is a separate, default-off response to the seven-run B06VK evidence. It
admits only seven literal CPU-A0 values, the two observed CPU-9c values, and
the two exact CSI byte/raw/canonical forms to one persistently guarded MINIT
call. Workspace raw and canonical hashes are logged and integrity-covered but
are no longer admission keys. The return path still requires exact ABI,
signatures, canaries, policy, four universal completion bytes, I801/CMOS,
SPD, QPI, IOH and memory-controller state before any ordinary DRAM access.

The new producer/consumer contract uses handoff version 9, profile 7 and a
required bit-7 evidence flag. I801 re-arm remains deferred until the complete
uncached 0--640-KiB test, simultaneous alias transaction, both complete 8-MiB
window tests, CBMEM creation and handoff read-back have passed. Only then may
the existing postcar, ramstage and reduced serial-only SeaBIOS entry path run.
That payload deliberately has no boot, graphics, option-ROM, storage, USB,
NIC, TFTP or SSH support; its terminal success marker is `Boot support not
compiled in.`

All 178 Python tests and the focused 29-test B06VI/B06VJ/B06VK/B06VL bundle
passed. Three fixed-epoch, ccache-disabled clean builds were byte-identical.
The four local MSI components, deterministic seven-byte wrapper patch, CBFS,
SeaBIOS ELF entry `000fecd6`, erased lower 12 MiB and exact upper-4-MiB
W25Q128 placement were independently verified. These are source/build
results only. See the [B06VL experiment](b06vl-broad-hard-gate-seabios.md) and
[manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-05 — B06VK live-state identification before B06VL flash

```text
test ID: B06VK-LIVE-01
image ID: X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 artifact 51db5981f881e0cac7155543a8e8e620ac6cad9803137bfdc44dce8a1cbd3864; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not re-read
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f from the preceding B06VK campaign; not re-emitted by this query
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: unchanged; exact model not restamped; no graphics path ran
PSU: unchanged; exact model not restamped
boot type: live continuation; no reset or power action issued and no new cold/warm provenance claimed
POST trace: none; this was a bounded read-only ROMMON query
serial log: research/msi/captures/2026-09-05-b06vk-live-id-guard-clear.raw
result: B06VK CAR ROMMON RESPONSIVE; B06VL NOT ACTIVE; CMOS GUARD CLEAR AT 0x2c
recovery required: no
notes: only id, resetcause and vinputs were issued; no unlock, register write, reset, Shelly request, AC action, DRAM access, postcar, ramstage or payload entry
```

The 616-byte immutable transcript has SHA-256
`829e51c5b5749b7d5270b55cb81921346cb975ebf0ce51ba682f8a81db58c234`.
It proves that the board was alive in B06VK's CAR ROMMON and that the persistent
CMOS diagnostic byte remained safely cleared (`CMOS_DIAG_0E=2c`, `VALID=01`).
The reset-cause command decoded no cause and deliberately cleared no status
bits. This observation is not a new boot, DRAM, ramstage, payload or B06VL
result; B06VL still requires programming before its first hardware run.

## 2026-09-05 — B06VL first complete coreboot-to-SeaBIOS hardware path

```text
test ID: B06VL-HW-02
image ID: X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 artifact 9f8e1085d09207073983bbb3e26ce51474ced604e020a4206406e1a485c3f5e7; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not re-read
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: unchanged; exact model not restamped; graphics disabled and no downstream bus reached
PSU: unchanged; exact model not restamped
boot type: Shelly-controlled one-second relay interruption, then two intended CSI/IOH continuation resets; electrical G3 not independently proven
POST trace: serial phase markers 01/02/03, d3/d4 MINIT call/return, hard-return PASS, DRAM tests, postcar, ramstage, payload; no independent physical POST-card trace retained
serial log: research/msi/captures/2026-09-05-b06vl-hw-02-complete-success-readable.txt
result: COMPLETE REDUCED COREBOOT-TO-SEABIOS PATH PASS ONCE; DELIBERATELY NON-BOOTING PAYLOAD TERMINAL REACHED
recovery required: no; persistent guard finalized only after all v9 post-memory checks passed
notes: one execution only; no full-chip read-back, confirmed G3, full-DIMM test, downstream GPU, storage, graphics, network or OS boot claim
```

The complete-path normalized 602-line transcript is SHA-256
`61bcab5ad23e4f8a5efa4899ee85c061acaeffb6ea9cb35233653f413f859a93`.
Its four immutable source fragments and the exact evidence-to-line mapping are
recorded in
[B06VL-HW-02: first complete coreboot-to-SeaBIOS hardware trace](../research/msi/b06vl-coreboot-seabios-hw-02.md).
The opening bootblock banner has a few already-cropped/corrupted bytes; the
subsequent full B06VL identity and decisive phase sequence are intact.

The third CSI phase produced stable CPU A0 `00017c00`, CPU `9c=00a00502`, CSI
byte/raw/canonical `08/03e3d24e/908dabb6`, and QPI endpoint `070f0f03`.
MINIT returned `EAX=0`; the independent hard-return contract passed with exact
memory-controller mapper `00024489`, F8 `00001545`, channel-2 DOD `000002ac`,
three reported ranks, channel status `00000140`, and IOH stage-9c `bf000000`.

The complete uncached low-memory test, simultaneous alias test, complete
8-MiB CBMEM-window test, and complete 8-MiB object-window test all passed. The
version-9/profile-7 handoff at `0x017feb60` had digest `7cc03894`; I801 was
rearmed from `08:00:5d:01:a3` to exact `08:64:9b:5c:a3`, and the persistent
guard was finalized. Coreboot then loaded postcar at `0x017d3000`, ramstage at
`0x016b9000`, emitted its tables, verified PAM decode, loaded SeaBIOS SELF at
`0x000f8800..0x000fffff`, and jumped to entry `0x000fecd6`.

SeaBIOS rel-1.17.0 found the coreboot table, memory map, CBMEM console, and
mainboard identity. Its raw bus-0 probe found 65 PCI functions, including the
X58 and ICH10R functions, then reached `Boot support not compiled in.` exactly
as intended. Coreboot itself still scanned only its root CPU-cluster/domain
objects and assigned no real PCI bridge resources. SeaBIOS reported maximum
bus 0, so this is not evidence for a downstream GPU or a boot device.

A later 16-MiB serial read contained the valid 40-byte terminal prefix followed
by a repetition delivered faster than the 115200-8N1 physical limit. The
repeated tail is retained separately as a ConsolePi/TTY replay artifact and is
not interpreted as a firmware loop. The primary transcript includes only the
hash-identified valid prefix.

## 2026-09-06 — B06VM automatic PCI/VGA/iPXE first-boot release build

```text
test ID: B06VM-BUILD-01
image ID: X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 26d2cb7527cdd827f9f9002a079bb1d56dcb194dbc815a28ec6a9c5b38d11869; local W25Q128 641063b6f0f756029145aef0bb228f173ced3fbb208daa059bed2aa887db1f6b
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; not physically re-read
CPU: intended Intel Xeon E5645, CPUID 000206c2; no hardware execution
CPU stepping: intended stepping 2; no hardware execution
microcode revision: embedded 0000001f; not executed
DIMM model: intended BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54; physical silkscreen not restamped
GPU: intended GF108 / GeForce GT 630, exact PCI ID 10de:0f00 and measured legacy ROM
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited B06VL path followed by B06VM 2b/2c/2d/2e/2f/34; no runtime trace
serial log: none for B06VM; COM1 0x3f8 at 115200 8N1 is the required first-run capture
result: BUILD/STATIC/COMPOSITE/INDEPENDENT ARTIFACT AUDIT PASS; NOT HARDWARE TESTED
recovery required: none during construction; B06VL known-good and vendor recovery chips remain required for first run
notes: no flash, programmer read-back, reset, Shelly/AC action, PCI write, VGA ROM, payload driver, network transfer, storage access or OS boot occurred for B06VM
```

B06VM keeps B06VL's successful vendor-assisted memory/QPI/IOH producer and
version-9/profile-7 handoff, but a valid path now returns directly into normal
coreboot ramstage instead of stopping in RAMMON.  The ramstage validates one
exact 12-root/2-downstream PCI topology before BAR sizing, assigns I/O only
from `1000..ffff` and MMIO only from `c0000000..dfffffff`, reserves PCIEXBAR
`e0000000..efffffff`, publishes low RAM through 3 GiB and remapped RAM from 4
through 5 GiB, and audits all resources and command bits.  Endpoint bus
mastering remains off; only the two forwarding bridges receive BME.

SeaBIOS rel-1.17.0 is built with boot menu/order, ATA/AHCI, CD-ROM, UHCI/EHCI
USB mass storage and keyboard, PS/2, SERCON, real IRQs, option-ROM, PCIBIOS and
PNPBIOS support.  The B06VM-only `etc/pci-optionrom-exec=1` policy permits
physical ROM mapping only for VGA, while the CBFS RTL8168 iPXE ROM remains
available.  The exact GT630 checksum exception is scoped through
`etc/optionroms-checksum=0`.  VGA plus iPXE leave 43,008 bytes in the legacy
option-ROM window.

Two ccache-disabled fixed-epoch clean builds produced identical coreboot and
iPXE bytes.  The full Python suite passed 190/190 tests and the focused B06VM
suite passed 12/12.  An independent read-only audit verified final hashes,
CBFS, SeaBIOS loaded bytes, iPXE PCIR/checksum, public/MSI separation, the exact
seven-byte deterministic wrapper patch, and W25Q128 top placement.  The TOLM
raw value `bc000000` was independently checked against its inclusive 64-MiB
encoding and correctly yields the exclusive 3-GiB boundary.

This is the first boot-capable legacy-BIOS candidate, not a production-ready
port.  AP/SMP, full ACPI/PIRQ/SMM/S3, generalized hardware, full-DIMM testing,
actual VGA-ROM execution, storage/USB boot, iPXE, and OS boot remain unproven.
See the [B06VM procedure](b06vm-auto-pci-vga-ipxe.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VM first selective downstream PCI hardware run

```text
test ID: B06VM-HW-02
image ID: X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected full-chip 641063b6f0f756029145aef0bb228f173ced3fbb208daa059bed2aa887db1f6b; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not re-read
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: AMD Radeon HD 5450 with onboard VBIOS; exact PCI ID and physical slot not captured
PSU: unchanged; exact model not restamped
boot type: one-second relay interruption; electrical G3 not independently proven
POST trace: inherited producer, destructive windows, CBMEM, postcar and ramstage passed; selective PCI ended at 38
serial log: immutable fragments listed in research/msi/b06vm-hw-02-2026-09-06.md
result: RAMSTAGE/SELECTIVE PCI PARTIAL PASS; RTL8168 ENUMERATED; GPU ENDPOINT ABSENT; SEABIOS NOT ENTERED
recovery required: first execution needed explicit persistent-guard clear ec->2c; no recovery after the retained second execution
notes: the late-replay capture is excluded from causal interpretation; no GPU BAR, VBIOS, payload, video, storage, network or OS-boot claim
```

The first execution stopped safely on the persistent CMOS guard and was not a
new platform failure.  After the explicit `ec -> 2c` clear, the second run
passed both complete 8-MiB destructive windows, CBMEM, the full-low-memory
MTRR transition, postcar and real ramstage.  X58 `00:03.0` received downstream
bus 1 but `01:00.0` read as absent; ICH10R `00:1c.4` received bus 2 and the
RTL8168 answered as `10ec:8168`.  B06VM then stopped at its exact topology
guard, POST `38`.  An answering wrong GPU would instead have reached POST
`39`, so this run isolates link/downstream visibility rather than the old
NVIDIA identity assumption.

Capture hashes and the ConsolePi replay exclusion are recorded in
[the immutable B06VM hardware analysis](../research/msi/b06vm-hw-02-2026-09-06.md).

## 2026-09-06 — B06VN IOU0/AMD physical-VBIOS release build

```text
test ID: B06VN-BUILD-01
image ID: X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 6cdce01c2feb0ee6e819a625332a2fd758785b0475357571a1a52de6ed41268a; local W25Q128 5f1c945aa98f213a09eaf417755c3578c9ebf50d218c1a6460959d019b8e5e45
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; not physically re-read
CPU: intended Intel Xeon E5645, CPUID 000206c2; no B06VN hardware execution
CPU stepping: intended stepping 2; no B06VN hardware execution
microcode revision: embedded 0000001f; not executed as B06VN
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54; physical silkscreen not restamped
GPU: intended AMD Radeon HD 5450 with physical onboard VBIOS; exact PCI device ID measured at runtime
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path, then B06VN 2b/2c/30/31 and either 32 or 35 before later resource/payload milestones
serial log: none for B06VN; COM1 0x3f8 at 115200 8N1 is required from before power-on
result: BUILD/REPRODUCIBILITY/202-TEST/SOURCE/CBFS/COMPOSITE AUDIT PASS; NOT HARDWARE TESTED
recovery required: none during construction; B06VL known-good and vendor recovery chips remain required
notes: no flash, programmer read-back, power action, PCIe write, GPU enumeration, VBIOS, payload, video, network, storage or OS boot occurred for B06VN
```

B06VN adds exactly one new hardware hypothesis.  After the complete inherited
platform gates, it logs X58 ports `00:01.0`, `00:03.0`, and `00:07.0`; if
IOU0's DLL is inactive, it issues one 16-bit `0x000c` start write to
`00:03.0 + 0x190` at ECAM `0xe0018190`; it then polls `DLLA=1 && LT=0` for at
most one second.  Ports 1 and 7 remain read-only and no PCIe Link-Control or
QPI retrain is requested.

The AMD GPU device ID is not guessed.  A present endpoint must be vendor
`1002`, VGA class `0300`, normal header; absence now continues toward serial
SeaBIOS.  CBFS has no AMD VBIOS.  SeaBIOS receives normal checksum validation
and physical-VGA-ROM execution policy, both integer value 1, while the pinned
RTL8168 iPXE ROM remains embedded.

Two clean fixed-epoch builds were byte-identical.  The complete Python suite
passed 202/202, the focused VI-through-VN subset 53/53, and independent object
inspection found exactly one `movw $0xc,0xe0018190` store.  See the
[B06VN procedure](b06vn-iou0-physical-vbios.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VN first boot-enabled SeaBIOS and iPXE execution

```text
test ID: B06VN-HW-02
image ID: X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected full-chip 5f1c945aa98f213a09eaf417755c3578c9ebf50d218c1a6460959d019b8e5e45; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not re-read
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: AMD Radeon HD 5450 with physical VBIOS; endpoint identity remained unreadable
PSU: unchanged; exact model not restamped
boot type: one-second Shelly relay interruption after explicit guard clear; electrical G3 not independently proven
POST trace: serial milestones through RAM windows, CBMEM, postcar, ramstage, PCI, payload and iPXE; no independent physical POST-card trace retained
serial log: immutable raw fragments listed in research/msi/b06vn-hw-02-2026-09-06.md
result: COREBOOT RAMSTAGE, BOOT-ENABLED SEABIOS AND IPXE OPTION ROM EXECUTED ONCE; GPU ABSENT; CAPTURE ENDED AFTER FIRST LOGGED IPXE INT16 ENTRY
recovery required: no
notes: one successful payload execution only; no flash read-back, confirmed G3, VGA/VBIOS, Ethernet link, DHCP, TFTP, storage or OS boot claim
```

The retained run used the previously successful `17800/a00502` CPU tuple,
returned from MINIT with `EAX=0`, passed the hard gate and all complete
uncached memory tests, established CBMEM, and entered postcar and ramstage.
Before B06VN's conditional link action, X58 root `00:03.0` already reported
Gen1 x16, `DLLA=1`, `LT=0`; therefore the write was correctly skipped. The
GPU at `01:00.0` remained absent, so SeaBIOS reported no VGA and did not run
the Radeon VBIOS.

SeaBIOS did enumerate the RTL8168 at `02:00.0`, copy and validate the CBFS
iPXE ROM, run it, satisfy its PCIBIOS/PMM requests and reach the Ctrl-B prompt.
The trace ends after the first logged INT 16h `AH=01` keyboard-status call. A later
exclusive 20-second UART observation received no bytes, and late Ctrl-B/CR
input elicited no response. This makes the still-immature IRQ/PIC/PIT path a
specific follow-up candidate, but it is not yet a proven root cause.

The same SeaBIOS scan found 32 bus-2 aliases with matching X58
vendor/device/class tuples and reported 98 PCI functions. Intel documents this behavior for
`IOHBUSNO.Valid=0`; the vendor-firmware reference reads
`00:14.0+0x10a=0x0100`. The next build should repair that mandatory routing
state before considering any secondary-bus reset. Exact capture hashes and
the excluded 51-MiB transport replay prefix are recorded in
[the B06VN hardware analysis](../research/msi/b06vn-hw-02-2026-09-06.md).

## 2026-09-06 — B06VN hardened fallback and independent payload repeat

```text
test ID: B06VN-HW-03 / B06VN-HW-04
image ID: X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected full-chip 5f1c945aa98f213a09eaf417755c3578c9ebf50d218c1a6460959d019b8e5e45; no programmer read-back recorded
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not re-read
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: AMD Radeon HD 5450 with physical VBIOS; endpoint identity remained unreadable
PSU: unchanged; exact model not restamped
boot type: two one-second Shelly relay interruptions; electrical G3 not independently proven
POST trace: HW-03 safe ROMMON fallback; HW-04 through RAM windows, CBMEM, postcar, ramstage, PCI, SeaBIOS, iPXE and first INT16 entry; no independent physical POST-card trace retained
serial log: hardened immutable raw captures and metadata listed in research/msi/b06vn-hw-03-hw-04-2026-09-06.md
result: HW-03 REJECTED PRE_A0=16e00 FAIL-CLOSED; HW-04 REPEATED COREBOOT RAMSTAGE, SEABIOS AND IPXE; GPU ABSENT; CAPTURE ENDED AFTER FIRST LOGGED IPXE INT16 ENTRY
recovery required: no; persistent guard explicitly cleared between runs
notes: B06VN now has two payload-reaching executions total; no flash read-back, confirmed G3, VGA/VBIOS, Ethernet link, DHCP, TFTP, storage or OS-boot claim
```

HW-03's two consecutive A0 reads agreed at `0x00016e00`, outside the exact
B06VL admission set; the firmware therefore logged `STABLE=00`. It correctly
made no MINIT call and returned to recovery ROMMON. HW-04
then independently reproduced the admitted `17800/a00502` path, `MINIT
EAX=0`, every complete uncached memory test, CBMEM, postcar, ramstage,
SeaBIOS, RTL8168 iPXE execution and the first logged INT 16h `AH=01`
entry, after which the capture ended. Its exclusive raw capture is 68,088 bytes with SHA-256
`538b9294fd5982b91d9742a083bd3be97ef43430fef30186c49f3fa25e5096d2`;
its aggregate 115200-8N1 rate envelope passed and it contains no HW-02-style
large replay prefix. Immediate Ctrl-B/Enter input produced no observed serial response,
but that operator interaction was not retained as its own artifact.

The repeat again showed root `00:03.0` active at Gen1 x16 while `01:00.0`
remained absent, and SeaBIOS again found 98 functions including the bus-2 IOH
aliases. The next isolated build is therefore the mandatory
`IOHBUSNO=0x0100` routing correction plus paired CF8/CFC and direct-PCIEXBAR
endpoint reads. See the
[hardened-repeat analysis](../research/msi/b06vn-hw-03-hw-04-2026-09-06.md).

## 2026-09-06 — B06VO IOHBUSNO configuration-routing release build

```text
test ID: B06VO-BUILD-01
image ID: X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 5bb953c3936dc5a4026126f07864a570f090e8ec2dda1ee2c1a30f5579a0b3c8; local W25Q128 01427d62e1fb50dc5ee9208490017eccf29113599636f4df4400cc9f9075426c
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; not physically re-read
CPU: intended Intel Xeon E5645, CPUID 000206c2; no B06VO hardware execution
CPU stepping: intended stepping 2; no B06VO hardware execution
microcode revision: embedded 0000001f; not executed as B06VO
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54; physical silkscreen not restamped
GPU: intended AMD Radeon HD 5450 with physical onboard VBIOS; exact PCI device ID still unobserved
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path, then B06VO PRE/POST telemetry and 33 before B06VN 2c/30/31; 3f is terminal routing-gate failure
serial log: none for B06VO; COM1 0x3f8 at 115200 8N1 is required from before power-on
result: BUILD/REPRODUCIBILITY/216-TEST/SOURCE/OBJECT/CBFS/COMPOSITE AUDIT PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06VN/B06VL and vendor recovery chips remain required
notes: no flash, programmer read-back, power action, hardware register access, GPU enumeration, VBIOS, payload, video, network, storage or OS boot occurred for B06VO
```

B06VO tests the routing defect isolated by the hardened B06VN repeat.  After
the exact raw root preflight and before IOU0 start or PCI scanning, it reads
CSRCFG `00:14.0+0x10a` through direct PCIEXBAR.  It accepts only `0000` or
the vendor-observed `0100`, writes exact `0100` only for `0000`, and requires
exact `0100` readback or halts at POST `3f`.  It records read-only PRE/POST
aliases at `01:0d.0` and `02:0d.0` and the documented local/global bus ranges.

After generic bridge routing assigns root `00:03.0` a secondary bus, a
read-only callback compares CF8/CFC and direct-PCIEXBAR identity reads of
`secondary:00.0` at cumulative 0/1/10/100 ms, then invokes the unchanged
`pci_scan_bus()`.  B06VO adds no bridge reset, PCIe Link-Control/retrain,
Link-Capability, GPU, IRQ, range-register or payload write.

The release builder completed two clean fixed-epoch builds with byte identity;
the full suite passed 216/216 and the focused VI-through-VO subset 60/60.
Object inspection found the intended `movw $0x100,0xe00a010a` plus only the
inherited conditional B06VN IOU0 store.  A separate B06VN regression build
compiled B06VO out and reproduced the prior B06VN public hash exactly.  See
the [B06VO procedure](b06vo-iohbusno-routing.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VO fixes IOH routing and reaches physical Radeon VBIOS/display

```text
test ID: B06VO-HW-02
image ID: X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 01427d62e1fb50dc5ee9208490017eccf29113599636f4df4400cc9f9075426c; no programmer read-back retained
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not physically re-read
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: AMD Radeon HD 5450, 1002:68f9 VGA plus 1002:aa68 audio, physical onboard VBIOS
PSU: unchanged; exact model not restamped
boot type: one-second Shelly relay interruption; electrical G3 not independently proven
POST trace: through RAM tests, CBMEM/postcar/ramstage, IOHBUSNO POST 33, Radeon POST 32, SeaBIOS, physical Radeon VBIOS, RTL8168 iPXE and first logged INT16 entry; no independent physical POST-card trace retained
serial log: research/msi/captures/2026-09-06-b06vo-hw-02-clean.raw, 66204 bytes, SHA-256 f03ada257e7f396bebac7d36dc9d36a49c72f8d8ae027237e73bec0f0ab39459
result: FIRST IOHBUSNO/PEG/GPU/VBIOS/DISPLAY PASS; SEABIOS AND IPXE EXECUTED; NO DEVICE OR OS BOOT
recovery required: no
notes: visible display output was reported live by the operator; no photo/exact screen contents, programmer read-back, confirmed electrical G3, Ethernet link, DHCP, TFTP, storage or OS-boot evidence retained
```

The exclusive quiet-gated capture reproduced all three expected CSI phases.
Phase 3 admitted `PRE_A0=0x17600`, observed `POST_A0/9C=17600/b00502`,
returned from MINIT with EAX zero and passed the hard return gate.  The complete
640-KiB low-memory test and both complete 8-MiB uncached windows passed before
CBMEM, postcar and ramstage.

B06VO then made its single new write with direct before/after evidence:

```text
PRE  IOHBUSNO=0000 ALIAS01:0d.0=343a8086 ALIAS02:0d.0=343a8086
POST IOHBUSNO=0100 ALIAS01:0d.0=ffffffff ALIAS02:0d.0=ffffffff
B06VO IOHBUSNO exact gate PASS (write=1)
```

Root `00:03.0` remained Gen1 x16 with `DLLA=1` and `LT=0`; no BIF/start
write was needed.  After secondary bus 1 was assigned, paired CF8 and direct
PCIEXBAR reads returned `68f91002` identically at 0, 1, 10 and 100 ms.
Coreboot enumerated the Radeon at `01:00.0`, accepted its VGA identity and
assigned its BARs and ROM window.  SeaBIOS additionally saw its audio function
at `01:00.1`.

SeaBIOS found 68 functions rather than B06VN's 98.  Bus 2 contained only the
real RTL8168 at `02:00.0`; none of the prior 32 X58 aliases was enumerated.
This count change is exactly the removal of 32 aliases plus the addition of
two Radeon functions.  The result, combined with the single B06VO delta,
strongly identifies missing `IOHBUSNO.Valid` programming as the prior routing
defect.  `LCFGBUS` remained `00-00`, so the run does not justify writing the
vendor-observed wider range.

With no AMD ROM in CBFS, SeaBIOS mapped the card's physical ROM BAR, validated
an AA55 image of 59,904 bytes, copied it to C0000 and executed `c000:0003`.
It subsequently configured split serial/VGA console, enabled VGA text mode
and continued through USB/disk initialization, proving that the physical VBIOS
returned.  The operator independently reported live display output.

The embedded RTL8168 iPXE ROM still reached its Ctrl-B prompt and first logged
INT 16h AH=01 entry.  Later post-capture Ctrl-B and Ctrl-B/CR produced no UART
response as an unretained observation, but may have missed the short prompt.
Because the retained recorder deliberately stopped one second after that
marker, this is only the next unobserved boundary, not a demonstrated hang.
The log also contains an i8042 flush timeout and no detected ATA device.  Any
later payload/input issue is separate from the solved IOHBUSNO/PEG/VBIOS path.
Full evidence and capture hashes are in the
[B06VO hardware analysis](../research/msi/b06vo-hw-02-2026-09-06.md).

## 2026-09-06 — B06VO repeat confirms display and exposes missing timer delivery

```text
test ID: B06VO-HW-04
image ID: X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 01427d62e1fb50dc5ee9208490017eccf29113599636f4df4400cc9f9075426c; no programmer read-back retained
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not physically re-read
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: AMD Radeon HD 5450, 1002:68f9 VGA plus 1002:aa68 audio, physical onboard VBIOS
PSU: unchanged; exact model not restamped
boot type: one-second Shelly relay interruption; electrical G3 not independently proven
POST trace: through RAM tests, CBMEM/postcar/ramstage, IOHBUSNO POST 33, Radeon POST 32, SeaBIOS, physical Radeon VBIOS, RTL8168 iPXE and first logged INT16 AH=01 entry; no independent physical POST-card trace retained
serial log: research/msi/captures/2026-09-06-b06vo-hw-04-long.raw, 67230 bytes, SHA-256 295f10c5bb20ef0289242ae4849693f12097aaf303b0a93abcbcf9ba880a04ae
result: SECOND COMPLETE B06VO COREBOOT/PCI/VBIOS/DISPLAY PATH; DISPLAY PHOTO MATCHES THE SEABIOS/IPXE PROMPT; ONE RETAINED RUN HAS >=120 S UART SILENCE AFTER FIRST LOGGED IPXE INT16 ENTRY; SUBSEQUENT TIMER WAIT AND IRQ ROOT CAUSE ARE SOURCE-DERIVED INFERENCES PENDING B06VP TEST
recovery required: no
notes: operator supplied an inline display photo after the run; it is not an independently hashed repository artifact; no normal capture metadata, programmer read-back, confirmed electrical G3, Ethernet link, DHCP, TFTP, storage or OS-boot evidence retained
```

The quiet gate discarded zero bytes, and the complete initialized path repeated
with `PRE/POST_A0=17c00/17c00`, `POST_9C=a00502`, MINIT EAX zero, every
bounded uncached RAM test passing, and CBMEM hash `be1ef6f6`.  IOHBUSNO again
changed `0000->0100`; the sampled aliases disappeared, all four Radeon CF8/ECAM
reads returned `68f91002`, and SeaBIOS again found 68 functions.  The physical
59,904-byte Radeon VBIOS returned.  The operator's subsequent photo visibly
shows SeaBIOS `rel-1.17.0-0-gb52ca86e`, iPXE on `02:00.0`, and the same
`Press Ctrl-B to configure iPXE` terminal line as the serial trace.

This time the recorder remained active.  No UART byte followed the first iPXE
`INT 16h AH=01` entry for at least 120 seconds.  It was then deliberately
interrupted to free the TTY, so a provenance JSON replaces normal recorder
metadata.  A subsequent three-second COM1 Ctrl-B probe captured zero bytes.
SeaBIOS initialized both EHCI and all six UHCI functions but logged no USB/HID
device, while the i8042 path returned only `ff` and timed out; no working
keyboard is claimed.  USB routing remains a later, separate hypothesis.

Source analysis narrows the next hypothesis.  The INT16 status request is
non-blocking; iPXE immediately follows it with a loop that executes `sti; hlt`
until BDA timer word `0040:006c` advances.  SeaBIOS configured its PIC and PIT,
but the custom non-SMP CPU-cluster path never calls coreboot's
`setup_lapic_interrupts()`.  Its standard BSP setup would unmask LAPIC LINT0 in
ExtINT virtual-wire mode, allowing legacy PIC IRQ0 to reach the CPU.  The
initializer is present in the ramstage object archive but absent from the final
B06VO ramstage due to no reference.  Missing LINT0 ExtINT is therefore the
leading inference, not yet a measured fact.  The next release will make only
that change, retain hardware IRQs and the real iPXE prompt, and validate exact
LAPIC readback before entering SeaBIOS.

Full evidence, hashes and proof boundaries are in the
[B06VO-HW-04 analysis](../research/msi/b06vo-hw-04-2026-09-06.md).

## 2026-09-06 — B06VP BSP LAPIC ExtINT release build

```text
test ID: B06VP-BUILD-01
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 2671fd320b11d0b87c7a2b91cdf5baf442b8609af35d2a2068ffad952aaad551; local W25Q128 e721119664b1945f1ad725c77b303010d51e4aaa7e3898b13a64b7f7e3bad71b
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; not physically re-read
CPU: intended Intel Xeon E5645, CPUID 000206c2; no B06VP hardware execution
CPU stepping: intended stepping 2; no B06VP hardware execution
microcode revision: embedded 0000001f; not executed as B06VP
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54; physical silkscreen not restamped
GPU: intended AMD Radeon HD 5450, 1002:68f9 VGA plus 1002:aa68 audio, physical onboard VBIOS
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path; B06VP 40 begin, 42 accepted readback, 43 terminal failure; then unchanged SeaBIOS/Radeon/iPXE path
serial log: none for B06VP; COM1 0x3f8 at 115200 8N1 is required from before power-on
result: BUILD/REPRODUCIBILITY/226-TEST/SOURCE/BINARY/CBFS/COMPOSITE/CHECKPATCH/INDEPENDENT AUDIT PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06VO and vendor recovery chips remain required
notes: no flash, programmer read-back, power action, hardware register access, IRQ0, timer progress, keyboard, network, storage or OS boot occurred for B06VP
```

B06VO-HW-04 remained for at least 120 seconds after iPXE's first logged
`INT 16h AH=01` call.  The displayed underscore continued to blink, but this
is generated autonomously by the VGA text-mode CRTC and does not prove CPU
progress.  Source inspection showed that iPXE next executes `sti; hlt` while
waiting for the BIOS Data Area tick.  SeaBIOS configured PIC/PIT and retained
hardware IRQ support, while the custom single-CPU cluster had no callback that
invoked coreboot's standard LAPIC virtual-wire initialization.

B06VP adds only that callback.  It validates `IA32_APIC_BASE` before LAPIC
MMIO, logs PRE TPR/SVR/LVT0/LVT1, calls `setup_lapic_interrupts()` exactly
once, revalidates the APIC base, logs POST state, and fails closed unless TPR
priority is zero, SVR is enabled at vector `0x0f`, LINT0 is unmasked
edge/high ExtINT and LINT1 is unmasked edge/high NMI.  It changes no memory,
QPI, IOH routing, PCIe, GPU, USB, PIC, PIT, IOAPIC, SeaBIOS or iPXE policy.

The release command made two clean fixed-epoch B06VP builds byte-identical.
The full repository suite passed 226/226, the focused B06VP contract passed
10/10, shell syntax passed, and checkpatch reported zero errors and zero
warnings for all five touched C files.  Independent lifecycle/source review
found no blocking issue.  Final ramstage disassembly confirmed the callback,
the single standard-helper call and the selected-field gates.  CBFS retains
SeaBIOS, the pinned 95,232-byte RTL8168 iPXE ROM, serial console and physical
option-ROM execution, with no embedded AMD VBIOS.

After saving B06VP, a separate clean B06VO build used the same fixed epoch and
reproduced the published B06VO base byte-for-byte at SHA-256
`5bb953c3936dc5a4026126f07864a570f090e8ec2dda1ee2c1a30f5579a0b3c8`.
This proves the default-off B06VP blocks do not alter B06VO.

The next hardware acceptance test is deliberately passive after boot: retain
the complete UART trace and do not press a key on the first run.  POST `42`
plus the serial PASS line proves only register readback; the hypothesis passes
only if iPXE's timeout and later output progress beyond the former boundary.
USB/PS2/serial input polling shares the timer path, so keyboard behavior is
evaluated after timer delivery, with USB HID still treated separately.

See the [B06VP procedure](b06vp-lapic-extint.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VP HW-03 reaches netboot.xyz

```text
test ID: B06VP-HW-03
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 e721119664b1945f1ad725c77b303010d51e4aaa7e3898b13a64b7f7e3bad71b; no programmer read-back retained
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target; revision not physically re-read
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical silkscreen not restamped
GPU: AMD Radeon HD 5450, 1002:68f9, physical onboard VBIOS
PSU: unchanged; exact model not restamped
boot type: complete three-phase reset sequence retained; initial electrical G3 provenance not independently recorded
POST trace: inherited RAM/QPI path, B06VO IOHBUSNO gate, B06VP 40/42, ramstage, SeaBIOS, Radeon VBIOS, RTL8168 iPXE and netboot.xyz menu
serial log: research/msi/captures/2026-09-06-b06vp-hw-03-full-to-netboot-menu.raw, 1,344,022 bytes, SHA-256 6659101eb3abd58bdca89f90d5dc2a8ed8824d9b07ebcccdcb4a86888d779e18
result: ONE COMPLETE LAPIC/TIMER/VGA/ETHERNET/DHCP/TFTP/NETBOOT.XYZ PASS; USB AND PS/2 INPUT NOT PROVED
recovery required: no
notes: no metadata sidecar for the long capture and no programmer read-back; not a 10-cold/10-warm milestone
```

The selected LAPIC state changed from masked LINT0/LINT1 to `LVT0=00000700`
ExtINT and `LVT1=00000400` NMI; the exact B06VP gate passed.  SeaBIOS and iPXE
then advanced beyond B06VO's former timer wait.  The RTL8168 reported link up,
DHCP assigned `192.0.2.196`, and TFTP fetched 388,848 bytes of
`netboot.xyz.kpxe` from `192.0.2.53`.  The downloaded iPXE executed and
displayed the netboot.xyz v3.0.0 menu.

SeaBIOS initialized both ICH10 EHCI and all six UHCI functions but printed no
USB descriptor or keyboard registration.  Its i8042 path returned `ff` and
timed out.  The next build therefore isolates the documented EHCI instruction
register requirement and adds read-only port/routing telemetry; it does not
mix in SATA, LPC, GPIO or full-southbridge policy.

See the [complete B06VP-HW-03 analysis](../research/msi/b06vp-hw-03-2026-09-06.md).

## 2026-09-06 — B06VQ isolated ICH10 EHCI release build

```text
test ID: B06VQ-BUILD-01
image ID: X58PROE-B06VQ-ICH10-EHCI-INIT-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 01503b957e717d35f7f7c4ebeb2d9a843d987df17365002bb06dd32f0270c754; local W25Q128 9c76445958c93382567fda4c77ff0184763f954cbc8fe12af8239f9c03bd9b27
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no target execution
CPU: intended Intel Xeon E5645, CPUID 000206c2; no target execution
CPU stepping: intended stepping 2
microcode revision: embedded 0000001f; not executed as B06VQ
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path; B06VQ 44 begin, 46 exact readback, 47 terminal failure; then unchanged PCI/Radeon/SeaBIOS/iPXE path
serial log: none for B06VQ; COM1 0x3f8 at 115200 8N1 required before power-on
result: BUILD/REPRODUCIBILITY/234-TEST/SOURCE/BINARY/CBFS/COMPOSITE/TOP-PLACEMENT/CHECKPATCH/5-BOARD ICH10 REGRESSION PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06VP and vendor recovery chips remain required
notes: no flash, target write, reset, USB enumeration, keyboard, SATA/storage or OS boot occurred for B06VQ
```

B06VQ freezes the already reached RAM/QPI/IOH/PCIe/LAPIC/SeaBIOS path as a
development assumption and adds one USB prerequisite. It programs only the
Intel-documented required fields at EHCI PCI offset `0xfc` on `00:1a.7` and
`00:1d.7`, using mask/value `2002000c`/`20020008`, preserving every other bit
and requiring exact whole-dword readback. The full ICH10 driver remains off.

The release command produced two byte-identical clean builds, verified the
pinned SeaBIOS/iPXE configuration and composite inputs, and placed the local
deterministic 4-MiB image at the top of a 16-MiB W25Q128 image. The complete
suite passed 234/234; the B06VQ contract passed 8/8; five established ASUS
ICH10 targets built after the helper refactor. Final disassembly confirms the
expected AND `dffdfff3` and OR `20020008` before the PCI config write.

The first hardware result is deliberately not predicted from the register
gate alone. The read-only pre-SeaBIOS snapshot will distinguish missing port
connect/power, companion routing and a later SeaBIOS enumeration failure. A
working milestone requires a descriptor, `USB keyboard initialized`, and
effective menu input.

See the [B06VQ procedure](b06vq-ich10-ehci-init.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VR isolated ICH10 AHCI-map release build

```text
test ID: B06VR-BUILD-01
image ID: X58PROE-B06VR-ICH10-AHCI-MAP-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 1c9c2a18bc592284aeadba8ed71580bef48202f422113d6bc68274cd07a84f7c; local W25Q128 55c340b0595b59399a6369af2a533c07324d1d9e5988e12d2d34b6e76e316913
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no target execution
CPU: intended Intel Xeon E5645, CPUID 000206c2; no target execution
CPU stepping: intended stepping 2
microcode revision: embedded 0000001f; not executed as B06VR
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path; B06VR 48 begin, 4a exact readback, 4b terminal failure; then normal PCI/Radeon/SeaBIOS/iPXE path
serial log: none for B06VR; COM1 0x3f8 at 115200 8N1 required before power-on
result: BUILD/REPRODUCIBILITY/SOURCE/BINARY/CBFS/COMPOSITE/TOP-PLACEMENT PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06VP and vendor recovery chips remain required
notes: no flash, target write, reset, AHCI MMIO, SATA-device access, storage or OS boot occurred for B06VR
```

B06VR admits only the exact quiescent dual-IDE reset identity or its own
retained AHCI identity.  Its sole new transition is the documented 16-bit
masked update of SATA MAP bits 7:5 to `011b`, followed by the required BAR5
zero write when changing programming interface.  It then requires D31:F2
`8086:3a22/010601`, MAP `0060`, D31:F5 absent and decode still disabled before
normal enumeration.  PCS, clocks, AHCI MMIO and the broad ICH10 driver remain
untouched.

See the [B06VR procedure](b06vr-ich10-ahci-map.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VS isolated ICH10 AHCI-port release build

```text
test ID: B06VS-BUILD-01
image ID: X58PROE-B06VS-ICH10-AHCI-PORTS-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 57299adf2fbfac9cb6e7d099148fc724dcae1efe32a0db6db1c2d105894b83ec; local W25Q128 09a6255851fb13091485ef511be150ab2971f53d06c7d1c82ed8b000fe5c5244
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no target execution
CPU: intended Intel Xeon E5645, CPUID 000206c2; no target execution
CPU stepping: intended stepping 2
microcode revision: embedded 0000001f; not executed as B06VS
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path; B06VS 4c begin, 4e exact readback, 4f terminal failure; then normal Radeon/SeaBIOS/iPXE path
serial log: none for B06VS; COM1 0x3f8 at 115200 8N1 required before power-on
result: BUILD/REPRODUCIBILITY/257-TEST/SOURCE/BINARY/CBFS/COMPOSITE/TOP-PLACEMENT/INDEPENDENT AUDIT PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06VP and vendor recovery chips remain required
notes: no flash, target write, reset, AHCI MMIO, SATA-device access, storage or OS boot occurred for B06VS
```

B06VS runs only after the inherited domain, leaf-resource and non-overlap
audits.  It requires exact AHCI identity, MAP and command state plus an exact
2-KiB BAR5 with `MEM|ASSIGNED|STORED` flags, valid granularity/alignment and a
matching raw BAR.  Its helper reads and writes only the PCS low byte, selecting
bits 5:0 as `3f`; reserved bit 14 must remain zero, ORM bit 15 must remain
unchanged and presence bits 13:8 are read-only telemetry.  A retained exact
state skips the write.  Five bounded later PCS samples cannot promote or fail
the path.

The release was rebuilt after independent review exposed and corrected three
issues: reserved bit 14 had initially been grouped with ORM, BAR
granularity/alignment and exact flags were not all gated, and builder checks
were caller-CWD dependent and insufficiently strict about USB/CBFS policy.
The final command performed two clean fixed-epoch byte-identical builds and
passed 257/257 tests.  It does not program SCLKCG, AHCI MMIO, PHY state or a
disk; the planned following experiment isolates only the documented target
SCLKCG field after B06VS has first been observed on hardware.

See the [B06VS procedure](b06vs-ich10-ahci-ports.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VT isolated ICH10 SATA clock-field release build

```text
test ID: B06VT-BUILD-01
image ID: X58PROE-B06VT-ICH10-SATA-CLOCK-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 1df68547f6c2bffdd71ea636304242db0db64c5333eba19eb3ed9b500b3208ad; local W25Q128 64260f7c0c17cbdeb7f2cac4d98f9b33d2b144395bb852f197a72d8aa0b1b9bb
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no target execution
CPU: intended Intel Xeon E5645, CPUID 000206c2; no target execution
CPU stepping: intended stepping 2
microcode revision: embedded 0000001f; not executed as B06VT
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path; B06VT 50 begin, 52 exact readback, 53 terminal failure; then normal Radeon/SeaBIOS/iPXE path
serial log: none for B06VT; COM1 0x3f8 at 115200 8N1 required before power-on
result: BUILD/REPRODUCIBILITY/265-TEST/SOURCE/BINARY/CBFS/COMPOSITE/TOP-PLACEMENT PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06VP and vendor recovery chips remain required
notes: no flash, target write, reset, AHCI MMIO, SATA-device access, storage or OS boot occurred for B06VT
```

B06VT is called directly after the inherited B06VS step at the same
post-allocation boot-state hook. It accepts only SCLKCG `00000000` or
`00000193`; both imply all six PCD fields and all reserved bits are zero. If
needed, its sole new write is a 32-bit RMW which replaces only bits 8:0 with
`0x193`. Complete-dword readback must be `00000193`, and the exact inherited
AHCI identity, MAP, PCS, command and ABAR resource contract is checked again.
The linked helper at `0x040002f9` contains AND `fffffe00` and OR `193`.

The current builder performed two clean fixed-epoch builds under an inherited
`PYTHONOPTIMIZE=1` environment after explicitly clearing it. The resulting
coreboot and iPXE images were byte-identical; CBFS integer controls, local
vendor-assisted composition, wrapper patch and 16-MiB top placement all pass.
This release does not access AHCI MMIO, enable PCI decode/BME, issue a reset or
COMRESET/OOB, change ORM/PI/PCS policy, or touch a disk.

See the [B06VT procedure](b06vt-ich10-sata-clock.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VQ-USBTRACE1 deferred SeaBIOS USB diagnostic build

```text
test ID: B06VQ-USBTRACE1-BUILD-01
image ID: X58PROE-B06VQ-USBTRACE1-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 5e2300597a4dd117f8d48531feebdafb3882cb1d2ea623ed6dde9ca20d5e3327; local W25Q128 94ddb5408a60970a73e003bd88d9aff5f32c381d49230b443ea18cad25c4e874
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no target execution
CPU: intended Intel Xeon E5645, CPUID 000206c2; no target execution
CPU stepping: intended stepping 2
microcode revision: embedded 0000001f; not executed as USBTRACE1
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged intended setup; exact model not restamped
boot type: none on target; QEMU i440fx instrumentation validation only
POST trace: expected exact B06VQ platform path; no new coreboot hardware-write POST stage
serial log: QEMU trace 24/24 events, dropped=0, through descriptor/configuration/HID/driver and USB keyboard initialization
result: TWO-BUILD REPRODUCIBILITY/277-TEST/SOURCE/PAYLOAD/CBFS/COMPOSITE/TOP-PLACEMENT/ISOLATION/QEMU TRACE PASS; NOT TARGET TESTED
recovery required: none during construction; known-good B06VP/B06VQ and vendor recovery chips remain required
notes: no flash, target write, reset, SATA delta or physical USB transaction occurred for this record
```

This diagnostic derivative preserves B06VQ and explicitly leaves B06VR,
B06VS and B06VT disabled. It adds bounded read-only RCBA/GPIO/EHCI/UHCI gate
telemetry before SeaBIOS. The payload is a clean deterministic local SeaBIOS
commit `5497f43189374647b3b0f282aa497e71c891f3c5`, reconstructed from base b52
and the hash-pinned patch. USB setup appends at most 96 fixed 16-byte entries
and prints them only after enumeration; overflow is explicit and never
overwrites early evidence. The parser rejects missing/duplicated indices,
count mismatch, overflow and missing termination in strict mode.

The QEMU path reached every intended instrumentation boundary and then
`USB keyboard initialized`. A subsequent full ordinary-B06VQ rebuild exactly
reproduced public base hash
`01503b957e717d35f7f7c4ebeb2d9a843d987df17365002bb06dd32f0270c754`,
showing that the payload patch does not leak into the stable variant. This is
instrumentation validation only; the first physical result must retain the
complete `[USB-GATE]` and `[USBTRACE]` blocks from reset.

See the [design and procedure](b06vq-usbtrace1.md),
[QEMU evidence](../research/msi/seabios-usbtrace-qemu-validation-2026-09-06.md)
and [release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VP HW-04 post-MINIT stall and guard recovery

```text
test ID: B06VP-HW-04
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906, verified later in the same recovered CAR session
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 7199961a544f914560e14ed264077fc9bad82416a82e2a9c58c003e21a8f5232; no programmer read-back in this run
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged setup; exact model not restamped
boot type: Shelly-controlled exact one-second AC removal, then automatic power return
POST trace: phase 1 CSI reset, phase 2 SYRE reset, phase 3 CSI/MINIT return; no later code retained
serial log: stopped after POST_MINIT_I801_RAW=08:00:5d:01:a3 and CMOS0E=ec; 180-second capture timeout
result: PARTIAL — MINIT RETURNED; NEW POST-MINIT STALL BEFORE RAMSTAGE/PAYLOAD
recovery required: next AC cycle stopped safely on persistent guard; guard then cleared interactively to 2c and CAR ROMMON retained
notes: this negative run does not revoke the earlier complete B06VP SeaBIOS/iPXE success; it proves the path is not yet repeatable
```

Raw capture SHA-256 is
`58290013f935febb09432032d619a3eae77790609ecf4cf32d03827a1ed22cb6`.
The guard-stop capture is
`ccfd2f6b61d7bbd20f14a436bb86bd40e64d1bfce6d14bd1f1fc5d4d6e305421`.
The retained post-clear transcript contains only `vinputs` and proves the
`CMOS0E=2c` postcondition; it does not contain the preceding clear command.

## 2026-09-06 — B06VP recovered-CAR read-only ICH10 census

```text
test ID: B06VP-CAR-ICH10-CENSUS-01
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906, verified by ROMMON id
boot type: same retained guard-fallback CAR session; no additional reset
POST trace: no new boot; ROMMON command execution only
serial log: LPC/SMBus/EHCI/RCBA/PM/GPIO/PIRQ/internal-interrupt register reads
result: PASS — READ-ONLY CENSUS COMPLETE; SIX COMMON ICH10 REQUIRED FIELDS PROVED ABSENT
recovery required: none; board deliberately left powered in CAR ROMMON
notes: no target register-write command; PCI reads necessarily update CF8 address selector and UART output changes UART status
```

The six selected fields read `CIR8[1:0]=0`, `FD[0]=0`,
`CIR9[27:26]=0`, `CIR7[19:16]=4`, `CIR13[19:16]=4` and
`CIR10[17:16]=0`, versus common-driver targets `2/1/2/5/5/3`.
USB clock gating, companion mapping, port-power override and GPIO
over-current-use masks supplied no evidence for an additional USB write.
PIRQ A-H all read `80`, HPTC and OIC read zero, and the internal interrupt
routes differ from the vendor runtime.  This is evidence for isolated future
experiments, not permission to copy the complete historical ICH10 driver.

See the [platform audit](../research/msi/platform-init-audit-2026-09-06.md)
and the immutable captures
`../research/msi/captures/2026-09-06-b06vp-car-ich10-readonly-census.raw`
and
`../research/msi/captures/2026-09-06-b06vp-car-ich10-routing-readonly.raw`.

## 2026-09-06 — B06VP CAR ICH10 six-required-field live transaction

```text
test ID: B06VP-CAR-ICH10-REQ6-01
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906
boot type: no additional boot; retained recovered CAR ROMMON session
POST trace: script PRE e5 / successful return e7 / ordinary ROMMON ready bc
serial log: 24-operation sealed table; PROGRAM_FNV=84afcaf8; complete run and trace retained
result: PASS — SIX MASKED ICH10 R/W FIELDS WROTE AND READ BACK EXACTLY
recovery required: none
notes: MUT=06 NONREV=06 TXN_FNV=287284a2 LAST=ok; no GCS/CIR5/FDSW/function-hide/lock/reset/rollback; board remained responsive
```

Nine identity/lock/PRE assertions completed before the first mutation. The
six exact complete-dword transitions were `00000000->00000002`,
`00000000->00000001`, `00000020->08000020`, two instances of
`b2b477cc->b2b577cc`, and `0008c008->000bc008`. Final byte-wide FDSW,
RCBA and LPC identity assertions passed. After the trace was archived, the
transaction record was discarded without rollback and direct reads confirmed
that all six values remained exact.

Raw run SHA-256 is
`3e71fc4d83e4b77fc79310bc33298bddf71c029bf79723202bf703abe385f085`;
discard/readback SHA-256 is
`822e56acb5656c83b6d8acc2ec5fc91454325272182cbff86646a81a44d77bfc`.
This validates a future isolated ICHBASE helper's write mechanics in one CAR
session only. It does not establish a functional USB, timer, IRQ, ACPI or
payload improvement.

## 2026-09-06 — B06VQ-PLATRO1 read-only platform-census build

```text
test ID: B06VQ-PLATRO1-BUILD-01
image ID: X58PROE-B06VQ-PLATRO1-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 09187e91f39c7aaa9979859009f4d4d5102230f1c8c2f70fb9593ca390b957b8; local W25Q128 fa6ddf9ffbc3857a6270a3e6e82c1ce445ae9b78e82dc67d779540482c14a4f2
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no target execution
CPU: intended Intel Xeon E5645, CPUID 000206c2; no target execution
CPU stepping: intended stepping 2
microcode revision: embedded 0000001f; not executed as PLATRO1
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited B06VQ path and one complete non-fatal PLATRO block before SeaBIOS
serial log: none for PLATRO1; COM1 0x3f8 at 115200 8N1 required before power-on
result: TWO-BUILD REPRODUCIBILITY/SOURCE/CBFS/COMPOSITE/TOP-PLACEMENT PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06VP/B06VQ and vendor recovery chips remain required
notes: no target flash, write, reset, MMIO decode change, resource creation, ACPI generation or hardware census occurred
```

The derivative keeps B06VQ's hardware policy and unmodified SeaBIOS/iPXE.
Its one new stage reads LPC, PM, PIRQ, RCBA/HPET decode and existing resource
state after resource enablement. PM-timer and HPET loops are bounded; HPET
MMIO is used only after exact decode gates and IOAPIC MMIO is explicitly
skipped. The stage is non-fatal and reports that fixed platform ranges are
not yet modelled as resources.

See the [design and target procedure](b06vq-platro1.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VQ-ICHBASE1 six-field ICH10 baseline build

```text
test ID: B06VQ-ICHBASE1-BUILD-01
image ID: X58PROE-B06VQ-ICHBASE1-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 533e4fdce0e86afc669ee18a0bdc7b813dfea959afeabe2d97302aa126efd96b; local W25Q128 a58c77058b7f83df4784ae515a6500888456cd0848fdaf0345b6f93feeb8e69a
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no ICHBASE1 target execution
CPU: intended Intel Xeon E5645, CPUID 000206c2; no target execution
CPU stepping: intended stepping 2
microcode revision: embedded 0000001f; not executed as ICHBASE1
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged intended setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path, then 54 begin, 56 complete target; 57 terminal failure
serial log: none for ICHBASE1; COM1 0x3f8 at 115200 8N1 required before power-on
result: TWO-BUILD REPRODUCIBILITY/SOURCE/CBFS/PAYLOAD/COMPOSITE/TOP-PLACEMENT PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06VP/B06VQ and vendor recovery chips remain required
notes: no target flash, reset or hardware access occurred for this build record; the separate B06VP CAR transaction is evidence for the primitive, not an ICHBASE1 image run
```

Hypothesis: the six fields marked BIOS-required in Intel ICH10 Family
Datasheet 319973-003 should be established before downstream platform
consumers. The implementation factors exactly those six masked updates into
a reusable common helper. Its board wrapper admits only exact LPC
`3a168086`, RCBA `fed1c001`, FDSW `00` and either the complete live-measured
PRE tuple or the complete TARGET tuple. A partial/mixed tuple is terminal
before the helper call. Complete TARGET is idempotent and performs no write.

The helper does not touch GCS, CIR5, FDSW, function hiding, RPFN, MAP, PMIR,
IRQ, watchdog, GPIO, SATA or lock policy. It executes before IOH bus routing,
the inherited EHCI helper, IOU0 link start and PCI scan. PLATRO1, USBTRACE1,
B06VR/B06VS/B06VT, full ICH10, ACPI and SMP remain disabled.

The release builder completed two byte-identical fixed-epoch builds, 11/11
focused ICHBASE1 tests and all 297 repository tests. It verified the pinned
clean SeaBIOS and iPXE trees, exact embedded iPXE ROM, CBFS payload controls,
local vendor composition, deterministic wrapper patch and explicit 12-MiB
erased-prefix W25Q128 top placement. The public base contains no proprietary
MSI CSI/MINIT bytes.

Expected first hardware result: exact PRE should report `write=1`, exact
TARGET should report `write=0`, and either must continue through the inherited
B06VQ/SeaBIOS path. Failure is POST `57`; recover by replacing the socketed
chip with the known-good B06VP/B06VQ or vendor image rather than broadening
the gate in place.

See the [design and target procedure](b06vq-ichbase1.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VP CAR PM/SMI/GPIO and USB interrupt census

```text
test ID: B06VP-CAR-PM-USB-RO-01
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906, verified by ROMMON id
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 e721119664b1945f1ad725c77b303010d51e4aaa7e3898b13a64b7f7e3bad71b; no programmer read-back in this retained session
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged setup; exact model not restamped
boot type: no additional boot; retained guard-fallback CAR ROMMON session
POST trace: no new boot; sealed ROMMON script execution only
serial log: two complete read-only 32-operation scripts through serial-gateway.example.invalid /dev/ttyUSB1 at 115200 8N1
result: PASS — EXACT PM/SMI/GPIO AND ALL EIGHT USB PCI INTERRUPT-PIN CENSUSES RETAINED
recovery required: none; both transactions were read-only and discarded without rollback
notes: no PM, SMI, GPIO, PIRQ, controller, IOAPIC or LAPIC register was written; PCI reads update CF8 and UART output changes UART status
```

The power-management census found `SCI_EN=0`, every PM/GPE event enable zero,
`SMI_EN=0`, `ALT_GP_SMI_EN=0`, and every `GPIO_ROUT` field disabled.  Historical
status remains pending (`PM1_STS=0801`, `SMI_STS=00006100`, GPIO-related
`GPE0_STS/ALT_GP_SMI_STS=6eff...`) and was deliberately not acknowledged.
Consequently the first SCI prerequisite is an isolated W1C of only
`PRBTNOR_STS`, while both IOAPIC GSI9 and legacy PIC IRQ9 remain masked; SMI
stays disabled because the build has no permanent SMM handler.

The USB pin census measured D26 `A/B/C/C` and D29 `A/B/C/A`, with every PCI
interrupt-line byte still zero.  Combining those measured pins with the MSI
vendor routes `D26IR=3250` and `D29IR=0237` gives direct APIC GSIs
`16/21/18/18` and `23/19/18/23`, respectively.  Legacy PIRQ route bytes remain
disabled at `80`; those direct routes can be installed later without enabling
the PIC path.  They do not explain the present keyboard failure because
SeaBIOS enumerates USB with `USBINTR=0` and polling.

See the [PM/SMI/GPIO analysis](../research/msi/b06vp-pm-smi-gpio-routing-census-2026-09-06.md),
the [USB/SeaBIOS audit](../research/msi/ich10-seabios-usb-init-readonly-audit-2026-09-06.md),
and the two capture metadata files
`../research/msi/captures/2026-09-06-b06vp-pm-smi-gpio-census.metadata.json`
and
`../research/msi/captures/2026-09-06-b06vp-car-usb-intpin-routing-census.metadata.json`.

## 2026-09-06 — B06VP CAR HPET -> IOAPIC -> LAPIC delivery probe

```text
test ID: B06VP-CAR-HPET-IOAPIC20-01
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906, verified by ROMMON id
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 e721119664b1945f1ad725c77b303010d51e4aaa7e3898b13a64b7f7e3bad71b; no programmer read-back in this retained session
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged setup; exact model not restamped
boot type: no additional boot; retained pre-QPI CAR ROMMON session
POST trace: staged sealed-script PRE/success markers followed by one deliberate failed IRR assertion; ROMMON remained responsive
serial log: complete census, arm, trigger, failure-path restore, decode restore and final read-only verification retained
result: CONTROLLED NEGATIVE — HPET CROSSED COMPARATOR, BUT BSP LAPIC VECTOR 51 NEVER ENTERED IRR IN THIS PRE-QPI STATE
recovery required: none; exact software restoration and independent final verification passed
notes: timer interrupt was disabled and IOAPIC entry 20 re-masked before the deliberate assertion; no open ROMMON transaction remains
```

HPET identity `0429b17f:8086a301` and timer-0 route capability `00f00000`
admitted IRQ20.  The main counter advanced past comparator `00100000` to
`00c1b628`, but LAPIC IRR bank 2 remained zero.  This rejects only the
pre-QPI/CAR delivery hypothesis; it does not condemn post-QPI HPET or IOAPIC
operation.  Entry 20, LAPIC SVR, timer state, comparator, counter, HPTC and OIC
were restored exactly.  A later post-QPI repetition requires a real IDT
handler, acknowledgement, EOI and bounded cleanup rather than merely polling
IRR.

See the [analysis](../research/msi/b06vp-car-hpet-ioapic-delivery-2026-09-06.md)
and [immutable experiment inventory](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VP CAR 8259/ELCR/TCO census and reversible halt proof

```text
test ID: B06VP-CAR-PIC-TCO-01
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906, verified by ROMMON id
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 e721119664b1945f1ad725c77b303010d51e4aaa7e3898b13a64b7f7e3bad71b; no programmer read-back in this retained session
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged setup; exact model not restamped
boot type: no additional boot; retained pre-QPI CAR ROMMON session
POST trace: three sealed-script executions; two read-only passes and one exact reversible TCO1_CNT mutation with automatic rollback
serial log: six immutable load/run captures through serial-gateway.example.invalid /dev/ttyUSB1 at 115200 8N1, indexed and hashed in capture metadata
result: PASS — ACTIVE TCO COUNT PROVED, HALT BIT EFFECT PROVED, EXACT ROLLBACK PASSED; UNSAFE RESET-STATE PIC/ELCR BASELINE MEASURED
recovery required: none; automatic rollback restored TCO1_CNT=0000 and final script status reported no open transaction
notes: no TCO status bit was acknowledged; no PIC, ELCR, PM, SMI, IOAPIC or LAPIC register was written; the board ended in its exact pre-test control state
```

Both 8259 mask bytes were `00`, leaving all legacy IRQ inputs open, and both
ELCR bytes were `00`, leaving IRQ9 edge-triggered.  This is not a valid
payload-facing baseline and explains why LAPIC virtual-wire setup alone cannot
be treated as complete legacy interrupt initialization.

The same retained state had `GCS=00200404` with `NR=0`,
`TCO1_STS=0008`, `TCO2_STS=0002`, `TCO1_CNT=0000`, `TCO2_CNT=0008` and
`TCO_TMR=0004`.  Repeated `TCO_RLD` values changed while halt was clear.  An
exactly gated write of only `TCO1_CNT.TCO_TMR_HLT` produced `0800`, froze all
three subsequent count samples at `0002`, and rolled back with exact
readback to `0000`.  This supports a narrow persistent TCO-halt release next;
timeout acknowledgement, TCO locking, 8259 initialization and IRQ9 ELCR
policy remain separate hypotheses.

See the [analysis](../research/msi/b06vp-car-pic-tco-census-2026-09-06.md)
and [immutable capture inventory](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06VP CAR standard 8259 init and PIT-to-PIC proof

```text
test ID: B06VP-CAR-PIC-PIT-01
image ID: X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906, verified by ROMMON id
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: expected local W25Q128 e721119664b1945f1ad725c77b303010d51e4aaa7e3898b13a64b7f7e3bad71b; no programmer read-back in this retained session
flash chip: socketed W25Q128.V..M, 16 MiB target
board revision: MSI X58 Pro-E / MS-7522 target
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2, microcode 0000001f
DIMM: BLS4G3D1609DS1S00, 4096 MiB dual-rank x8, sole responding SPD address 0x54
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
boot type: no additional boot; retained pre-QPI CAR ROMMON session
POST trace: four sealed scripts; exact 8259/ELCR setup, read-only census, PIT one-shot proof and PIC cleanup
serial log: eleven immutable load/run/discard captures through serial-gateway.example.invalid /dev/ttyUSB1 at 115200 8N1
result: PASS — STANDARD 8259/IRQ9 ELCR STATE VERIFIED; PIT0 CAUSED MASTER PIC IRR0 0->1 WHILE MASKED; PIC CLEANUP VERIFIED
recovery required: cold reset only for an exact PIT reset state; board remained responsive and no transaction is open
notes: CPU ExtINT acceptance is not claimed because CAR retained IF=0 and no temporary IDT handler existed
```

The standard ICW sequence produced master/slave vectors `20h/28h`, masks
`fb/ff` and ELCR `00/02`. The LAPIC remained software-disabled with LINT0 and
LINT1 masked. PIT channel 0 mode 0/count `0800` set master-PIC IRR0 on the
first bounded poll without unmasking IRQ0. Reinitializing both PICs cleared
IRR0 and restored the intended masks. ExtINT bypasses LAPIC IRR/ISR, so the
remaining CPU-delivery proof needs a compiled temporary IDT and handler rather
than another LAPIC-IRR poll.

See the [analysis](../research/msi/b06vp-car-pic-pit-proof-2026-09-06.md)
and [immutable capture inventory](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06WB deterministic early TCO-halt build

```text
test ID: B06WB-BUILD-01
image ID: X58PROE-B06WB-ICH10-TCO-HALT-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 1e6d0b4ef23dbf255f51f694fb05ae6ec50e1610210bece4912b3e595ed420cc; local W25Q128 9f7cd3d9ce30cb7d99e5f1644115f0d9ca66bc7ae2c02cd37dedfbce02bb5bf4
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no B06WB target execution
CPU/DIMM/GPU: intended fixed E5645 / SPD54 BLS4G3D1609DS1S00 / AMD HD 5450 configuration
boot type: none; source/build/static-artifact record only
POST trace: expected 79 begin, 7a ready or terminal 7b rejection
serial log: none for B06WB; COM1 0x3f8 at 115200 8N1 required before a future power-on
result: TWO-BUILD DETERMINISM, 7/7 FOCUSED AND 361/361 COMPLETE TESTS, BINARY/CBFS/COMPOSITE/TOP-PLACEMENT AND INDEPENDENT SOURCE AUDIT PASS; NOT HARDWARE TESTED
recovery required: none during construction; known-good B06WA and vendor recovery chips remain required
notes: B06VY, B06VZ and B06WA target-hardware PASS records are explicit prerequisites before B06WB is flashed
```

B06WB inherits B06WA and adds only an exact-gated early transition of
`TCO1_CNT` from `0000` to `0800`, setting `TCO_TMR_HLT` without setting
`TCO_LOCK`, changing `GCS.NR`, acknowledging timeout history, reloading the
watchdog, or changing its timer. It admits `0000` and the idempotent retained
state `0800` only, and verifies complete LPC/RCBA/PM decode plus all preserved
TCO state before later platform writes. Independent disassembly found the
single intended 16-bit output to port `0568` with value `0800`; no functional
build blocker was found.

See the [design and gated test procedure](b06wb-ich10-tco-halt.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06WC integrated experimental platform release build

```text
test ID: B06WC-BUILD-01
image ID: X58PROE-B06WC-INTEGRATED-PLATFORM-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
ROM hash: public base 133638bfac31d3e675ed3535cbd506d1d8b3dfe4f4e6c741a6d80ee9b18930ef; local W25Q128 c7482e03098f1e9bb8f845401d1bdc33e97a1b131ceffd9bd9fc27e089254831
flash chip: socketed W25Q128.V..M, 16 MiB target; not programmed for this record
board revision: MSI X58 Pro-E / MS-7522 target; no B06WC target execution
CPU: intended Intel Xeon E5645, CPUID 000206c2; no B06WC hardware execution
CPU stepping: intended stepping 2; no B06WC hardware execution
microcode revision: embedded 0000001f; not executed as B06WC
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged test setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path, then 79/7a TCO, 7c/7d PIC, 7f/80/81 quiet ACPI, 83/84 USB admission and normal SeaBIOS/iPXE path; terminal failures 7b/7e/82/85/86 retain the complete serial reason
serial log: none for B06WC; COM1 0x3f8 at 115200 8N1 must be armed before power-on
result: TWO-BUILD DETERMINISM, 403/403 COMPLETE TESTS, FOCUSED/HOST/IASL/AML/BINARY/CBFS/COMPOSITE/TOP-PLACEMENT AND THREE INDEPENDENT READ-ONLY AUDITS PASS; NOT HARDWARE TESTED
recovery required: none during construction; socketed known-good coreboot and vendor recovery chips remain required for first execution
notes: predecessor hardware qualification is intentionally waived for this integrated experimental image; no flash, reset, register access, ACPI runtime, USB endpoint, SATA device or OS boot occurred for this record
```

B06WC changes the release strategy from one separately qualified register
delta per image to an aggressively integrated but still exact-gated platform
candidate. It preserves the hardware-reached vendor-assisted DDR3/High-QPI,
postcar/ramstage, selective PCI, Radeon physical-VBIOS display, SeaBIOS and
RTL8168 DHCP/TFTP paths. It automatically layers the corrected AHCI route,
all-six-port PCS/SCLK policy, minimal AHCI MMIO enablement, TCO halt, standard
8259 vectors/masks, IRQ9 ELCR policy, deterministic IOAPIC masking, quiescent
HPET decode, quiet native ACPI-mode transition, minimal FADT/MADT/MCFG/DSDT
and read-only USB admission into that path.

The normal boot never executes the new destructive interrupt diagnostic.
ROMMON exposes it only after `unlock WRITE` followed by `irqprobe pit`; it uses
a bounded temporary CAR IDT to test PIT0 through PIC IRQ0 and LAPIC ExtINT,
then cleans up readable state. A cold reset is mandatory afterward because
the PIT mode/count is not reversible.

The complete suite passed 403/403 after the release build. The five focused
B06WC Python modules passed 38/38, the separate IRQ-probe host state machine
passed all four grouped test functions, and independent review found no
release-blocking ordering or safety defect. IASL compiled the DSDT with zero
errors, warnings or remarks; recompilation produced the exact embedded
394-byte AML. The 16-MiB artifact has a fully erased lower 12 MiB and its
upper 4 MiB exactly equal the verified deterministic composite.

See the [integrated design and first-run procedure](b06wc-integrated-platform.md)
and [release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-06 — B06WC-HW-01 reaches POST 63 after PCI allocation

```text
test ID: B06WC-HW-01
image ID: X58PROE-B06WC-INTEGRATED-PLATFORM-20260906
build commit: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus the documented dirty X58 port
ROM hash: expected public base 133638bfac31d3e675ed3535cbd506d1d8b3dfe4f4e6c741a6d80ee9b18930ef; expected local W25Q128 c7482e03098f1e9bb8f845401d1bdc33e97a1b131ceffd9bd9fc27e089254831; no programmer read-back
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; exact PCB revision not reported
CPU: Intel Xeon E5645, CPUID 000206c2, stepping 2, microcode 0000001f
DIMM: BLS4G3D1609DS1S00., 4096 MiB, dual-rank x8, sole SPD responder 0x54
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: not reported
boot type: one-second Shelly relay interruption; electrical G3 not independently proven
POST trace: three CSI phases, High-QPI, MINIT return, UC RAM tests, CBMEM, postcar, ramstage and PCI allocation; terminal 63
serial log: research/msi/captures/2026-09-06-b06wc-hw-01-full-platform.raw; 45014 bytes; SHA-256 ba7333441d273e32043b874489372bed3091f5fedda4bb9254364cbbabe88a04
result: MAJOR PARTIAL PASS — SAFE SOFTWARE-POLICY REJECTION BEFORE THE FIRST PCS WRITE
recovery required: none; the exact gate intentionally halted normal execution
notes: guard was explicitly cleared from CMOS 0e=ec/invalid to 2c/valid before the relay cycle
```

The run proved trained DDR3 and High-QPI for this one fixed configuration,
MINIT return, all bounded uncached memory tests, CBMEM, postcar, ramstage, the
selective PCI tree and resource allocation.  The terminal line measured the
intended quiescent AHCI hardware tuple: D31:F2 `8086:3a22/010601`, hardware
command `0000`, MAP `0060`, exact 2-KiB ABAR `cfcff000`, PCS `0000`, SCLKCG
`00000000` and D31:F5 absent.  Only coreboot's future in-memory command policy
was `0003`, because legacy I/O BARs 0..4 accompanied the AHCI MMIO BAR.  B06VV
correctly required memory-only policy `0002` and stopped at POST `63`; POST
`5e`, the first PCS-write boundary, never appeared.

This observation justifies the narrow B06WD successor: require the complete
measured tuple, clear only `PCI_COMMAND_IO` in the in-memory device policy,
prove that no hardware register changed, and then enter the unchanged SATA
stages.  See the immutable [B06WC-HW-01 analysis](../research/msi/b06wc-hw-01-2026-09-06.md).

## 2026-09-07 — B06WD SeaBIOS input successor release build

```text
test ID: B06WD-BUILD-01
image ID: X58PROE-B06WD-SEABIOS-INPUT-20260906
build commit: a66879da91968b2fad3fc63fbaf1678651c952d9 on x58-pro-e/seabios; x58-pro-e/edk2 preserved at common checkpoint 02b1c98fec2d3c8889c624d3bb12a15d1cd9a129
ROM hash: public base dcbaf409103db4e1b032d32b54d36d4d62198cbc78e9e7534de38aad1687b005; local 4-MiB composite 8a695a85afafb452cbd2339508fcf8abd633cb68128c20c97a19abce364f9e12; local W25Q128 9c2bd669f34d913d41e9902320e753e13a24ba3d936f1bd0a6e7f1b23e50533f
flash chip: intended socketed W25Q128.V..M, 16 MiB; not programmed for this record
board/CPU/DIMM/GPU: intended fixed MSI X58 Pro-E / E5645 / sole SPD54 BLS4G3D1609DS1S00 / HD 5450 configuration
boot type: none
POST trace: expected SATA policy 87/88 or terminal 89; input 8a/8b/8c or terminal 8d
serial log: none
result: BUILD-TESTED — TWO BYTE-IDENTICAL RELEASE BUILDS AND 410/410 TESTS PASS; HARDWARE UNTESTED; NOT FLASHED
recovery required: none during construction; known-good socketed recovery chip remains mandatory for first execution
notes: B06WD is the SeaBIOS-line successor; the separate EDK2 line remains at the shared B06WC checkpoint
```

B06WD pairs the software-only SATA policy correction justified by B06WC-HW-01
with an exact-gated ICH10R KBC-decode transition and a bounded primary PS/2
keyboard probe.  The existing UHCI/EHCI SeaBIOS keyboard path remains enabled
as an independent input test.  Two clean release-wrapper executions produced
byte-identical coreboot/SeaBIOS/iPXE outputs, and the complete 410-test suite
passed.  No B06WD target run or flash is claimed here.  The exact delta,
expected logs and first-test/recovery procedure are in
[b06wd-seabios-input.md](b06wd-seabios-input.md); artifact provenance is in the
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-07 — B06WD-HW-01 reaches ACPI construction and fails closed before MCFG

```text
test ID: B06WD-HW-01
image ID: X58PROE-B06WD-SEABIOS-INPUT-20260906, observed in retained ramstage output
build commit: a66879da91968b2fad3fc63fbaf1678651c952d9 on x58-pro-e/seabios
ROM hash: expected public base dcbaf409103db4e1b032d32b54d36d4d62198cbc78e9e7534de38aad1687b005; expected local 4-MiB composite 8a695a85afafb452cbd2339508fcf8abd633cb68128c20c97a19abce364f9e12; expected local W25Q128 9c2bd669f34d913d41e9902320e753e13a24ba3d936f1bd0a6e7f1b23e50533f; no programmer read-back retained
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 target; exact PCB revision not restamped
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: fixed-test 0000001f; retained continuation starts after its telemetry
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical label not restamped
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS, enumerated in this run
NIC: RTL8168 10ec:8168, enumerated in this run
PSU: unchanged test setup; exact model not restamped
boot type: operator reported new image active; retained evidence does not distinguish cold, warm or electrical-G3 start
POST trace: authoritative serial continuation covers late RAM tests/CBMEM, postcar, ramstage, PCI/resources, platform/SATA/AHCI/input work, device finalization and terminal ACPI failure; no independent diagnostic-card code retained
serial log: research/msi/captures/2026-09-07-b06wd-hw-03-ramstage-acpi-fail.raw; 38495 bytes; SHA-256 2a8572670d58041475571a7707bc2996fcaff83a148c36eecf21c6d59054041e
result: MAJOR PARTIAL PASS — SATA POLICY 0003->0002 WITHOUT HARDWARE CMD WRITE AND SUBSEQUENT AHCI READY PASSED; KBC DECODE OPENED BUT INTERFACE TEST 03/ACK FF FAILED; ACPI ADDED FADT/SSDT EVIDENCE THEN FAILED CLOSED AT WRONG-BDF PCIEXBAR GATE BEFORE MCFG/SEABIOS
recovery required: none recorded; terminal gate deliberately halted the path
notes: the 2-byte hw-01 prestart artifact is non-evidence; hw-02 is replay-contaminated; hw-03 is the authoritative coherent continuation; no physical keyboard, complete ACPI, SeaBIOS or payload success is claimed
```

The run confirms the B06WD SATA correction once: the in-memory command policy
changed from `0003` to memory-only `0002`, hardware CMD remained `0000`, and
the unchanged route/PCS/SCLK/AHCI sequence reached `AE=1`, `PI=3f` with a
dynamically allocated MEM-only ABAR.  PCI enumeration found the HD 5450 and
RTL8168, resource audits passed, and coreboot initialized and finalized its
selected devices.

The KBC decode transition `LPC_EN 2001 -> 2401` changed port `64` from `ff` to
`00`, but the coreboot interface test returned `0x3` and keyboard reset ACK was
`ff`; PS/2 input therefore failed.  USB admission was structurally complete
but classified the observed electrical state as `OC_NO_CONNECT`, so USB input
is also unproved.

ACPI construction logged native-minimal FADT/FACP and SSDT additions, then
reported `PCIEXBAR=ffffffff:ffffffff HOST=34058086` and failed closed.  The
tested ACPI source read `00:00.1`, while the X58 SAD and the already-passed
platform gates use `ff:00.1`; the exact ECAM host ID remained readable.  This
is a software wrong-BDF false-negative, not evidence of a lost QPI/IOH link.
The next bounded successor can retain the same fail-closed check at the correct
SAD BDF.  See the immutable [B06WD-HW-01 analysis](../research/msi/b06wd-hw-01-2026-09-07.md).

## 2026-09-07 — B06WE corrected ACPI SAD-BDF release build

```text
test ID: B06WE-BUILD-01
image ID: X58PROE-B06WE-ACPI-SAD-BDF-FIX-20260907
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d on x58-pro-e/seabios
ROM hash: public base 2097ae97f0b54cc1a8997811fbd849f66d5b5c65e59c5adb840f7adfec73af53; local 4-MiB composite 3219f86e9aaacc2ba6ab12848a5aed9da3b9cfcceb240aee8b14262a31224cc6; local W25Q128 d9b401a743b945918c15f9e6f5379ce4af99392d0288b46d87db6ef9c85c8ada
flash chip: intended socketed W25Q128.V..M, 16 MiB; not programmed for this record
board revision: intended MSI X58 Pro-E / MS-7522; exact PCB revision to restamp at execution
CPU: intended Intel Xeon E5645, CPUID 000206c2
CPU stepping: intended stepping 2
microcode revision: embedded fixed-test 0000001f; not executed as B06WE
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole responding SPD address 0x54; physical label to restamp
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged test setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected inherited path, then 8e corrected MCFG gate begin, 8f exact gate pass, or terminal 86 with complete serial reason
serial log: none for B06WE; COM1 0x3f8 at 115200 8N1 must be armed before power-on
result: BUILD-TESTED — TWO BYTE-IDENTICAL RELEASE BUILDS, 415/415 TESTS, LINKED-BINARY/CBFS/COMPOSITE/TOP-PLACEMENT AND INDEPENDENT ARTIFACT AUDIT PASS; HARDWARE UNTESTED; NOT FLASHED
recovery required: none during construction; socketed known-good recovery chip remains mandatory for first execution
notes: B06WE changes only the read-only ACPI SAD/PCIEXBAR gate from absent 00:00.1 to measured ff:00.1; PS/2 and USB behavior is deliberately unchanged
```

B06WD's retained hardware trace showed exact ECAM host ID `34058086` while
the MCFG callback returned `ffffffff:ffffffff` from `00:00.1`.  Other
already-passed X58 gates address the actual CPU-side SAD at `ff:00.1`.
B06WE therefore reads ID and PCIEXBAR from that BDF, requires complete values
`2d818086/00000000:e0000001/34058086`, and emits POST `8e` before and `8f`
after the check.  The change introduces no PCI, MMIO, PM, SATA, USB, PS/2 or
reset write.

The linked ramstage contains the expected CF8 selectors and constants; two
clean builds are byte-identical, the embedded revision is clean
`a2eb375438c8`, and the 16-MiB flash image has an erased lower 12-MiB prefix
with its deterministic 4-MiB composite placed exactly at the top.  No B06WE
target execution or payload entry is claimed here.  See the
[design/test procedure](b06we-acpi-sad-bdf-fix.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-07 — B06WD-HW-02 repeats the all-port USB overcurrent state

```text
test ID: B06WD-HW-02
image ID: X58PROE-B06WD-SEABIOS-INPUT-20260906
build commit: a66879da91968b2fad3fc63fbaf1678651c952d9 on x58-pro-e/seabios
ROM hash: expected local W25Q128 9c2bd669f34d913d41e9902320e753e13a24ba3d936f1bd0a6e7f1b23e50533f; no programmer read-back retained
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board/CPU/DIMM/GPU: fixed MSI X58 Pro-E / E5645 / sole SPD54 BLS4G3D1609DS1S00 / HD 5450 configuration
boot type: one-second Shelly mains interruption; electrical G3 not independently proved
POST trace: complete logical reset-to-terminal serial path across three exclusive acquisitions
serial logs: 2026-09-07-b06wd-usb-cold-02.raw (20132 bytes, b4ffc764...ffcb), silent 02b interval, and 02c final (38640 bytes, 5789a719...19b5)
result: REPEAT MAJOR PARTIAL PASS — platform/SATA/AHCI path repeated; all twelve EHCI/UHCI port views again report live overcurrent active and no connection; PS/2 protocol again failed; expected wrong-BDF ACPI halt
recovery required: none; the deliberate ACPI gate halted execution
notes: physical USB attachment and port were not independently restamped; no descriptor, HID initialization, accepted key action or SeaBIOS entry is claimed
```

The run independently repeated the B06WD structural USB state.  Both EHCIs
were in D0 with valid memory BARs and all six UHCIs had I/O decode.  USB
function-disable, EHCI clock, PPO, MAP and OC-pin-mux gates were coherent.
Nevertheless every EHCI PORTSC was `00003030` and every UHCI PORTSC was
`0c80`, summarized as `OCA=fff/OCC=fff`, `CCS=000/PE=000` in both controller
families.  The OCA fields are live read-only external overcurrent inputs;
clearing sticky OCC cannot remove them.  The common board-level OC#/VBUS/USB
power-switch chain is therefore the leading hypothesis, ahead of descriptor,
HID, interrupt or SeaBIOS-driver issues.

The current terminal halt has no command interpreter.  A reset-retentive path
into the CAR ROMMON, or a successor with a deliberately late DRAM monitor, is
required for bounded GPIO56 and post-allocation USB experiments.  Full
capture provenance and the exact non-claims are in
[B06WD-HW-02](../research/msi/b06wd-hw-02-usb-repeat-2026-09-07.md).

## 2026-09-07 — B06WF late interactive USB-lab release build

```text
test ID: B06WF-BUILD-01
image ID: X58PROE-B06WF-LATE-USB-LAB-20260907; internal lab ID B06WF-LATE-USB-LAB2
build commit: source base a2eb375438c85cd7908140636cbee8407bee8c0d plus the uncommitted B06WF delta on x58-pro-e/seabios
ROM hash: public base ffe200f4ec40d03409d1590c93a522324fbf1cc902a74b54bbf6a720a60dd709; local 4-MiB deterministic composite a36c54d14e96ad93fbcb97f3f9820779c77d6961d12fed7090a8931a63f3c9fa; local W25Q128 a017b3db7585d5ba14c93ac83c329a51512245f81d015d07b4ad01b402ae639a
flash chip: intended socketed W25Q128.V..M, 16 MiB; not programmed for this record
board revision: intended MSI X58 Pro-E / MS-7522; exact PCB revision to restamp at execution
CPU: intended Intel Xeon E5645, CPUID 000206c2, stepping 2, microcode 0000001f
DIMM: intended BLS4G3D1609DS1S00, 4096 MiB dual-rank x8, sole SPD responder 0x54
GPU/NIC: intended AMD Radeon HD 5450 physical VBIOS / RTL8168 10ec:8168
boot type: none; source/build/static-artifact record only
POST trace: expected 90 monitor entry, 91 exact baseline ready, 92 mutation, 93 rollback, 94 continue, 95 latched failure, 96 non-reversible W1C, 97 persistent GPIO57/OCA-ready
serial log: none for B06WF; COM1 0x3f8 at 115200 8N1 must be armed before power-on
result: BUILD-TESTED — TWO BYTE-IDENTICAL RELEASE BUILDS; 427/427 COMPLETE AND 12/12 B06WF SOURCE-CONTRACT TESTS PASS; CBFS/COMPOSITE/WRAPPER/TOP-PLACEMENT PASS; TARGET HARDWARE UNTESTED
recovery required: none during construction; socketed known-good recovery chip remains mandatory for the first execution
notes: no automatic USB/GPIO mutation, no generic register writer and no PORTSC RMW; one-shot fixed commands only; any failed preflight/readback/rollback blocks payload continuation until reset
```

B06WF adds a `BS_WRITE_TABLES` / `BS_ON_ENTRY` serial lab after device
initialization/finalization and before ACPI tables and SeaBIOS. Its reversible
GPIO56 and CONFIGFLAG probes restore and re-read their fixed controls. The OC
acknowledgement uses fixed W1C images and deliberately makes the boot
non-continuable. The primary new path reproduces the vendor-correlated
GPIO57/H_PWRGD ordering as a manual one-shot command: set only bank-2 MSB bit 1
to output while its latch is low, wait `udelay(0x10000)`, set that target high,
sample both EHCIs and all six companion UHCIs, and require every OCA indication
clear. On success, precisely that GPIO57 direction/level deviation is allowed
to persist through the final continuation gate into SeaBIOS.

The delay units are explicit: the preceding XRS operand `0x10000` meant 65,536
uncalibrated `pause` instructions, not microseconds; B06WF independently uses
65,536 calibrated microseconds. GPIO level comparisons assert only known
configured-output bits and the target. Dynamic native GPIO inputs, USB CCS and
UHCI LSDA are telemetry rather than admission values. Failure rollback still
attempts to restore input direction if level readback fails.

The preceding live B06WD/XRS experiment is the basis for the hypothesis, not a
B06WF execution claim. It changed all EHCI ports from `00003030` to `00003020`
and cleared companion-UHCI OCA bit 10 on all ports; sticky OCC/OCI remained and
real CCS/LSDA values appeared on attached ports. The first B06WF target run is
therefore `usb status`, `unlock GPIO57-PWRGD`, `usb gpio57 pwrgd`, require both
OCA bitmaps `000`, then `continue` to test SeaBIOS enumeration and keyboard
input. See the [B06WF procedure](b06wf-late-usb-lab.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-07 — B06WD CAR proves GPIO57 USB release and UHCI port control

```text
test ID: B06WD-USB-GPIO57-LIVE-01
image ID: X58PROE-B06WD-SEABIOS-INPUT-20260906, retained CAR recovery ROMMON
build commit: a66879da91968b2fad3fc63fbaf1678651c952d9 on x58-pro-e/seabios
ROM hash: expected local W25Q128 9c2bd669f34d913d41e9902320e753e13a24ba3d936f1bd0a6e7f1b23e50533f; no new programmer read-back retained
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; exact PCB revision not restamped during this live series
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: embedded fixed-test 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical label not restamped
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged fixed test setup; exact model not restamped
boot type: multiple isolated CAR transactions with a mandatory one-second AC interruption after every invasive GPIO57 test; electrical G3 not independently instrumented
POST trace: retained B06WD CAR recovery path and ROMMON prompt between experiments
serial logs: complete verified inventory in research/msi/b06wd-usb-gpio57-hw-2026-09-07.md; decisive reset end-half a3a6674c219afae90483eb176c4e1735391ef0f74bdc33f5a727743e4bbd921e
result: MAJOR CONTROLLED ELECTRICAL/ROOT-PORT PASS — GPIO56 HIGH-PRESERVING NEGATIVE CONTROL; GPIO57 LOW/DELAY/HIGH CLEARED LIVE OCA ON BOTH EHCIS AND ALL UHCIS; D26:F0 PORT 1 RESET/CLEAR/ENABLE ENDED 0987 WITH RUN=ok
recovery required: yes — exactly one second AC-off before the next transaction; fresh CAR recovery prompts retained
notes: successful reset script SHA-256 058c831aac120e2f4d12bc07d70621d67a6523d043c1c8113a0e84ae6d1b49d4, sealed FNV bb167682; no descriptor, USB address, HID binding or accepted key is claimed
```

The GPIO56 negative control changed the already-high `USB_MODE` pin from
input to output/high; all EHCI2 ports remained `00003030`.  It therefore rules
out only direction ownership plus an already-high GPIO56 level as the missing
release action.  It does not establish GPIO56 polarity and does not authorize
GPIO56-low.

The vendor-correlated GPIO57/`H_PWRGD` sequence changed the bank-2 direction
byte `0f->0d`, held the target output low for a bounded delay, then drove it
high.  Both EHCI groups changed all six PORTSC values `00003030->00003020`:
live OCA cleared while sticky OCC remained.  D29's six UHCI ports became
`0880`.  D26 became `0983 0880 0880 0880 0880 0883`, so all six D26 OCA bits
also cleared while two ports exposed real connected states.

The GP_LVL2 byte was written `15->17` for target bit 1 but subsequently read
`1f`.  Byte bit 3 is in native-function mode under
`GPIO_USE_SEL2=030300ff`; a GP_LVL read there is not a reliable GPIO latch
value.  The extra native bit and the resulting generic full-byte rollback
readback difference (`1d` versus baseline `15`) are therefore caveats, not a
second GPIO write.  The target GPIO57 bit itself restored low; the independent
EHCI/UHCI OCA transition is the causal observation.

The final D26:F0 port-1 test used a SeaBIOS-style register sequence.  After
GPIO57 release it admitted `0983`, wrote reset `0200`, observed `0a82`, cleared
reset and observed `0983`, then wrote PE `0004` and observed `0987`.
ROMMON completed `RUN=ok`, program FNV `bb167682`, transaction FNV
`a4390453`.  The exact program/load and unlock prefix are retained separately
from the complete end half; this is explicitly a split capture, not one
uninterrupted serial transcript.

XRS `delay 0x10000` denotes 65,536 executions of a loop containing `PAUSE`.
It is not 65 ms, not a calibrated unit, and does not prove USB timing
compliance.  The experiment proves the observed register progression only.
No descriptor enumeration, HID initialization, accepted keyboard input,
SeaBIOS USB success, automatic B06WF execution, or stability count is claimed.
See the immutable
[B06WD GPIO57 hardware note](../research/msi/b06wd-usb-gpio57-hw-2026-09-07.md).

## 2026-09-07 — B06WD CAR GPIO57 full-speed companion-port follow-up

```text
test ID: B06WD-USB-GPIO57-UHCI6-P2-01
image ID: X58PROE-B06WD-SEABIOS-INPUT-20260906, retained CAR recovery ROMMON
build commit: a66879da91968b2fad3fc63fbaf1678651c952d9 on x58-pro-e/seabios
ROM hash: expected local W25Q128 9c2bd669f34d913d41e9902320e753e13a24ba3d936f1bd0a6e7f1b23e50533f; no new programmer read-back retained
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board/CPU/DIMM/GPU: unchanged fixed MSI X58 Pro-E / E5645 / sole SPD54 BLS4G3D1609DS1S00 / HD 5450 configuration
boot type: isolated retained-CAR transaction followed by the established one-second AC recovery
POST trace: retained B06WD CAR ROMMON; sealed script completed RUN=ok
serial log: 2026-09-07-b06wd-usb-car-gpio57-uhci6-port2-reset-short-run.raw
result: CONTROLLED PASS — D26:F2 port 2 exposed a full-speed connected state after GPIO57 release and accepted reset/clear/enable
recovery required: yes, established one-second AC interruption before the next transaction
notes: script FNV c5a0d015; script SHA-256 c3b41ebb984811737e95d4cebf23019e6c6008b22ae0e63bed26fb8e3812c84c; shortened PAUSE count is not calibrated time
```

The port progressed from baseline `0c8a` to `088b` after GPIO57 high, then
`0a8a` with reset asserted, back to `088b` after reset clear, and `088f` after
port enable. Alongside the earlier D26:F0 low-speed progression
`0983 -> 0a82 -> 0983 -> 0987`, this proves live CCS, OCA release and PE
control for both low- and full-speed attachments. It still does not prove a
USB descriptor read, address assignment, HID binding, accepted key,
SeaBIOS enumeration, calibrated settle interval, B06WF execution or stability.

## 2026-09-07 — B06WD hardened UART-helper read-only validation

```text
test ID: B06WD-UART-QUEUE-HARDEN-01
image ID: X58PROE-B06WD-SEABIOS-INPUT-20260906, retained CAR recovery ROMMON
build commit: a66879da91968b2fad3fc63fbaf1678651c952d9 on x58-pro-e/seabios
ROM hash: expected local W25Q128 9c2bd669f34d913d41e9902320e753e13a24ba3d936f1bd0a6e7f1b23e50533f; no new programmer read-back retained
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; exact PCB revision not restamped
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: embedded fixed-test 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical label not restamped
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged fixed test setup; exact model not restamped
boot type: read-only command on the already powered CAR session after a successful two-second Shelly AC recovery
POST trace: retained B06WD CAR ROMMON; no reset or POST transition requested
serial log: research/msi/captures/2026-09-07-b06wd-uart-hardened-id.raw, 86 bytes, SHA-256 1f1358cdeb0e8902ceb4eb18f7dfbb9dbdc31b524524a8af19f2c91cb29a61fa
result: PASS — one bounded `id` command returned one clean B06WD identity and prompt without replayed status/ACK fragments
recovery required: no
notes: ConsolePi helpers use fail-closed queue clearing before command transmission; this validates the observed command round trip only, not the absence of every possible USB-UART transport failure
```

The updated ConsolePi helpers were syntax-checked remotely after deployment.
Normal command sessions clear pending RX and TX, while `--keep-input` preserves
RX boot output but still clears pending TX. Already transmitted bytes and a
separate orphan writer remain outside that mechanism, so all hardware commands
must continue to use one process and the shared UART lock.

## 2026-09-07 — B06WF clean cold boot reaches CSI pass two, then does not return

```text
test ID: B06WF-QPI-PASS2-RETURN-01
image ID: X58PROE-B06WF-LATE-USB-LAB-20260907
build commit: a2eb375438c8-dirty-x58-pro-e-b06wf
ROM hash: a017b3db7585d5ba14c93ac83c329a51512245f81d015d07b4ad01b402ae639a
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board/CPU/DIMM/GPU: unchanged fixed MSI X58 Pro-E / Xeon E5645 CPUID 000206c2 / sole SPD54 BLS4G3D1609DS1S00 / Radeon HD 5450
boot type: cold AC cycle via Shelly, 2 s off interval; CMOS auto-guard explicitly cleared in ROMMON with `unlock RESET` + `autoguard clear`
POST trace: reset vector, ICH10 early BAR, SPD54 full read, PCIEXBAR, QPI `PHASE=01`, CSI internal reset, QPI `PHASE=02`
serial log: [2026-09-07-b06wf-authorized.raw](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index), 4934 bytes, SHA-256 89d04e12f6384bb24bdaad30717189618d8c3a1b26da66041e683f95712d9401
result: PARTIAL — SPD and PCIEXBAR gates pass; CSI pass-one reset is observed and the retained pass-two signature is accepted. The pass-two CSI call emits no return, `PASS2_ACCEPTED`, late USB monitor, ramstage, or payload marker before capture timeout.
recovery required: yes — subsequent 2 s AC cycle restored the CAR ROMMON
notes: this is a clean capture, unlike the earlier replay-contaminated long trace. It does not prove DRAM training, QPI high-speed link completion, USB enumeration, or SeaBIOS execution. B06WF currently selects the inherited B06VL broad QPI path; the apparent B06WF identity does not imply that its late USB monitor was reached.
```

The explicit guard-clear command changed CMOS diagnostic byte `0x0e` from the
in-progress value `0xec` to the cold authorization `0x2c`. The first cold
entry then passed the exact SPD and X58 revision gates and left the expected
I801/CPU/IOH tuple before CSI pass one. The reset-return entry had I801
signature `08:31:ce:68:97`, CMOS `0xec`, and `PHASE=02`; the absence of any
subsequent bytes is therefore evidence of a non-returning or reset-inducing
pass-two vendor call, not evidence of a serial-capture prompt failure. The
board was recovered by the documented Shelly AC cycle.

## 2026-09-07 — B06WF repeat reaches the complete pass-two acceptance

```text
test ID: B06WF-QPI-PASS2-ACCEPT-02
image ID: X58PROE-B06WF-LATE-USB-LAB-20260907
boot type: second cold AC cycle after explicit ROMMON guard clear, 2 s off
serial log: [2026-09-07-b06wf-repeat.raw](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index), 11344 bytes, SHA-256 fd4991e20532e65fc23b5072049c6348bc545b6d69161d48df7188fe7fa295be
result: PASS THROUGH QPI PASS2 — `PHASE=01`, `PHASE=02`, `B06V8_CSI RETURN EAX=1/0/2a6`, exact CSI state, `PASS2_ACCEPTED`, then `PHASE=03` with stable `CPU_A0=00017400`; third SPD and PCIEXBAR preflight also emitted. The capture ended before a terminal marker, and a later suffix capture was rate-implausible ConsolePi replay, so no RAMSTAGE/SeaBIOS/USB claim is made.
recovery required: yes — board was left in the experimental continuation path and requires the documented AC recovery before another transaction
```

This repeat demonstrates that the pass-two CSI call is not unconditionally
non-returning; the first attempt stopped before its SPD telemetry, while the
repeat completed it and accepted the tuple. The remaining uncertainty is now
after pass-two acceptance (pass-three/DRAM handoff), not the initial SPD or
PCIEXBAR gate.

## 2026-09-07 — B06WG completes automatic USB, keyboard and iPXE entry

```text
test ID: B06WG-HW-USB-01
image ID: X58PROE-B06WG-AUTO-USB-SEABIOS-20260907
build commit: source base a2eb375438c85cd7908140636cbee8407bee8c0d plus the B06WG delta on x58-pro-e/seabios
ROM hash: expected local W25Q128 ba5c6d24365ed7fceee4fe64b4a6179244fbe60c295f13349c6da30eb764ac7c; no programmer read-back retained
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; exact PCB revision not restamped
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: embedded fixed-test 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical label not restamped
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged fixed test setup; exact model not restamped
boot type: one-second Shelly mains interruption; electrical G3 not independently instrumented
POST trace: complete automatic three-phase QPI/MINIT, fast-postmem, postcar, ramstage, platform, GPIO57, ACPI and payload passage on serial
serial log: research/msi/captures/2026-09-07-b06wg-clean-cold2.raw, 121124 bytes, SHA-256 caa59c5e4db3a4e9f2053703c10779a309e100b3528dc2b21f852c96af022fcc
result: MAJOR PASS — B06WG automatic GPIO57 release and both OCA gates passed; SeaBIOS initialized VGA, EHCI/UHCI, a USB keyboard and a Transcend 128GB USB mass-storage device; the operator confirmed working USB-keyboard input; RTL8168 iPXE obtained DHCP and TFTP-chainloaded netboot.xyz
recovery required: none for the initial passage; later experiments ended in the documented reset-state CAR guard
notes: this is one accepted-key observation and one reset-to-payload trace, not 10-run stability; B06WG's sparse post-memory path does not validate all advertised RAM
```

The capture includes `USB keyboard initialized`, the Transcend device's
512-byte block geometry, the complete bounded `[USBTRACE]` journal, RTL8168
link-up, DHCP and a successful 388848-byte TFTP PXE-NBP. This advances the
USB milestone from electrical release to descriptor/configuration, HID and
mass-storage operation. Later netboot.xyz attempts produced a TLS record
authentication error (`1c0ded02`) and HTTP/no-progress timeouts
(`4c0c6035`/`4c072035`). Those errors do not distinguish NIC/root-port,
chainloaded-UNDI and unvalidated-high-RAM causes.

## 2026-09-07 — B06WG local USB Hiren's reaches a graphical bugcheck

```text
test ID: B06WG-HW-HIRENS-01
image ID: X58PROE-B06WG-AUTO-USB-SEABIOS-20260907
ROM hash / hardware configuration: same as B06WG-HW-USB-01
boot type: local USB boot selected in the already running B06WG SeaBIOS session; complete reset provenance belongs to the preceding boot, not this partial capture
POST trace: capture begins mid-transfer after CH341 USB-UART reset; external loader activity and the following firmware reset-to-CAR-ROMMON are retained
serial log: research/msi/captures/2026-09-07-b06wg-local-usb-hirens.raw, 2593277 bytes, SHA-256 edc8938171612f57bd10115387d435eb36ca244113acfd912e84249b9b5605d4
result: MAJOR PARTIAL PASS — sustained successful EHCI mass-storage reads, external bootloader execution and successful SeaBIOS AHCI reads were followed by an operator-observed graphical Windows PE BSOD and automatic reset; STOP code was not captured
recovery required: automatic reset returned to firmware, which rejected the retained PCIEXBAR state with CODE=0d and entered the CAR recovery ROMMON
notes: no SeaBIOS USB transfer error appears in the retained portion; this proves loader/kernel progress far beyond enumeration, not a completed Windows boot or the cause of the bugcheck
```

The dominant USB pattern is a 65024-byte data phase with normal CBW/CSW
traffic. `AX=1500`, `DL=0/1` `invalid handle_legacy_disk` messages are absent
floppy-type queries and were followed by further successful I/O; they are not
the terminal fault. The first decisive follow-up is graphical capture of the
STOP code. `0x7B` would prioritize AHCI/storage handoff, `0xA5` ACPI, while a
memory-, machine-check- or interrupt-related code would prioritize high-RAM,
APIC and IRQ validation. The complete evidence and non-claims are preserved
in [the B06WG hardware note](../research/msi/b06wg-usb-boot-hw-2026-09-07.md).

## 2026-09-07 — B06WH low-noise SeaBIOS release build

```text
test ID: B06WH-BUILD-01
image ID: X58PROE-B06WH-AUTO-USB-SEABIOS-20260907
build commit: source base a2eb375438c85cd7908140636cbee8407bee8c0d plus the B06WH delta on x58-pro-e/seabios
ROM hash: public base 7b079fe0a962585e85d12e4d72291fb6cc4c18c6e08f845112b3d01ca5f49513; local 4-MiB deterministic 0dc15a23ed536152f024bc080b979be27dbb4cacaa792679c1f65f15a4bf1f16; local W25Q128 b5f894d4a2fc1552c141d656ef4564cc4be5d4f7ae5ca75de638bb4315024c6b
flash chip: intended socketed W25Q128.V..M, 16 MiB; not programmed for this record
board revision: intended MSI X58 Pro-E / MS-7522; exact PCB revision to restamp
CPU: intended Intel Xeon E5645, CPUID 000206c2
CPU stepping: intended stepping 2
microcode revision: embedded fixed-test 0000001f; not executed as B06WH
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole SPD responder 0x54; physical label to restamp
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged test setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: expected B06WG-equivalent path with distinct B06WH identities and unchanged 98..9f automatic USB codes
serial log: none for B06WH; COM1 0x3f8 at 115200 8N1 must be armed before power-on
result: BUILD-TESTED — TWO CLEAN FIXED-EPOCH BUILDS BYTE-IDENTICAL; CURRENT 454/454 COMPLETE AND 17/17 COMBINED B06WG/B06WH TESTS PASS; CBFS, PAYLOAD, LOCAL COMPOSITE AND W25Q128 TOP PLACEMENT PASS; HARDWARE UNTESTED
recovery required: none during construction; socketed known-good recovery chip remains mandatory for first execution
notes: sole functional delta is SeaBIOS DEBUG_LEVEL 8->6; CONFIG_DEBUG_USB_TRACE and every platform/USB/payload feature remain enabled; synchronous serial timing necessarily changes
```

B06WG's local Hiren's capture contained 41,549 level-7
`ehci_send_pipe` lines. B06WH retains the same SeaBIOS revision and USB
journal but lowers the global threshold to level 6. Independent payload
inspection finds controller setup, keyboard, errors and `[USBTRACE]` strings,
while the complete per-transfer format string is absent. The 16-MiB image has
an erased lower 12-MiB prefix and its upper 4 MiB compare exactly with the
deterministic local composite.

After the B06WH integration, the complete B06WG wrapper was also rerun. Both
clean builds completed and reproduced all published B06WG artifact hashes,
including public base `fe02f595...79a5`, deterministic composite
`02fedcc5...1690` and W25Q128 `ba5c6d24...ac7c`. This closes the artifact-level
regression check for the preserved hardware-proven image.

The first B06WH run must repeat display, keyboard and mass-storage operation,
then retry local Hiren's while recording the screen. A repeated BSOD needs its
STOP code before any platform delta is selected. Lower serial volume is not
itself a USB throughput, memory, ACPI or bugcheck fix. See the
[design/test procedure](b06wh-quiet-usb-seabios.md) and
[release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-07 — B06WH reaches graphical Windows PE, then stops with ACPI_BIOS_ERROR 0xA5

```text
test ID: B06WH-HW-02
image ID: X58PROE-B06WH-AUTO-USB-SEABIOS-20260907
build commit: source base a2eb375438c85cd7908140636cbee8407bee8c0d plus the B06WH delta on x58-pro-e/seabios
ROM hash: expected local W25Q128 b5f894d4a2fc1552c141d656ef4564cc4be5d4f7ae5ca75de638bb4315024c6b; no programmer read-back retained
flash chip: operator-reported socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; exact PCB revision not restamped
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: embedded fixed-test 0000001f
DIMM model: BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: sole responding SPD address 0x54; physical label not restamped
GPU: AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged fixed test setup; exact model not restamped
boot type: operator started the active B06WH image; the retained capture begins in late post-memory execution, so electrical-G3/cold provenance is not independently established
POST trace: retained serial covers late RAMINIT, CBMEM, postcar, complete ramstage/platform/PCI/ACPI construction, SeaBIOS payload entry and USB initialization; after the graphical bugcheck the automatic reset reaches bootblock/romstage and the retained-state PCIEXBAR guard
serial log: research/msi/captures/2026-09-07-b06wh-hw-02-usb-winpe-a5.raw, 77929 bytes, SHA-256 f1246d789ac87c022eef48aee92f08d5a35520a8f49e37655564410e519cf3f9
result: MAJOR PARTIAL PASS — complete ramstage and ACPI construction, SeaBIOS, Radeon display, USB HID/MSC and local Windows PE graphical execution were reached; Windows then displayed ACPI_BIOS_ERROR (0xA5) and reset
recovery required: no external recovery; the automatic reset returned to firmware, whose retained PCIEXBAR/platform-state guard stopped at CODE=0d in the CAR ROMMON
notes: the UART capture does not contain the graphical bugcheck text or its four parameters; 0xA5 is an operator-observed screen result, and its exact ACPI subtype is therefore unknown
```

The same trace proves that B06WH completes its quiet-USB objective rather than
failing in the loader or USB transport: SeaBIOS reports the initialized USB
keyboard and the Transcend 128GB mass-storage device before transferring
control to the local medium.  The visible Windows screen proves execution past
the firmware payload, but not a booted operating system or stable high memory.

An audit of the exact ACPI/interrupt contract used by this run identifies
several concrete incompatibility candidates:

- B06WH explicitly leaves PIRQ and SCI routing to the payload
  (`PIRQ_WRITE=0`, `SCI_WRITE=0`, `PAYLOAD_OWNS_ROUTING=1`), but its DSDT has
  no PCI-routing `_PRT` packages for the root bridge or downstream bridges.
- The hardware IOAPIC ID is zero and the emitted MADT describes both the BSP
  Local APIC and the IOAPIC as ID zero.  The MADT also reports
  `IRQ0_OVERRIDE=0`; it contains the SCI IRQ9 override but no legacy
  IRQ0-to-GSI2 override.
- The minimal FADT publishes no GPE0 register block.
- B06WH advertises an i8042/PNP0303 keyboard interface even though the bounded
  firmware probe did not establish a working PS/2 keyboard interface.  USB
  keyboard operation is independently proved and does not validate that
  legacy advertisement.

These are measured or source-audited differences, not a decoded diagnosis of
the Windows stop.  Without the four `0xA5` bugcheck parameters it is not yet
proved which one Windows rejected; missing PCI `_PRT` information is a strong
candidate, while the APIC-ID collision, missing IRQ0 override, absent GPE0
block and false i8042 advertisement must be corrected or isolated
independently.

## 2026-09-07 — B06WI exact-gated vendor-correlated IRQ/ACPI build

```text
test ID: B06WI-BUILD-01
image ID: X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907
build commit: source base a2eb375438c85cd7908140636cbee8407bee8c0d plus the B06WI delta on x58-pro-e/seabios
ROM hash: public base 68ceaabd643aa16ac436b4de33a91b0f87ace3b5b6bd0bab7ffeb7c64a70ee86; local unpatched 8c729dfb55bf3757ee6bee2129cd68fa1b349d2df3538011d5348e5dd7b2be38; local deterministic 4-MiB 132d6987696e4fb9800877611cad25d2e019a3eeabbab8cf2fde06c612572bf6; local W25Q128 3d9e6e03e2ed5a59dcdb95b34e2f0cd82e4becee9aa188b81efe2597374c6b04
flash chip: intended socketed W25Q128.V..M, 16 MiB; not programmed for this record
board revision: intended MSI X58 Pro-E / MS-7522; exact PCB revision to restamp
CPU: intended Intel Xeon E5645, CPUID 000206c2
CPU stepping: intended stepping 2
microcode revision: embedded fixed-test 0000001f; not executed as B06WI
DIMM model: intended BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
DIMM slot: intended sole SPD responder 0x54; physical label to restamp
GPU: intended AMD Radeon HD 5450 1002:68f9 with physical VBIOS
PSU: unchanged fixed test setup; exact model not restamped
boot type: none; source/build/static-artifact record only
POST trace: none; expected new a6/a7/a8 success sequence or terminal a9
serial log: none; expected B06WI-IRQ-ACPI1 PRE/READY or FAIL with MUTATED/ROLLBACK fields; COM1 0x3f8 at 115200 8N1 must be armed before power-on
result: BUILD-TESTED — release artifacts exist and the focused B06WI source-contract suite passed 8/8; HARDWARE UNTESTED
recovery required: none during construction; socketed known-good recovery chip and one-second AC interruption remain mandatory for first execution
notes: the exact 0xA5 subtype is unknown; this image tests a coherent IRQ/ACPI hypothesis and is not claimed to fix Windows, RAM, USB, storage or any BSOD
```

B06WI preserves B06WH through the post-B06VY canonical masked-IOAPIC and
quiet-ACPI states. Before PCI scan it exact-gates LPC/RCBA/OIC, all-disabled
PIRQ, PCI interrupt pins, six IP/IR words and all 24 masked destination-zero
IOAPIC entries. It then changes only D26IP's F2 pin field
`30000321->30000421`, the D31/D29/D28/D27/D26 IR fields to
`0232/0237/3201/3216/3250`, and hardware IOAPIC ID `0->1`. Each write is read
back; a post-write failure attempts selected-field rollback and halts at `a9`.

The matching native tables add direct-GSI root and HD5450/RTL8168 child
`_PRT` packages, MADT IRQ0-to-GSI2 and IOAPIC ID 1, plus FADT GPE0 at
`0520/10` with all enables still zero. B06WI suppresses the unproved
PNP0303/FADT-8042 advertisement. It does not copy the vendor DSDT BIOS
operation region at `0xffffff00`, SMM/SMI services, the dynamic twelve-CPU
MADT, IOAPIC ID 6, PIRQ low nibbles, `INT_LINE`, redirection entries, inactive
D29:F3 policy or any full GPIO/LVL image. See the immutable
[B06WI design and first-test contract](b06wi-vendor-irq-acpi.md).

## 2026-09-07 — B06WI release rebuild reproduces published image

```text
test ID: B06WI-REBUILD-02
image ID: X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus preserved tracked patch and nine additional mainboard files
ROM hash: W25Q128 3d9e6e03e2ed5a59dcdb95b34e2f0cd82e4becee9aa188b81efe2597374c6b04
hardware/configuration: intended fixed target from B06WI-BUILD-01; no hardware access during this task
boot type: none; two clean host-side builds
POST trace: none; B06WI hardware run still pending
serial log: none
result: PASS — two fresh builds byte-identical to each other and the previous B06WI artifact; 462/462 tests, ASL zero errors/warnings, composite/wrapper/top-placement checks passed
recovery required: no
notes: no code delta, flash operation or claim that Windows A5 is fixed; known-good B06WG/B06WH images retain their published hashes
```

The complete new build log is
`builds/experimental/msi-x58-pro-e-b06wi-rebuild-20260907-2315.log`, SHA-256
`202fbd976e4546d6135807cc0fb13a51669eaae4915b3d5551eaff662eb856af`.
The [release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
now records the dirty source delta, copied build scripts/configs, linked debug
symbols and DSDT extracted from the ROM. The separate
[overall assessment](overall-status-2026-09-07.md) distinguishes demonstrated
SATA reads, USB input and loader execution from the outstanding ACPI/OS,
SMP, warm-reset and UEFI milestones.

## 2026-09-07 — B06WI-HW-01 IRQ/ACPI READY and JetFlash boot

```text
test ID: B06WI-HW-01
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived B06WI delta
ROM hash: intended W25Q128 3d9e6e03e2ed5a59dcdb95b34e2f0cd82e4becee9aa188b81efe2597374c6b04; build banner observed, flash readback not repeated
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; exact PCB revision not restamped
CPU: Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: live 0000001f
DIMM model: BLS4G3D1609DS1S00, 4 GiB, 2Rx8
DIMM slot: sole SPD54, physical label not restamped
GPU: AMD HD5450 1002:68f9, physical option ROM
PSU: unchanged, model not restamped
boot type: one-second Shelly AC interruption; fresh COLD_DEFAULT trace
POST trace: serial confirms new B06WI READY/a8 path; no separate POST-card recording
serial log: research/msi/captures/b06wi-hw01-20260907T2136.raw
result: PARTIAL PASS — new hardware IRQ readback, ACPI table construction, SeaBIOS and JetFlash boot-sector handoff; Windows outcome pending
recovery required: USB-UART adapter reset before the run; no flash recovery
notes: no added RAM/QPI tests, no boot-medium writes, no further reset after USB handoff; target left running
```

The 600.030-second raw capture contains 95,259 bytes, SHA256
`d80b43fd2263f2a020be095d3825c352b5f6702b12d6e1f72d4ac00c8146a00e`.
At +359.047 seconds the selector chose the actual JetFlash menu entry 2;
SeaBIOS mapped the USB drive to BIOS drive 0 and entered `0000:7c00`.
No firmware reset appeared during the remaining approximately four minutes.
This does not establish a Windows boot or a resolved A5 error. The
[hardware report](../research/msi/b06wi-jetflash-hw-2026-09-07.md) records
the exact routing/table evidence, initial UART noise, and the selector's
event-text defect without modifying the archived raw evidence.

## 2026-09-07 — B06WI-HW-01 operator screenshot confirms Windows A5 again

```text
test ID: B06WI-HW-01-SCREEN
build commit: same archived B06WI delta as B06WI-HW-01
ROM hash: intended W25Q128 3d9e6e03e2ed5a59dcdb95b34e2f0cd82e4becee9aa188b81efe2597374c6b04; no new flash readback
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; exact PCB revision not restamped
CPU: Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f in the associated serial run
DIMM model: BLS4G3D1609DS1S00, 4 GiB, 2Rx8
DIMM slot: sole SPD54; physical slot label not restamped
GPU: AMD HD5450 1002:68f9, physical option ROM
PSU: unchanged fixed setup; model not restamped
boot type: continuation of B06WI-HW-01 JetFlash attempt; no additional boot is counted
POST trace: associated UART already records IRQ READY/a8, ACPI construction, SeaBIOS and JetFlash boot-sector handoff
serial log: research/msi/captures/b06wi-hw01-20260907T2136.raw; unchanged, screen text is not part of this raw capture
result: MAJOR PARTIAL PASS — new IRQ/ACPI path and local Windows PE graphical execution reached; operator screenshot shows ACPI_BIOS_ERROR (0xA5) again, so Windows boot still fails
recovery required: screenshot announces a restart, but subsequent reset/recovery completion is not established by this observation
notes: screenshot supplied in the conversation after the initial pending report; no standalone image file/hash or four bugcheck parameters available; no new RAM/QPI tests or boot-medium writes
```

This observation closes the earlier pending screen result without rewriting
that historical record or its raw UART evidence. B06WI's routing changes and
matching tables are insufficient to complete this Windows PE attempt. The
identical top-level stop code does not prove an identical ACPI subtype or
object across B06WH and B06WI; the four parameters are still needed for that
distinction.

B06WJ is the next source-only experiment at this observation point: describe
the existing BSP as `CP00`, add coherent LPC PIC/PIT/RTC and low-I/O resources,
and publish the already decoded HPET through native ACPI. The RAM/QPI and
working USB path remain the operator-accepted baseline. No completed WJ
release build, hardware execution or A5 fix is claimed by this entry.

## 2026-09-07 UTC — B06WJ release built after recurring Windows A5

```text
test ID: B06WJ-BUILD-01
image ID: X58PROE-B06WJ-ACPI-PLATFORM-20260907
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived patch/mainboard source
ROM hash: W25Q128 9ad2108df0a88b66384d6833f3c50bf66f2509f2a739baf0c1b05509b476bd06
hardware/configuration: intended fixed E5645/SPD54/HD5450/JetFlash target from B06WI-HW-01; no WJ execution
flash chip: intended socketed W25Q128.V..M, 16 MiB; not programmed here
boot type: none; source/host-build record
POST trace: none; expected inherited a8 and new B06WJ-PLATFORM1 HPET/table serial marker
serial log: none for WJ; predecessor raw remains immutable
result: BUILD PASS — two final clean builds byte-identical, 469 host tests pass, 1076-byte DSDT compiles/disassembles without IASL errors/warnings
recovery required: none during build; preserve known-good socketed chip and one-second AC recovery for hardware test
notes: CP00 UID0, LPC PIC/PIT/RTC, low-I/O root resources and HPET namespace/table added; no new hardware writes, RAM/QPI tests, USB changes, SMM/SMP/sleep claims or A5-fix assertion
```

The [manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) pins
the exact flash artifact, debug symbols and source snapshot. The
[design/test procedure](b06wj-acpi-platform.md) keeps the next hardware run
focused on JetFlash/Windows. B06WI and its original ROM hash are preserved.

## 2026-09-08 local / 2026-09-07 UTC — B06WJ ACPI and JetFlash handoff

```text
test ID: B06WJ-HW-01
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived B06WJ delta
ROM hash: intended W25Q128 9ad2108df0a88b66384d6833f3c50bf66f2509f2a739baf0c1b05509b476bd06; fresh banner observed, no programmer readback
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: live 0000001f
DIMM model: BLS4G3D1609DS1S00, 4 GiB, 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9, physical option ROM
PSU: unchanged fixed configuration; model not restamped
boot type: user-requested one-second Shelly mains interruption; COLD_DEFAULT
POST trace: UART IRQ READY/a8 path; no independent POST-card capture
serial log: research/msi/captures/b06wj-hw01-20260908T0030-boot-snapshot.raw
result: PARTIAL PASS — B06WJ new ACPI marker, SeaBIOS and JetFlash boot-sector execution; operator reports loading, Windows outcome pending
recovery required: target AC cycle only; ConsolePi neither power-cycled nor rebooted
notes: no added RAM/QPI qualification, manual register tests, guard writes or further resets after selection
```

The immutable 2,622,413-byte snapshot has SHA256
`eca57b17e623856bd72c96c4518af21815870f6b3d420047b9c799b00db621f9`.
Its 2,526,979-byte repeated/noisy pre-reset prefix is excluded from hardware
conclusions; the coherent fresh B06WJ trace follows. New runtime telemetry
confirms HPET ID `8086a301`, `CP00/UID0`, LPC PIC/PIT/RTC, low-I/O coverage and
`ACPI: done.` The selector chose actual JetFlash entry 2 at +360.660 seconds;
SeaBIOS subsequently entered `0000:7c00` from that USB drive. The remote
600-second passive capture was still running when this snapshot was saved.
The [run report](../research/msi/b06wj-jetflash-hw-2026-09-08.md) records the
capture limitation and pending screen outcome. No A5 fix or OS boot claimed.

Capture-completion addendum: the helper finished normally at +600.033 seconds.
The final `b06wj-hw01-20260908T0030.raw` and its `.raw.json` metadata are
archived beside the immutable snapshot. Final raw size/hash are identical;
no further UART output or firmware reset was seen before the deadline. The
port was released without disturbing the loading board. Windows outcome
remains pending, not inferred from serial silence.

## 2026-09-08 — B06WJ operator confirms recurring Windows ACPI error

```text
test ID: B06WJ-HW-01-RESULT
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived B06WJ delta
ROM hash: intended 9ad2108df0a88b66384d6833f3c50bf66f2509f2a739baf0c1b05509b476bd06; no new programmer readback
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: associated run 0000001f
DIMM model: BLS4G3D1609DS1S00, 4 GiB, 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: outcome of existing B06WJ-HW-01, not a new boot
POST trace: associated IRQ READY/a8, ACPI done, SeaBIOS and USB boot-sector handoff
serial log: research/msi/captures/b06wj-hw01-20260908T0030.raw; unchanged
result: OS FAIL — operator confirms the same previously identified Windows ACPI error; firmware/loader partial pass remains
recovery required: subsequent target-only diagnostic reset recorded separately
notes: no new screenshot or four bugcheck arguments; identical top-level A5 does not prove identical subtype; RAM/QPI remain accepted baseline, no requalification
```

## 2026-09-08 — B06WJ vendor comparison and RAM-only ACPI correction

```text
test ID: B06WJ-ACPI-RAMLAB-01
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived B06WJ delta; no firmware source edit
ROM hash: 9ad2108df0a88b66384d6833f3c50bf66f2509f2a739baf0c1b05509b476bd06; local artifact rechecked, no new programmer readback
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: associated boot 0000001f
DIMM model: BLS4G3D1609DS1S00, 4 GiB, 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: two target-only one-second AC cycles to establish diagnostic entry; no reset after RAM mutations
POST trace: inherited coreboot/SeaBIOS/iPXE path, then late BIOS-phase GRUB and JetFlash MBR; no independent POST-card capture
serial log: research/msi/captures/b06wj-jetflash-acpi-fixed-20260908-01.raw plus mutation/readback captures linked below
result: PARTIAL PASS — corrected SSDT and native DSDT fully verified in actual RAM, coherent roots/pointers/E820 publication, USB key invitation reached; Windows outcome unconfirmed
recovery required: first GRUB entry was before BIOS disk mapping, so one additional target cycle was used; ConsolePi untouched; RAM corrections disappear on normal firmware restart
notes: grouped descriptor/routing fixes with GRUB table relocation, not a single-variable A/B test; no flash/disk writes or new RAM/QPI qualification
```

The [full report](../research/msi/b06wj-acpi-a5-runtime-lab-2026-09-08.md)
records two fresh read-only reference captures, the actual MSI NVS runtime
patch, deterministic Intel ACPI comparisons, and the target's table and PCI
pin census. The candidate DSDT is 1618 bytes, SHA256
`ebb760c9558dca096b8b1bd21820416c1b04020375afd7512da6d3b421ea8bbf`;
the corrected 113-byte SSDT is
`08522e71ad8df4fe7c301e60989e64c515a9437f0a248dcdeec5be0107a58033`.
All eleven final structures were reconstructed from complete serial readbacks.
The boot capture begins 08:34:58 UTC, is 1695 bytes with SHA256
`af7161b94affffa9edfd8b5840d09d5e13d06e916a4f234a27d001b5c0300d9c`,
and ends after a bounded 180-second passive window. It does not prove that
Windows accepted the new tables. The user was asked for the current display;
no further key, reset or speculative hardware change was issued afterward.

## 2026-09-08 — RAM-ACPI JetFlash attempt also ends in operator-reported A5

```text
test ID: B06WJ-ACPI-RAMLAB-01-RESULT
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived B06WJ delta; unchanged
ROM hash: 9ad2108df0a88b66384d6833f3c50bf66f2509f2a739baf0c1b05509b476bd06; no new programmer readback
flash chip: socketed W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: associated boot 0000001f
DIMM model: BLS4G3D1609DS1S00, 4 GiB, 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: outcome of existing RAMLAB handoff, not a new boot
POST trace: associated coreboot/SeaBIOS/late-GRUB/JetFlash path
serial log: research/msi/captures/b06wj-jetflash-acpi-fixed-20260908-01.raw; later single-space metadata b06wj-jetflash-space-20260908-01.raw.json
result: OS FAIL — operator reports A5 again; the previous pre-handoff RAM-table verification remains valid
recovery required: none performed in response to this report
notes: one space transmitted at 08:49:37 UTC, no RX over 60 seconds; no capture of key acceptance, Windows-selected RSDP or four bugcheck parameters; no RAM/QPI requalification
```

The [updated RAM lab report](../research/msi/b06wj-acpi-a5-runtime-lab-2026-09-08.md)
records the failed OS outcome separately from the valid table readbacks.
Remaining FADT power-button and legacy-VGA resource hypotheses are not yet
tested. A WinPE F8/KD session is the proposed next discriminator, contingent
on an actual debugger endpoint and a separately arranged boot. No firmware,
boot-medium, GPIO or interrupt-controller mutation was made on this report.

## 2026-09-08 — B06WK native ACPI repair release built

```text
test ID: B06WK-BUILD-01
image ID: X58PROE-B06WK-ACPI-REPAIR-20260908
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived dirty delta
ROM hash: bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0
flash chip: intended socketed W25Q128.V..M, 16 MiB; not flashed
board revision: intended MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: intended Xeon E5645, CPUID 000206c2
CPU stepping: 2, inherited inventory
microcode revision: prior 0000001f; no new hardware measurement
DIMM model: BLS4G3D1609DS1S00, 4 GiB, 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9 with physical option ROM
PSU: unchanged; model not restamped
boot type: none; host build only
POST trace: none; inherited diagnostic path retained
serial log: none; expected B06WK-REPAIR1 FADT_FLAGS=00000065 / CTBL_CONSUMER_FLAG=01
result: BUILD PASS — two byte-identical builds, 515 host tests, 7 native acpigen C tests, IASL clean, independent ROM/table audit
recovery required: none; WJ full image unchanged, vendor/socketed recovery retained
notes: no flash, reset, console access or OS boot; local MSI CSI/MINIT path retained; A5 resolution unproved
```

Four functional changes: common CTBL consumer flag correction; WK-gated
44 additional IOH root IRQ routes; two legacy VGA producer windows; fixed
power-button description. No new event-enable/GPIO/IRQ register writes.
Native C regression logs record two consumer failures before the flag fix
and seven passes afterward. Full-image bytes, DSDT checksum/hash, local
composition and the inherited seven-byte wrapper patch were audited.
See the [release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
and [test hypothesis/recovery](b06wk-acpi-repair.md). Next is the offered
Linux live USB, not another RAM/QPI training qualification or automatic reset.

## 2026-09-08 — B06WK SeaBIOS selects Intenso but rejects its boot sector

```text
test ID: B06WK-HW-01
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus WK delta
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no new flash readback
flash chip: socketed W25Q128.V..M, 16 MiB, inherited inventory
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645 / CPUID 206c2, inherited inventory
CPU stepping: 2, inherited inventory
microcode revision: prior 1f; initial CPU phase not captured
DIMM model: BLS4G3D1609DS1S00, 4 GiB 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: operator-started; no agent reset or independent cold/warm confirmation
POST trace: late RAMINIT -> postcar -> ramstage -> ACPI -> SeaBIOS; no separate card trace
serial log: research/msi/captures/b06wk-intenso-boot-20260908-01.raw
result: WK REPAIR1 flags65/consumer01 observed; Intenso key2 selected; SeaBIOS rejected received boot signature and fell back to iPXE
recovery required: none; stopped only own capture process, UART released
notes: no extra RAM/QPI tests, guard writes, target reset, install or storage write; no OS entry
```

The [run report](../research/msi/b06wk-intenso-hw-2026-09-08.md) retains the
89,763-byte serial capture (SHA256 `ae047d7affdc19f8104ca73519fc8ac319e9e8c008b693963099f65636027107`),
exact TX timing and pinned SeaBIOS signature-check interpretation. Intenso
Ultra Line 8.01 is USB disk0 in this attempt; JetFlash was entry3, not selected.
The exact ISO/write method and actual medium bytes need confirmation before
assuming a particular formatting cause. No Windows ACPI outcome follows.

## 2026-09-08 — B06WK Intenso retry reaches ISOLINUX and Linux setup

```text
test ID: B06WK-HW-02
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no flash readback
flash chip: socketed W25Q128.V..M, 16 MiB; inherited inventory
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645; observed CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f observed
DIMM model: BLS4G3D1609DS1S00, 4 GiB 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: one target-only Shelly off/on with local one-second return timer; no electrical G3 measurement
POST trace: c3, automatic vendor-assisted initialization, postcar/ramstage and SeaBIOS; no separate POST-card capture
serial log: research/msi/captures/b06wk-intenso-retry-boot-20260908-02.raw
result: PARTIAL PASS — Intenso boot sector executes, ISOLINUX 6.03 loads bzImage and three initrds; Linux early EDD setup completes; kernel/desktop outcome pending
recovery required: one controlled target AC cycle; UART capture ended normally and target left running
notes: operator-reprepared medium, exact ISO/write method not provided; no medium writes, firmware changes, extra RAM/QPI tests or guard changes
```

The [retry report](../research/msi/b06wk-intenso-retry-hw-2026-09-08.md)
records the actual Intenso-by-name selection at 11:39:01 UTC, repaired WK
ACPI marker, loader progression and interpretation of optional BIOS probes.
The 600-second capture contains 107,574 bytes, SHA256
`c8d4d2699f663aa91e7c6007381d8b81b364f7957f1edba00d057c26d6dd73c8`;
its metadata digest matches the local copy. A display question is pending.
No native kernel serial command line was set, so silence after setup is
not by itself a hang. The previous boot-sector rejection is overcome, but
neither a completed Linux boot nor Windows A5 resolution is claimed.

## 2026-09-08 — B06WK-HW-02 photo confirms Linux kernel execution and stall

```text
test ID: B06WK-HW-02-RESULT
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta; unchanged
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no flash readback
flash chip: socketed W25Q128.V..M, 16 MiB; inherited inventory
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645 / CPUID 000206c2 from associated boot
CPU stepping: 2
microcode revision: 0000001f from associated boot
DIMM model: BLS4G3D1609DS1S00, 4 GiB 2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: result of existing HW-02, not a new boot
POST trace: unchanged associated firmware/payload trace
serial log: research/msi/captures/b06wk-intenso-retry-boot-20260908-02.raw; immutable
result: KERNEL ENTRY/DRIVER INIT PASS; OPERATOR-REPORTED STALL at displayed rtc0 registration, timestamp28.036957; no complete OS session
recovery required: none performed in response; target left unchanged
notes: photo PXL_20260908_134258640.jpg SHA256 5a4e1f529a5958109b059c62ff65414819b7f666f44eb7d196145e3e0ed6bbd6; Linux6.8.0-31-generic; no panic/stack trace visible
```

The [appended display/source analysis](../research/msi/b06wk-intenso-retry-hw-2026-09-08.md)
distinguishes Linux kernel execution from the prior setup-only serial proof.
RTC/timekeeping is a specific candidate because the upstream RTC class may
read/set system time immediately after the observed message. IRQ8 delivery
and complete ACPI operation remain unproved; neither PS/2 absence nor the
last printed RTC line alone diagnoses the stall. Next: a kernel-owned COM1
console plus `initcall_debug`, preserving the current platform baseline.

## 2026-09-08 — B06WK Linux diagnostic retry blocked by UHCI reset state

```text
test IDs: B06WK-HW-03, B06WK-HW-04, B06WK-HW-05
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta; unchanged
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash chip: socketed W25Q128.V..M, 16 MiB; inherited inventory
board revision: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: Xeon E5645, observed CPUID000206c2
CPU stepping: 2
microcode revision: observed0000001f
DIMM model: BLS4G3D1609DS1S00, 4 GiB2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot types: target-only timed mains interruptions1s,1s,2s; no independent G3 measurement
POST trace: each reaches ramstage then USB-AUTO exact-late-preflight stop, before payload; no separate card capture
serial logs: research/msi/captures/b06wk-intenso-linux-diag-20260908-03.raw, -04.raw, -05.raw
result: BLOCKED — five UHCI command/status tuples0018/0024 violate required0000/0020; no SeaBIOS, no Intenso selection, no Linux diagnostic args executed
recovery required: two bounded retries reproduced issue; no fourth reset; full physical power-off coordination requested
notes: zero UART TX in all three metadata records; no USB-AUTO GPIO write, no guard bypass, no build/flash/storage write; target remains powered at guard halt
```

The [complete comparison and preparation](../research/msi/b06wk-linux-initcall-hw-2026-09-08.md)
records immutable raw/metadata digests, native source checks, private FIFO
control design and the actual absence of Linux diagnostic execution.
HW03/04/05 raw SHA256 respectively:
`f75a4e1f8746487155ef8d3f08aa3fbfb8e2acad3af604cc79408ba333da2adf`,
`6f034fa691ad834ceddc6e3728cb7a1bfe631aba37b0a3f3b293a2710ed60f51`,
`c618b5f3040f3beb4f740a7662a8562a6208a9a6e3ac5053b7dbb50121a74efd`.
All sizes/digests match the remote metadata; UART locks were released on
normal capture closure. Existing Linux kernel-entry proof from HW02 remains
valid, but its RTC stall and Windows A5 cause remain unresolved. No inference
that pre-release OCA caused this command/status gate failure is made.

## 2026-09-08 — B06WK-HW-06 operator cold start restores USB; editor race

```text
test ID: B06WK-HW-06
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta; unchanged
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash chip: socketed W25Q128.V..M,16MiB; inherited inventory
board revision: MSI X58 Pro-E/MS-7522; PCB revision not restamped
CPU: Xeon E5645, observed CPUID000206c2
CPU stepping: 2
microcode revision: observed0000001f
DIMM model: BLS4G3D1609DS1S00,4GiB2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: operator cold start; no agent reset; electrical discharge duration unmeasured
POST trace: several early reset prefixes, final completed vendor-assisted init/ramstage/SeaBIOS; no independent card capture
serial log: research/msi/captures/b06wk-intenso-linux-diag-20260908-06.raw
result: USB GATE RECOVERY PASS; Intenso/ISOLINUX loads original kernel/initrds; diagnostic edit unperformed due stale helper state after loader transition
recovery required: operator cold start after HW03–05; no further agent reset
notes:36 TX events,32 mistakenly after loader; no Enter; no successful edited readback or native Linux serial output; display follow-up pending
```

The [complete HW06 record](../research/msi/b06wk-linux-initcall-hw-2026-09-08.md)
preserves the exact original command, script failure, hypotheses and pending
diagnostic plan. Raw109283 bytes SHA256
`9f77fc34aa951c6488b233e0c840ede57de516516fdc88e2ee26f19a883c484e`,
metadata SHA256
`da5c405d06f72c827f5227f2658b32686a83e0eda1c524cb1725d30c5fd4f272`;
both verified after normal closure15:58:18 UTC. No firmware/register fix,
flash operation, extra RAM/QPI qualification or completed OS boot is claimed.

## 2026-09-08 — B06WK-HW-07 native Linux serial reaches initramfs userspace

```text
test ID: B06WK-HW-07
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta; unchanged
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash chip: socketed W25Q128.V..M,16MiB; inherited inventory
board revision: MSI X58 Pro-E/MS-7522; PCB revision not restamped
CPU: Xeon E5645, observed CPUID000206c2
CPU stepping: 2
microcode revision: observed0000001f
DIMM model: BLS4G3D1609DS1S00,4GiB2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: one requested target-only Shelly2-second interruption near16:08:02UTC; no independent G3 measurement
POST trace: fresh WK/c3, normal vendor-assisted initialization, USB gate pass, SeaBIOS
serial log: research/msi/captures/b06wk-linux-user-serial-20260908-07.raw; final archive pending capture closure
result: NATIVE COM1/INITCALL DIAGNOSTICS PASS; RTC init returns0; /init/eudev and SATA/USB/HID/NIC drivers reached; desktop unconfirmed
recovery required: user-requested reset from previous Linux run; no additional reset
notes: corrected menu-only helper sends soleESC at16:13:51.984531UTC; user selectsIntenso/edits/boots; no agent TX afterESC; no firmware/register/storage modification
```

The [complete HW07 record](../research/msi/b06wk-linux-initcall-hw-2026-09-08.md)
preserves the kernel-confirmed command line and milestone timestamps. At
107.050646s Linux runs `/init`; eudev starts187.917800s, both USB sticks and
native RTL8168 register, and ISO9660 reads occur. This is actual initramfs
userspace progress, not merely a loader banner, but no complete live session
is yet confirmed. The earlier apparent RTC boundary is passed.

At479.860093s the Radeon driver cannot load `radeon/CEDAR_pfp.bin` (-2),
aborting GPU probe after removing the VGA console; other modules continue
through613.836352s. The missing OS-driver firmware is distinct from its
recognized ATOM VBIOS. Slowness, an unclassified MCE notification and the
source-supported uncovered high-RAM MTRR interval remain open; high RAM is
actually allocated by Linux. No Windows A5 retest or memory/cache fix occurs.
The live capture remains passive. Snapshot04 SHA256:
`1ab196357c0f7e2f18bb2e40a8463a4bdd2fd1a16823d488c2a42347e559bf4a`.

HW07 closure update: the final drain proves further progress after the long
quiet interval: `kvm_x86_init` returns0 at964.387633s. Capture closes normally
16:32:31.184562UTC after1483.809s, without changing or resetting the board.
The full raw622646bytes has SHA256
`b08f7670ba9f8cc6e0b586cd8c22bdd59dd734518ba1ac43c5137efe214e81a4`;
metadata SHA256
`40c31209585dc4ec5aacce151f15e0dcf6ed46735f7c17d9f6b096a20945b5f9`.
Both are archived locally, raw size/hash verified against metadata;
`error:null`, soleUARTTXESC, normal UART lock release. Desktop/root login
remains unconfirmed, not disproven by silence. No further action is taken
while waiting for the operator's display observation.

## 2026-09-08 — B06WK-HW-08 Memtest request blocked before SeaBIOS

```text
test ID: B06WK-HW-08
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta; unchanged
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash chip: socketed W25Q128.V..M,16MiB; inherited inventory
board revision: MSI X58 Pro-E/MS-7522; PCB revision not restamped
CPU: E5649@2.53GHz per actual HW07 Linux brand string; earlier targetE5645 prose was mistaken
CPU stepping: fresh CPUID206c2,step2
microcode revision: fresh0000001f
DIMM model: fresh SPD54 BLS4G3D1609DS1S00,4GiB2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: one user-requested target-only2-second Shelly interruption near18:26:47UTC; no measured G3
POST trace: freshWK/c3,CSI/MINIT return,postmem/ramstage,terminalUSB exact-late-preflight; no separate card trace
serial log: research/msi/captures/b06wk-intenso-memtest-20260908-08.raw
result: BLOCKED BEFORE PAYLOAD; fiveUHCI0018/0024 vs expected0000/0020; no SeaBIOS/Intenso/Memtest result
recovery required: fuller cold recovery; no further reset or guard bypass performed
notes: zeroUARTTX; capture closed18:33:43UTC,target left powered at halt; old repeated-text prefix excluded from new-boot conclusions
```

The [full record](../research/msi/b06wk-memtest-hw-2026-09-08.md) preserves
reset provenance,25/25 helper tests,corrected CPU inventory and raw/metadata
digests. Full raw4129633bytes SHA256
`be2706932d951b0ec65c682480516b4278f9ce264c3fd195eb22cffc355ba9b5`;
metadata SHA256
`a300dd04fe0614bec6edfd8c3ae41c1cf1deaae293bd24e33906a2c8808471c9`.
The first4069102bytes precede freshWK. No conclusion about the Memtest
medium's contents, bootability or RAM integrity follows from this run.

## 2026-09-08 — B06WK-HW-09 operator cold start reaches Intenso GRUB

```text
test ID: B06WK-HW-09
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WK delta; unchanged
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no programmer readback
flash chip: inherited socketed W25Q128.V..M,16MiB
board revision: MSI X58 Pro-E/MS-7522; PCB revision not restamped
CPU: E5649@2.53GHz from actual HW07 brand string; not freshly identified before attachment
CPU stepping: inherited206c2,step2
microcode revision: inherited1f
DIMM model: inherited BLS4G3D1609DS1S00,4GiB2Rx8
DIMM slot: sole SPD54; physical label not restamped
GPU: Radeon HD5450 1002:68f9
PSU: unchanged; model not restamped
boot type: operator-reported cold start; no agent reset/Shelly action; off duration unmeasured
POST trace: capture starts duringpostmem,earlyreset/CSI/MINIT missed; freshWKramstage,USBadmission,SeaBIOS,IntensoGRUB
serial log: research/msi/captures/b06wk-intenso-memtest-20260908-09.raw
result: USB/PAYLOAD RECOVERY AND INTENSO BOOTSECTOR/GRUB ENTRY PASS; no observed Memtest version/pass/errors
recovery required: operator cold start afterHW08; none further
notes: actual mediumIntensoAluLine5.00,7864320x512bytes; onlyESC+2 transmitted; no media/firmware edits; boardlefton
```

[Full HW09 evidence](../research/msi/b06wk-memtest-hw-2026-09-08.md) records
the initialLF-heavy stream separately from actual progress. Capture
18:44:21–18:51:24UTC; fresh identity permitsESC18:48:43.079883 and Intenso2
18:48:43.189182. Last serial output is `Welcome to GRUB!`; operator display
feedback is needed to establish Memtest execution/results.

Final raw878352bytes SHA256
`6b5bbc136abc63c6e0efb1220d879ce89671b5242d041d54f73aae1da6e63e01`;
metadata SHA256
`918e0bd740dadaaf9d49ae097c1d615c875677fadd757a11dae5fbebf5bdf933`.
Both archived; raw size/hash match metadata, error=null,normal UART release.

## 2026-09-08 — HW09 operator feedback and offline HW07 latency analysis

The operator confirms Memtest did not run after the HW09 Intenso/GRUB
handoff. This is not a memory-test failure or a RAM error count. No new
hardware session, reset, register write, build or media edit took place.

The [detailed analysis](../research/msi/b06wk-linux-latency-root-cause-analysis-2026-09-08.md)
compares the immutable HW07 trace, archived WK configuration, current
matching handoff code and vendor-reference Linux log. Key findings:

- 11 unique PMU/perf latency warnings, not 22 independent events; the
  photographed hrtimer warning belongs to HW06, not HW07.
- Bus-ff conflicts and DMAR-IR erratum text also occur on vendor firmware;
  all eight USB IRQ assignments match the vendor Linux capture.
- Postcar leaves usable 4–5-GiB RAM default UC. The entry gate verifies
  that MTRR policy; Linux actually allocates high RAM. Normal Linux cleanup
  and trimming do not repair this WB/UC/WP configuration. Late runtime
  MTRR readback and a controlled causal comparison remain outstanding.
- Radeon fails to obtain OS-side CEDAR firmware; an unclassified MCE is
  separately unresolved. Serial progress continues to kernel time964s.
- Duplicate serial consoles add overhead but byte volume alone does not
  account for the long late pauses. No complete OS session is asserted.

This is an analysis-only update, not an additional passing hardware test.

## 2026-09-08 — Operator iPXE photo proves Memtest5.01 execution with errors

Separate from HW09's USB/GRUB outcome, the operator reports an iPXE netboot
and supplies `PXL_20260908_201023349.jpg`, SHA256
`7dc5dac730ea6dcb7b7ba2113eaf8abdad42b5884b0e67e06bce4a786a7b9ccd`.
The photo shows E5649/one CPU/4094M, StateRunning, elapsed1:45, zero completed
passes and23 errors. Ten visible own-address mismatch rows fall within
0xbffff000–0xbfffffff. Current header4–5GiB is not the location of those
historical errors. Intended firmware remains WK; actual boot type, ROM
readback, tester binary and selected map are not captured for this run.

[Detailed observation](../research/msi/b06wk-memtest-hw-2026-09-08.md)
records the address/value tuples and official5.01 source: it prefers LBIO
RAM over later BIOS E820 reservations. HW07's SeaBIOS allocation leaves
precisely the final12KiB reserved, including that page. A test of live
firmware/USB memory is a strong specific hypothesis, not a proven writer
or definitive exclusion of hardware faults. This is the first newly
provided Memtest-execution evidence, not a stability pass. No agent reset,
UART action, hardware write, firmware build or medium edit occurred.

## 2026-09-08 — B06WK-HW10 different DIMM blocks requested SeaBIOS menu

The operator requests the ESC menu and reports a different4-GiB module.
Passive attachment finds CAR ROMMON; `id` confirmsWK. Read-only `vinfo`,
`spd`, `resetcause` and `vinputs` show no CSI/MINIT attempts in this runtime,
validCMOS0E=2c and a respondingSPD54 base block with validCRCbea5. Its
header92/1Rx8/4-Gbit-device geometry differs from the exact oldheader93/
2Rx8/2-Gbit profile. ExplicitSPD query returnsb8/targetFAIL. Original
automatic fallback line was missed; do not invent that earlier stop reason.

[Full HW10 record](../research/msi/b06wk-new-dimm-menu-hw-2026-09-08.md)
preserves inventory limits, command transcript digests and interpretation.
No reset, guard clear, memory training, payload attempt or build occurs.
The board remains in CAR ROMMON and the ESC menu has not been reached.
Supporting a different geometry requires separate implementation, not
equating every4-GiB DIMM with the current hash-pinned configuration.

## 2026-09-08 — HW10 follow-up: forced menu path investigated

The operator explicitly asks to force the menu with the changed DIMM.
Fresh `help`, `vinfo` and argument-less `vprep` capture proves the active
CAR monitor rejects manual vendor operations before execution. No unlock,
reset or write is issued; CSI/MINIT attempted flags remain zero. Source
matching the archived WK release confirms no CAR resume/call operation,
and downstream old-SPD/DOD/two-rank contracts would also reject the changed
one-rank module. This is not a newly attempted or successful payload boot.

The [HW10 follow-up record](../research/msi/b06wk-new-dimm-menu-hw-2026-09-08.md)
contains the exact rejection and limitations. Transcript
`research/msi/captures/b06wk-new-dimm-force-feasibility-20260908-11.raw`, SHA256
`e9396d35719045a274742099653f6809e15f13663613a9385c3237c6b4a85724`, is archived
unchanged. Board remains in CAR ROMMON; UART released. New-profile firmware
or the previously accepted DIMM is required for a supported continuation.

## 2026-09-08 — Replacement DIMM: old profile, different serial confirmed

The operator asks for another SPD serial comparison and confirms fitting an
identical known-good replacement for Memtest. Two new complete SPD command
responses now show BLS4G3D1609DS1S00.,4GiB2Rx8, unlike the intervening HW10
one-rank module. Comparing all256bytes with HW07 finds only byte125 changed
f3→fb: old serial01020304, current01020305. Remaining255bytes are identical.
Full FNV consequently changes fb66b530→3f5e3f88; hashes recomputed locally.

The [recheck record](../research/msi/b06wk-new-dimm-menu-hw-2026-09-08.md)
archives both explicit reads and excludes an initial repeated UART stream
from fresh-state claims. Model/timing/topology checks now pass; the extra
automatic full-SPD-digest contract still rejects this serial-only difference.
CSI/MINIT attempted0; no reset, override, firmware/EEPROM modification or
Memtest execution occurred. The board is left in responding CAR ROMMON with
UART released. Serial-independent admission is the relevant software change
for this module; the earlier one-rank-policy issue no longer applies to it.

## 2026-09-08 — Serial-independent SPD fix implemented, image build deferred

The operator explicitly requests the code change and defers a new image.
Implemented default-off `X58_PRO_E_SPD_SERIAL_INDEPENDENT`, enabled in the
working WK-derived configuration for the pending successor. The real256-byte
hash remains telemetry/manual confirmation; a separate profile fingerprint
substitutes zero only for serial bytes122–125. Every other byte and the
existing CRC/read/model/geometry/timing/topology gate remains enforced.

All four automatic/post-MINIT gates use the shared admission rule. Both
handoff producers preserve raw identity and the profile hash; the consumer
requires the same profile through a new optional v10/164-byte record. Both
cached hashes are cleared on another read or vendor-state invalidation.
Legacy contracts v1–v9 remain unchanged with the option disabled. Applicable
diagnostic messages identify the new v10 contract without removing ROMMON.

[Implementation and next-image checklist](spd-compatibility.md) records the
hypothesis, evidence, exact scope, failure/recovery expectations and future
test configuration. Verification: all 567 host tests pass, including 9 new
SPD tests, actual C helper/consumer execution, 1,024 accepted serial-byte
variants and 2,016 rejected single-bit nonserial mutations. `git diff --check`
passes. No firmware, payload or release image was built; no SSH/UART,
reset, flash or SPD EEPROM operation occurred in this implementation turn.

The WK16-MiB SHA256 remains
`bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0`;
the4-MiB base ROM and archived effective configuration also retain their
pre-change hashes. This is a source-only pending change, not a new hardware
success or a Memtest pass. The next requested image must receive a fresh
build identity and carry the new option explicitly.
