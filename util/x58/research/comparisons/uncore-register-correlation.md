> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Nehalem/X58 Uncore register correlation

Date: 2026-08-04.  Status: **verified-static** where instruction addresses and
literal arguments are stated; register meanings are **documented** only when
the Intel source below names them.  This file preserves the initial static
analysis and negative local-host probe.  A subsequent target session obtained
the complete live values and proved that the running `V8.14B8` `MINITDLL`
`.text` is byte-identical to the analyzed `8F0` code; see
[MSI X58 Pro-E live Uncore and SPD correlation](../msi/live-uncore-2026-08-04.md).

## Execution result and boundary

The read-only capture was executed as:

```bash
python3 scripts/dump_nehalem_uncore.py --require-target
```

It returned status 2 with empty `profiles` and `devices`.  The analysis host is
an AMD Ryzen/X570 system, so this is the expected negative result; it is not an
MSI X58 Pro-E dump.  The tool did not scan hidden buses or perform a write.
The later remote target capture does not change this original local result.

## Pinned public register sources

| Source | Local verification | Use |
|---|---|---|
| [Intel Xeon 5500 Series Datasheet, Volume 2, document 321322](https://www.intel.de/content/dam/www/public/us/en/documents/datasheets/xeon-5500-vol-2-datasheet.pdf) | 473,072 bytes; SHA-256 `3e0691020033944375986aff6159a18ce9d14f4b0338b4256d3f03eb159e4342` | CPU Uncore BDFs, IMC, channel, rank, SAD/TAD, and CPU-side QPI register names and fields |
| [Intel Xeon 5600 Series Datasheet, Volume 2 supplement, document 323370](https://www.intel.ie/content/dam/www/public/us/en/documents/datasheets/xeon-5600-vol-2-datasheet.pdf) | 713,600 bytes; SHA-256 `3a0748c6617ddee45a5faf0661d81db59357940ddeb0258054f8fc14d86c1cb8` | Westmere-EP additions and explicit dependency on the 5500-series base register document |
| [Intel X58 Express Chipset Datasheet, document 320838](https://www.intel.de/content/dam/doc/datasheet/x58-express-chipset-datasheet.pdf) | 7,427,268 bytes; SHA-256 `8ba958f3a77685577bed0a66563adb97248babfad87aa243b109070cced1d2ff` | IOH topology and the separate IOH-side QPI register map |
| [Linux `i7core_edac.c`](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/edac/i7core_edac.c) | retrieved upstream file SHA-256 `459a2dbeae81c35bbd9d9aba9eb95aa80a438d9a61e57572a96b67007acd75bc` | Bloomfield/Gainestown and Gulftown/Westmere-EP device IDs and Linux enumeration behavior |

Document 321322 describes Gainestown/Xeon 5500 rather than the MSI desktop
board.  Its BDF and named-register layout agrees with the Bloomfield IDs in
Linux and with accesses recovered from MSI `MINITDLL`; this agreement makes it
a strong layout reference, but server-only RAS features must remain optional.

## CPU Uncore configuration layout

The processor exposes these functions on a high PCI bus selected from the
configured maximum bus number and socket number.  The bus is therefore a
runtime value; the device/function layout is stable.

| Device.function | Published block | Representative offsets |
|---|---|---|
| `00.0` | generic noncore | `0x88` memory-lock status, `0x90` MC configuration, `0xc0` UCLK ratio |
| `00.1` | system address decoder (SAD) | `0x50` PCIEXBAR, `0x80..0x9c` DRAM rules, `0xc0..0xdc` interleave lists |
| `02.0` / `02.1` | CPU-side QPI link 0 / PHY 0 | link status `02.0:0x50`, PHY-init status `02.1:0x80` |
| `02.4` / `02.5` | optional second CPU QPI link / PHY | same layout for a second-link processor |
| `03.0` | IMC common | `0x48` control, `0x4c` status, `0x5c` reset control, `0x60` channel mapper, `0x64` maximum DIMM organization |
| `03.1` | target address decoder (TAD) | `0x80..0x9c` DRAM rules, `0xc0..0xdc` interleave lists |
| `03.2` | optional RAS | scrub/ECC control and counters |
| `03.4` | IMC test/PHY support | clock ratio, PHY/test pattern controls, plus vendor-used unpublished offsets |
| `04.0..3` | memory channel 0 | control, address, rank, thermal |
| `05.0..3` | memory channel 1 | same layout |
| `06.0..3` | memory channel 2 | same layout |

The channel-control functions contain the most useful post-boot correlation
points:

| Offset | Published register | Why capture it |
|---:|---|---|
| `0x50` | `MC_CHANNEL_DIMM_RESET_CMD` | DIMM reset/CKE sequencing |
| `0x54` | `MC_CHANNEL_DIMM_INIT_CMD` | training command, rank, masks, RCOMP/ZQCL, CKE |
| `0x58` | `MC_CHANNEL_DIMM_INIT_PARAMS` | rank topology and training time constants |
| `0x5c` | `MC_CHANNEL_DIMM_INIT_STATUS` | training completion/pass fields and FSM state |
| `0x60` | `MC_CHANNEL_DDR3CMD` | MRS and direct DDR commands before `INIT_DONE` |
| `0x70/0x74` | MRS values | MR0/MR1/MR2 policy |
| `0x7c` | rank-present bitmap | populated logical ranks |
| `0x80..0xe8` | timing/ODT/round-trip/background fields | trained or policy-derived channel state |

CPU-side QPI and IOH-side QPI must not be conflated.  CPU link status is at
CPU Uncore `02.0:0x50`.  The X58 IOH exposes its link layer at IOH devices 16
and 17; for port 0, link control/status are `10.0:0xc4` and `10.0:0xc8`.

## MSI `MINITDLL` access mechanism

The deterministic raw `8F0` PE region is 100,096 bytes, SHA-256
`54fb7d14c1d88c0a42ea0e7b51511735983686b79fdf9db64e5ace2fc651264a`,
preferred base `0xfffc1dc0`.  Its meaningful 94,749-byte `.text` SHA-256 is
`c93135af56b85c28bb6f6da6589a2f5a7a35c6a79407b21745d369bc1aa292f4`.
The analyzer pins this section hash so PE-header normalization cannot change
code identity.  It contains cdecl MMCONFIG helpers:

| Address | Operation |
|---:|---|
| `0xfffc47b6` / `0xfffc47e2` / `0xfffc480f` | 8/16/32-bit read |
| `0xfffc486e` / `0xfffc489f` / `0xfffc48d2` | 8/16/32-bit write |

Each helper computes:

```text
PCIEXBAR | (bus << 20) | (device << 15) | (function << 12) | offset
```

At `0xfffc851e..0xfffc8586`, the DLL first uses CF8/CFC directly to write
`0x80ff0150` and `0x80fe0150`, which decode as buses `ff` and `fe`, device 0,
function 1, offset `0x50` (`SAD_PCIEXBAR`).  It writes the selected PCIEXBAR
base with enable bit 0 set, then performs normal Uncore accesses through the
MMCONFIG helpers.  This explains why a raw search for many `0xcf8` references
under-counts the memory-init code.

`scripts/analyze_minit_pci.py` recovers literal call arguments without
assigning values to dynamic operands:

```bash
python3 scripts/analyze_minit_pci.py \
  blobs-local/msi-x58-pro-e/extracted/MINITDLL-region.bin \
  --only-decoded-uncore
```

For the pinned image it found 450 helper calls in total.  Device, function,
and offset were all literal and recognized as CPU Uncore at 118 call sites;
85 of those have either a published name or an explicitly neutral
`MC_TEST_OBSERVED_*` label.  The remaining calls use runtime channel/function
variables and are deliberately retained as unresolved.

## High-confidence static sequences

### DIMM reset and initialization

The sequence beginning at `0xfffc5f0b` has a runtime channel number and uses
device `4 + channel`, function 0:

1. write `0x00000200` to channel offset `0x54`
   (`MC_CHANNEL_DIMM_INIT_CMD.IGNORE_RX`);
2. write byte `1` to `03.0:0x5c`
   (`MC_RESET_CONTROL.BIOS_RESET_ENABLE`);
3. read/modify/write channel offset `0x58`
   (`MC_CHANNEL_DIMM_INIT_PARAMS`);
4. write the per-channel DIMM-reset command at channel offset `0x50`.

In the later routine at `0xfffd82bd`, constant propagation establishes
`EBX == 0`.  The write at `0xfffd8594` is therefore `03.0:0x60 = 0x00024489`.
The published `MC_CHANNEL_MAPPER` fields decode that value as identity mapping
for read and write: logical channels 0, 1, and 2 map to physical channels 0,
1, and 2 respectively.

The loop starting at `0xfffd8646` walks channel devices 4 through 6.  For each
present channel it writes `4` then `0` to offset `0x50` (assert and release
`BLOCK_CKE`), followed by `0x00020200` at offset `0x54`
(`ASSERT_CKE | IGNORE_RX`).  This is a direct code-to-datasheet match for the
DIMM reset/CKE handoff.

### IMC mode and completion-related state

At `0xfffc67ee..0xfffc6807`, the code reads `03.0:0x48`, sets bit 6, and writes
it back.  Document 321322 names this bit `MC_CONTROL.DIVBY3EN`, used with
three- or six-way interleaving.  In the later routine, MSI builds the active
channel bits at positions 8..10, clears bit 6, conditionally sets ECC bit 1,
and writes `MC_CONTROL` back.  The exact point where `INIT_DONE` bit 7 is set
still needs a complete control-flow trace before turning this into an init
sequence.

### Published and unpublished test/PHY space

Direct literal accesses strongly cluster in `03.4`: the extractor finds 101
resolved call sites there.  Published registers include `0x6c`
`MC_TEST_PH_CTR`, `0xa8` `MC_TEST_PAT_GCTR`, `0xb0` `MC_TEST_PAT_BA`, and
`0xbc` `MC_TEST_PAT_IS`.

More importantly, the vendor code also uses `03.4:0x5c`, `0xac`,
`0xc4..0xcc`, `0xf8`, and `0xfc`, which the public 321322 map leaves unnamed.
Ten inspected loops repeatedly read `03.4:0xf8` and continue while bit 30 is
set.  This establishes an observed busy/completion relationship, not the
field's architectural name.  These loops have no visible software timeout;
that vendor behavior must not be copied into coreboot without a bounded
timeout and diagnostic POST code.

The dump tool labels these offsets `MC_TEST_OBSERVED_*` so cold/warm and DIMM
configuration diffs can retain them without inventing register semantics.

## Intel PEIM correlation

Intel GUID `63C0690C-5D9E-4EE3-840E-FAE0E76E291A` is named
`UnCoreInitPlatform` in the S55xx firmware.  Its PCI service calls encode an
address as:

```text
(bus << 24) | (device << 16) | (function << 8) | offset
```

Examples recovered from S55xx R0033 include accesses to `03.0:0x50` and
`03.0:0x54`, plus a six-dword walk of `03.2:0x80..0x94`, exactly the published
correctable-ECC counters.  The Intel desktop PEIM directly forms
`03.4:0x5c`.  These are semantic anchors for the same device/function layout,
not proof of an interchangeable PEIM ABI.

The already verified exact-code matches remain relevant: MSI `MINITDLL`
shares 888 bytes with both S55xx `UnCoreInitPlatform` versions and 404 bytes
with each Intel desktop version.  The register correlations above strengthen
the common-source interpretation because they agree independently with the
published BDF layout.

## Next target capture

The highest-value next artifact is one 256-byte cold-boot snapshot from the
actual MSI board, followed by a warm-reset snapshot with identical CPU, DIMM,
slot, and firmware settings.  The first comparison should prioritize:

1. `03.0:0x48..0x64` for active channels, reset, mapping, and max geometry;
2. `04.0/05.0/06.0:0x50..0xe8` for reset, training, rank, timing, and ODT;
3. `03.4:0x5c`, `0xac`, `0xc4..0xcc`, `0xf8`, and `0xfc` as vendor-observed
   PHY/test state;
4. CPU `02.0:0x50` and `02.1:0x80` for QPI link/PHY status;
5. SAD/TAD rules and RIR/SAG tables for the final memory map.

Post-boot snapshots cannot reveal the order of write-only commands and do not
replace cold-boot tracing, but they can validate static register identities,
separate stable policy from volatile status, and tell us which hidden test
fields deserve instrumentation first.
