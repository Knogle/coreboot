> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# DDR ratio 6: isolated QPI Slow/High comparison

## Scope and evidence boundary

On 2026-08-31 the identical MSI X58 Pro-E reference machine at
`192.0.2.203` was observed twice under vendor firmware with the DDR policy
held at ratio 6 (DDR3-800).  The first state used QPI Slow Mode and the second
used QPI High Speed.  These are post-initialization snapshots of an already
running Linux system, not reset-state captures or a recovered programming
sequence.  No register was written from Linux during collection.

Three complete CPU-Uncore PCI configuration captures in each state were
byte-identical within that state:

```text
de3ec2ae667abe816cb770ad9b4ed033c8d414f82d711240dd6f85092d2e73b4  ratio6/QPI-Slow (each of three)
53c9f7caf24e6495f4741fa6b990a13b87cdd5d99539ef7ce35612de35b94a23  ratio6/QPI-High (each of three)
1a8ac52cba615c7f3fe39db32c015964e40c52f159bdcb80e140bf8360a064c5  ratio6/QPI-High plus visible IOH functions
```

> **Superseding A0 note (updated 2026-09-05):** The table below preserves the original
> 2026-08-31 comparison, but its suggestion that CPU `ff:02.1:0xa0` was partly
> DDR-ratio-dependent is not established as a deterministic encoding. Later
> controlled ratio-6 G3 sequences observed the additional High-QPI values
> `00017800`, `00017a00`, and, in the B06VC runs on 2026-09-05, `00017c00`.
> Together with the earlier `00017000` and `00017600`, the exact observed
> High-QPI set is
> `{00017000, 00017600, 00017800, 00017a00, 00017c00}`. Bits 11:9 vary across
> these samples, but the observations establish no field width, name,
> read/write behavior, semantic meaning, or valid mask/range. The conservative
> interpretation is an unknown reset- or training-variable value within an
> otherwise matching High-QPI endpoint; that interpretation remains an
> inference.

> **Pass-3 CSI follow-up (2026-09-05):** Four controlled B06VC G3 runs had
> preselector A0 `17c00/17a00/17c00/17000`. The two admitted values invoked
> pass-3 CSI and returned ABI-clean with result `0/0/11`; both produced the
> same 772-byte state (SHA-256
> `0a10266b0e65c8888b001b386ce73460075e6cc810ed58cd00338a29ce13f199`,
> FNV `03e3d24e`) and byte `0x2a6=08`. This state differs from the saved B06VA
> state (SHA-256
> `ed71cba07f00d61cef6cf4c132877bb323b84f32c15622d737998ea705e4d8a7`,
> FNV `8b38506a`) at exactly byte `0x2a6`, whose saved value was `0c`.
> Read-only canonicalization of that byte to zero yields FNV `908dabb6` for
> all three samples. This supports an exact `{08,0c}` successor hypothesis;
> it does not establish a field meaning or authorize a wider mask/range.

Raw evidence:

- [`ratio6/QPI-Slow 01`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
  [`02`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index), and
  [`03`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [`ratio6/QPI-High 01`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
  [`02`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index), and
  [`03`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [`ratio6/QPI-High visible-IOH capture`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)

## Isolated state differences

The stable CPU link-0 differences are:

| Function/register | QPI Slow | QPI High | Interpretation boundary |
|---|---:|---:|---|
| `ff:00.0 +0xd0` | `0x00000183` | `0x00000180` | mode-coupled control |
| `ff:02.1 +0x50` (`QPI_0_PLL_STATUS`) | `0x160c0110` | `0x160c0112` | High-Speed PLL state |
| `ff:02.1 +0x54` (`QPI_0_PLL_RATIO`) | `0x00000010` | `0x00000012` | High-Speed ratio 18 |
| `ff:02.1 +0x6c` (`QPI_0_PH_CTR`) | `0x0000a020` | `0x0040a0a8` | trained High-Speed PHY control |
| `ff:02.1 +0x80` (`QPI_0_PH_PIS`) | `0x030f0f03` | `0x070f0f03` | RX/TX link states remain `0xf` |
| `ff:02.1 +0x94` (`QPI_0_PH_PTV`) | `0x00000102` | `0x00010202` | High-Speed training state |
| `ff:02.1 +0x9c` (`QPI_0_PH_LDC`) | `0x00000502` | `0x00b00502` | High-Speed link-delay state |
| `ff:02.1 +0xa0` (meaning unknown) | `0x00000c00` | `0x00017000` | coupled and partly DDR-ratio-dependent |
| `ff:02.1 +0xa4` (`QPI_0_PH_PRT`) | `0x001d2c03` | `0x00322808` | periodic-retraining policy |
| `ff:02.5 +0x50/+0x54` | ratio `0x10` | ratio `0x12` | mirrored PLL programming/state |

Additional link-0 fields at `+0xdc`, `+0xe0`, `+0xe4`, `+0xe8`, `+0xf0`, and
`+0xf4` also change.  The Slow-state mirror-like values in `ff:02.2` and
`ff:02.3` become zero in High Speed.  These observations identify a state
cluster; they do not establish which fields are writable, their order, or
their reset requirements.

The same High-Speed tuple at `+0x50`, `+0x54`, `+0x6c`, `+0x80`, `+0x94`,
`+0x9c`, and `+0xa4` is present in the older DDR-ratio-8 High-Speed capture.
The DDR3-800 timing, address-map, topology, and policy registers remain
byte-identical across this ratio-6 Slow-to-High transition.  This separates
QPI-speed selection from the DDR divider at the policy level.

Some IMC values did move: `ff:03.4 +0xfc`, channel `+0x54`, and channel-0 RTL
changed.  They are training or runtime residues, not static QPI policy and not
values to hard-code.  The visible IOH snapshot was otherwise stable; the only
unrelated visible change was a historical PCI status bit.

A one-second live poll collected 53,700 link-0 samples.  `PH_CTR` stayed at
`0x0040a0a8` and `PH_PRT` at `0x00322808`; `PH_PIS` was normally
`0x070f0f03`, with 57 brief transitions through the expected `0xe` retraining
substates.  This is consistent with the already documented periodic QPI L0
retraining rather than instability.

## Bring-up decision

The comparison does **not** justify replaying CPU-side values.  Static CSI
correlation shows paired CPU/IOH PHY fields, a speed/PLL choice, scrambler and
delay programming, controlled reset, calibration, and bounded training.  An
isolated write of `PH_CTR=0x0040a0a8` would skip that protocol.

The next experimental image therefore keeps the autonomous Slow-QPI link
untouched and advances only the SPD evidence boundary: fixed address `0x54`,
DDR3 base bytes `0x00..0x7f`, and the revision-selected JEDEC CRC.  High-Speed
QPI remains a later, separately recoverable milestone after stable DRAM.
