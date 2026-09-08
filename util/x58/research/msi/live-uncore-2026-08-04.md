> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI X58 Pro-E live Uncore and SPD correlation

Date: 2026-08-04.  Evidence labels used below:

- **verified-hardware**: read from the running target;
- **verified-static**: observed directly in the matching firmware code;
- **documented**: field meaning stated in an Intel datasheet;
- **inference**: consistent with the observations, but not yet isolated by a
  controlled hardware change.

No memory-controller, QPI, SPD-data, or flash-content write was performed.
One explicitly authorized invasive experiment temporarily cleared four
documented IOH device-hide bits; the exact original value was restored and
verified.

## Target identity

| Item | Observation |
|---|---|
| Mainboard | MSI X58 Pro-E / MS-7522 revision 3.0 |
| Vendor firmware | AMI `V8.14B8`, release date 2012-11-09, DMI BIOS revision 8.15 |
| CPU | Xeon E5645, family 6 model 44 stepping 2 |
| Microcode | `0x13` |
| IOH / southbridge | `8086:3405` X58/5520 IOH; `8086:3a16` ICH10R |
| Flash | Winbond W25Q32.V, 4 MiB, socketed according to vendor DMI |
| Installed memory | three 4-GiB Corsair `CMX8GX3M2A1333C9` UDIMMs, 12 GiB total |

The CPU Uncore is enumerated on PCI bus `ff`.  All required Westmere-EP
functions were captured without an error.

## Immutable captures

| Artifact | Purpose | SHA-256 |
|---|---|---|
| [`running-uncore.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | first 256-byte baseline | `a56a0f12e923b8337cdfcbbf86bdcbd3d456cb225f6cf3b9a2b9373dbd61cac2` |
| [`running-uncore-02.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | repeat baseline | `a56a0f12e923b8337cdfcbbf86bdcbd3d456cb225f6cf3b9a2b9373dbd61cac2` |
| [`running-uncore-03.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | same hardware state with expanded IOH names | `d56977d62ba3e823621ba0b8616ca6c6eff8036220ca8aa591fd2b0bfa4e8aa1` |
| [`running-uncore-4k.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | explicit extended-config read | `d39d79eb707fdcb79e5f267ebd8b36f51d7c940adc2d5a964c102427487f3bd3` |
| [`hidden-qpi-probe.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | temporary IOH Device 16/17 unhide | `7a939c49b0c87e470879dc226b1bf3c032980e2e89bf367326978d38a6ccd836` |
| [`spd-smbus-decoded.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | raw and decoded SPD bytes | raw SPD hash given below |

The first two JSON files are byte-identical.  Comparing their raw PCI
configuration bytes with the third capture also produces no changed dword.
The 4-KiB request returned 4096 bytes only for IOH `00:00.0` and `00:14.0`;
the 24 CPU-Uncore functions expose 256 bytes each.

## SPD and channel population

SMBus addresses `0x50`, `0x52`, and `0x54` contain byte-identical 256-byte
SPDs with SHA-256
`3d7d33481200d79ed5ec6467ac899df1196d984751bc8d8ae6755911e755bfd6`.
Addresses `0x51`, `0x53`, and `0x55..0x57` NACK.

The SPD CRC is valid (`stored = calculated = 0xe5fc`).  Each populated module
is described as:

```text
DDR3 UDIMM, 4096 MiB
2 ranks, x8 SDRAM devices, 64-bit data width, no ECC-width extension
2-Gbit SDRAM, 8 banks, 15 row bits, 10 column bits
tCKmin 1.5 ns / DDR3-1333 maximum
CAS 6, 7, 8, 9
tAA/tRCD/tRP 13.5 ns; tRAS 36 ns; tRFC 160 ns
```

DMI locators 0, 2, and 4 are populated.  The natural mapping is therefore
`0x50/0x52/0x54` to DIMM0 of channels 0/1/2 respectively, but this last
electrical mapping remains an **inference** until a one-DIMM capture identifies
each address independently.

## Memory-controller state

### Common controller

`ff:03.0 MC_CONTROL = 0x00000740` has all three documented
`CHANNEL[n]_ACTIVE` bits and `DIVBY3EN` set.  `INIT_DONE` is write-only and
cannot be inferred from its readback.  `MC_CHANNEL_MAPPER = 0x00024489`
decodes to identity mapping for reads and writes on all three channels.

`MC_MAX_DOD = 0x000000d4` documents the maximum populated geometry as one
DIMM per channel, two ranks, eight banks, 2^15 rows, and 2^10 columns.  That is
an exact match for the SPD geometry.

`MC_STATUS = 0x00000018` sets the 5500-datasheet `ECC_ENABLED` bit but also a
bit left reserved in that base document.  At the same time, `MC_CONTROL.ECCEN`
is clear and the SPDs have no ECC bus-width extension.  DMI incorrectly claims
72-bit/ECC modules.  Therefore this capture does **not** justify claiming that
ECC is operational; the Westmere meaning/readback needs further isolation.

### Per-channel state

All channels report the same organization and almost all the same trained
state:

| Register | Channel 0 | Channel 1 | Channel 2 | Interpretation |
|---|---:|---:|---:|---|
| `MC_DOD_CH_DIMM0` | `0x2ac` | `0x2ac` | `0x2ac` | present, dual-rank, 8 banks, 15 rows, 10 columns |
| `MC_CHANNEL_RANK_PRESENT` | `0x03` | `0x03` | `0x03` | logical ranks 0 and 1 |
| `MC_RIR_LIMIT_CH_0` | `0x0f` | `0x0f` | `0x0f` | 4-GiB channel-local range |
| first four RIR ways | `0,1,0,1` | `0,1,0,1` | `0,1,0,1` | two-rank interleave |
| `MC_CHANNEL_DIMM_INIT_STATUS` | `0x140` | `0x140` | `0x140` | idle; final command complete and WR DQ-DQS passed |
| round-trip latency | 51 UCLK | 51 UCLK | 53 UCLK | channel 2 trained two UCLK longer |

The init-status register is cleared by each new training command.  Its
`0x140` value describes the final command; clear pass bits for earlier phases
must not be interpreted as training failures.

`MC_DIMM_CLK_RATIO_STATUS = 0x0a000008` reports maximum ratio 10
(DDR3-1333) and current ratio 8 (DDR3-1066).  The requested ratio is also 8.
The vendor firmware therefore deliberately runs these DDR3-1333 SPDs at the
more conservative DDR3-1066 setting.

## Reconstructed physical memory map

The enabled SAD rules are:

| Rule | Value | Range | Mode |
|---|---:|---|---|
| 0 | `0x00000bc3` | 0 to 3 GiB | address XOR mode; package list points to socket 0 |
| 1 | `0x00000fc0` | 3 to 4 GiB | disabled, hence MMIO hole |
| 2 | `0x000033c3` | 4 to 13 GiB | address XOR mode; package list points to socket 0 |

TAD rules use the same limits but values `0x00000bc5` and `0x000033c5`.
Their mode is documented three-way `MOD3`, and interleave list
`0x02100210` assigns channels in the `0,1,2` pattern.  Thus 3 GiB below the
hole plus 9 GiB above 4 GiB produces the observed 12 GiB.

The kernel E820 map independently agrees: RAM below `0xc0000000`, then RAM
from `0x100000000` through `0x33fffffff`.  X58 IOH `TOLM = 0xbc000000`
decodes through its inclusive 64-MiB-granular field to `0xbfffffff`; the
64-bit `TOHM` pair is `0x000000033c000000`, ending at `0x33fffffff`.
Non-coherent memory is disabled because its base is greater than its limit.

## CPU-side QPI

Only CPU QPI link 0 is connected to the IOH:

- `ff:02.0 QPI_QPILS_L0 = 0x86000000`, including documented
  `CHIPSET_LINK = 1`;
- `ff:02.1 QPI_0_PH_PIS = 0x070f0f03`: operational-speed initialization,
  remote ACK, TX ready, RX/TX state `0x0f`, calibration complete, and link-up
  identifier set;
- link 1 reports `QPI_QPILS_L1 = 0` and no link-up identifier.

`QPI_0_PLL_STATUS = 0x160c0112` decodes to maximum ratio 22, minimum 12,
ratio mask 1, and current ratio 18.  At the documented 133-MHz reference this
is about 2.394 GHz forwarded clock, corresponding to the 4.8-GT/s operating
class.  The next requested ratio is also 18.

## Vendor test/PHY registers and matching live code

The unpublished values are stable across all three 256-byte snapshots:

```text
03.4:5c = 0x00000001
03.4:ac = 0x00380000
03.4:c4 = 0x00000000
03.4:c8 = 0x00000000
03.4:cc = 0x00000000
03.4:f8 = 0x00001545
03.4:fc = 0x942004d7
```

In particular, bit 30 of `03.4:f8` is clear in the completed runtime state.
This matches the static observation that ten vendor loops wait while that bit
is set, strengthening the neutral busy/completion interpretation without
inventing a field name.

`MC_TEST_PAT_BA = 0x89abcdef`.  The matching live `MINITDLL` code contains
literal `push 0x89abcdef` instructions at `0xfffc6f22` and `0xfffc722c`, each
immediately followed by offset `0xb0` and the 32-bit MMCONFIG write helper.
The public register map identifies `03.4:b0` as the memory-test pattern
buffer.  This is a direct static-code/live-register anchor.

## Live firmware identity

Two independent internal-flash reads were byte-identical:

```text
size:    4,194,304 bytes
SHA-256: 75232f915d44f2180d35c6b293d21694fae8c217c8f54bbd84a9a7c8819a2db1
```

The proprietary image remains only at
`blobs-local/msi-x58-pro-e/vendor-live-v8.14b8-2026-08-04.bin`.

The live image contains `MINITDLL` at file offset `0x3c1dc0`, preferred base
`0xfffc1dc0`.  Its raw PE differs from the public `8F0` derivative because
headers and `.data` differ, but both `.text` sections are 94,749 bytes and
have the identical SHA-256:

```text
c93135af56b85c28bb6f6da6589a2f5a7a35c6a79407b21745d369bc1aa292f4
```

The static analyzer consequently returns the same 450 helper calls, 118
literal Uncore BDF accesses, 85 named accesses, and an identical access list
for both images.  This upgrades the earlier code/register work from
cross-version analogy to analysis of the code actually present on the target.

## Controlled IOH device-hide experiment

X58 IOH Device 20/function 0 had `DEVHIDE1 = 0x3fffef74`; bits 26 through 29
hide Device 16/17 functions 0/1, the IOH-side QPI link and protocol blocks.
The probe performed only this masked transition:

```text
original:       0x3fffef74
during probe:   0x03ffef74
after restore:  0x3fffef74
```

The clear read back successfully, but direct ECAM reads of all four functions
still returned `0xffffffff`.  `IOHBUSNO` is valid and selects bus 0, so an
incorrect bus number is excluded.  The datasheet restricts changes in this
CSR group to an Intel QPI quiescence flow; the strongest present inference is
that changing the stored hide bits during normal traffic does not update the
active internal decode.  The system was left with the exact original hide
value.  Entering QPI quiescence was not attempted because its control block is
itself in the hidden Device 16/function 1 space.

## Invasive runtime probes

The follow-up probes were run from the disposable PartedMagic initramfs.  The
structured summary is
[`captures/2026-08-04-runtime-invasive-probes.json`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
and the reproducible helper is
[`../../scripts/probe_westmere_runtime.py`](../../scripts/probe_westmere_runtime.py).

### Memory lock and channel power-down

`ff:00.0 MEMLOCK_STATUS = 0x00040401` has documented
`MEM_CFG_LOCKED` bit 0 set.  Writing the documented `MC_CFG_UNLOCK` value `2`
to write-only `MC_CFG_CONTROL` did not clear it.  Changing channel 0
`MC_CHANNEL_0_CKE_TIMING_B.tRANKIDLE` from 50 to 16 DCLK was consequently
rejected: requested `0x00000204` read back as original `0x00000644`.

This establishes that per-channel automatic power-down policy is programmable
before the hard memory lock, but not from this post-POST state.  Westmere
documents DRAM self-refresh for package C3/C6 and S3; it does not expose a
separate live single-channel self-refresh action in the public register set.

### Error injection

Loading `i7core_edac` succeeded and exposed the hardware injection interface.
A one-shot ECC-bit injection request on channel 0 was nevertheless not armed.
The driver logged failed readbacks for `ADDR_MATCH+4`, `ECC_ERROR_MASK`, and
`ECC_ERROR_INJECT`; all remained zero.  The following 64-MiB write produced
neither CE nor UE counts.  This is a lock rejection, not evidence that an
injected error was handled.

The driver labels the DIMMs `S4ECD4ED`, while their verified SPD data has no
ECC extension.  This reinforces the warning that the apparent ECC status on
this consumer-board configuration is not proof of physical ECC operation.

### QPI periodic retraining and action bits

The vendor configuration leaves periodic QPI retraining enabled:

- `QPI_0_PH_PRT = 0x00322808` gives interval 8 and exponential factor 10;
- at 4.8 GT/s the documented formula predicts approximately 1.748 ms;
- tight live polling observed L0R transitions at approximately 1.743 ms and
  integer multiples;
- `QPI_0_PH_PIS` moved from normal L0 `0x070f0f03` through
  `0x070e0e03`, `0x070e0f03`, and `0x070f0e03`, exposing the two PHY
  tracking halves entering documented state `0xE`.

Writes to `RETRAIN_NOW` did not produce a persuasive increase over natural
periodic events: 6/100 control windows versus 9/100 request windows.  A write
to the documented post-L0-exempt `PHY_RESET` action returned in 9.9 us, but no
distinct initialization or recovery state was observed.  A tighter A/B test
found only normal L0R states in 3/200 control windows and 2/50 PHY-reset
windows.  Therefore neither action is claimed to have executed: a PCI config
write could return only after an internal action completed, or the action may
have been ignored in the present state.

The final audit confirmed the original three channel timing values, QPI
control/status/timing values, hard lock, and zeroed injection registers.  The
host remained reachable and no reset or lockup occurred.

## Public register sources

- [Intel Xeon 5500 Series Datasheet Volume 2, 321322](https://www.intel.de/content/dam/www/public/us/en/documents/datasheets/xeon-5500-vol-2-datasheet.pdf)
- [Intel Xeon 5600 Series Datasheet Volume 2 supplement, 323370](https://www.intel.ie/content/dam/www/public/us/en/documents/datasheets/xeon-5600-vol-2-datasheet.pdf)
- [Intel X58 Express Chipset Datasheet, 320838](https://www.intel.de/content/dam/doc/datasheet/x58-express-chipset-datasheet.pdf)
- [Linux `i7core_edac` driver](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/edac/i7core_edac.c)

Document 323370 explicitly supplements the unchanged 5500-series register
documentation rather than replacing it, so both CPU documents are required
for this E5645 capture.

## Highest-value next experiment

A one-DIMM cold boot in each physical slot would establish the exact
SPD-address/channel/slot wiring and reveal which trained registers are policy
versus per-channel results.  A warm-reset snapshot with the same population
would then isolate reset-retained PHY fields.  Neither requires writing an
undocumented live memory-controller register.
