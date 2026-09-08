> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI MINITDLL X58 PHY scan-chain notes

Status: verified-static for the pinned `V8.14B8` MINITDLL descriptor copy and
instruction sequences; verified-live on B06J for the bounded channel-2
read/write protocol and training-result mutation.  Field semantics remain
neutral unless the Intel register documentation establishes them.

## Descriptor source

`MINITDLL` `.data` begins at `0xfffd9220`.  Initialization at `0xfffc2989`
copies `0x23a` entries from `0xfffd9c11` into workspace index base `0x298`.
Every entry is packed as:

```c
struct phy_descriptor {
        uint16_t start_bit;
        uint8_t width;
};
```

The final descriptor ends at scan bit `0x1603`.  MINIT's read helper adds the
neutral fixed tail `0x27`, giving read-chain base `0x162a`.

## Access protocol

All accesses use CPU Uncore device `ff:03.4`:

```text
0x5c[26:25]  channel selector
0xf8         command and busy state
0xfc         scan data
```

For descriptor `{start,width}`, let `end = start + width - 1` and
`mask = (1 << (width - 2)) - 1`.  The two upper bits within the field are
control bits used by MINIT, not caller payload.

```text
write payload = (value & mask) | ((mode + 2) << (width - 2))
write command = 0x40000000 | end
write busy    = command bit 30

read command  = 0x80000000 | (0x162a - end)
read busy     = command bit 31
read value    = register 0xfc & mask
```

MINIT waits until `0xf8[31:30]` is clear before a new command.  All open-code
implementations must retain finite timeouts.

## Coarse-RD profile, channel 2 rank 0

The high-level path pulses nine width-7 descriptors to value two with mode one
around its coarse-RD routine:

```text
start: 15fd 13ae 115f 0f10 0936 06e7 0498 0249 0cb7
width: 7 for every entry
```

Within the `0..0x80` even-valued sweep, the rank-wide width-9 descriptor starts
at `0x09c7`.  For each non-ECC data lane, two result fields are seeded to zero:

| Lane | A start/width | B start/width |
|---:|---:|---:|
| 0 | `13f9/12` | `1405/11` |
| 1 | `11aa/12` | `11b6/11` |
| 2 | `0f5b/12` | `0f67/11` |
| 3 | `0d0c/12` | `0d18/11` |
| 4 | `0732/12` | `073e/11` |
| 5 | `04e3/12` | `04ef/11` |
| 6 | `0294/12` | `02a0/11` |
| 7 | `0045/12` | `0051/11` |

MINIT then issues rank-specific RD DQ/DQS training command `0x26b01` and tests
the corresponding pass status.  The pinned MSI routine also performs more
setup through `fffd2bbd`; the descriptor profile alone did not produce a pass
in B06J.

## Preliminary RCVEN profile, channel 2 rank 0

The rank-wide width-8 descriptor begins at `0x09fa`.  MINIT clamps its useful
value to at least one.  Each primary lane field is seeded to zero and each
secondary field to `0x15e`:

| Lane | Primary start/width | Secondary start/width |
|---:|---:|---:|
| 0 | `13bd/9` | `13c6/11` |
| 1 | `116e/9` | `1177/11` |
| 2 | `0f1f/9` | `0f28/11` |
| 3 | `0cd0/9` | `0cd9/11` |
| 4 | `06f6/9` | `06ff/11` |
| 5 | `04a7/9` | `04b0/11` |
| 6 | `0258/9` | `0261/11` |
| 7 | `0009/9` | `0012/11` |

After FIFO reset `0x20600`, MINIT uses receive-enable-only command `0x27301`.
The live B06J experiment proved that this command changes the lane fields.  At
global values one and `0x10`, lane 0 and lane 7 both returned primary `0x11`
and secondary `0x1ff`; status completed at `0x100` without the pass bit.  This
is evidence that the training engine and scan interface run, but that a prior
setup phase remains absent.

## Boundaries

These tables are reverse-engineered local research outputs, not copied vendor
firmware.  They apply only to the pinned Westmere/X58 path, CPU unit zero,
channel 2, rank 0, and a non-ECC eight-data-lane configuration.  They must not
be generalized to other channels, ranks, ECC, registered DIMMs, Bloomfield,
or another MINIT revision without separate correlation.

## Intel MRS cross-check and live late-phase probes

Intel DX58SO function `0xffe37125` performs the same CKE and
MRS2/MRS3/MRS1/MRS0 sequence but does not issue MSI's preceding bank-4
command.  In MSI `0xfffd2bbd`, that command is conditional and its low word
is `0x2000 | table_byte`, where `table_byte` is selected from a runtime
15-row DIMM table.  The prior experimental constant `0x3308` is therefore
not supported by the disassembly and has been removed from the default live
and firmware paths.  It remains possible to supply a reconstructed MSI value
explicitly to the live script, but omission is now the safe default.

On B06J, the complete fixed common/channel profile with channel field
`0x0a02=3` was held while the Intel MRS sequence and all 65 even RD values
`0x00..0x80` ran.  Every point still completed with status `0x100`, no pass
bits and eight lane tuples `[3,0]`; all 23 saved PHY fields restored and QPI
remained `0x030f0f03`.  The resulting JSON is byte-identical to the earlier
failed sweep because that log format did not record the bank-4 choice; its
70-line serial-gateway.example.invalid file therefore has the same SHA-256
`6bffce99261fc589825b8a46a6f432a3d0a0ecfaf9b88c4cb6f71a03b2c8a97b`.

The Intel main path also exposes the stage order `0x26b01` (RD), `0x27301`
(RCVEN), `0x25b01` (write-level) and `0x23b01` (write DQ/DQS).  Bounded live
probes of the two later commands both completed at status `0x100` without
their expected pass bit; QPI stayed unchanged.  Consoleio log SHA-256:
`219607502ad91bcf0bb9867150512fba0533f4aee37fe15c754a7ae76e59cef1`.
This supports the existing conclusion that a shared earlier PHY/per-DIMM
prerequisite is absent; another coarse sweep is not justified.

## Intel DX58SO correlation and live precommand result

The locally extracted Intel DX58SO PEI module with GUID
`63C0690C-5D9E-4EE3-840E-FAE0E76E291A` and PE32 SHA-256
`aca71404ccb5eb9df65997ee887914db3231503f0cfb0c0b3b5ca2b06135e4ae`
contains literal commands `0x26b01` and `0x27301`.  Around virtual addresses
`0xffef636f` and `0xffef6b93`, respectively, it constructs the rank bits,
writes channel offset `0x54`, and polls channel status offset `0x5c` bit 8.
The RCVEN path also:

- writes `0x0100` to the low word of channel offset `0xa0`, publicly
  correlated as `MC_CHANNEL_ODT_PARAMS2`;
- writes the same per-lane secondary seed `0x15e` through its PHY helper at
  `0xfff1cb1c`;
- performs FIFO reset `0x20600` in helper `0xfff1c4ec`;
- on a model/stepping-dependent path, calls `0xfff1c5ac`, which writes
  `0x30200` and waits for status bit 9.

The optional precommand was exercised on the live B06J board.  It completed
with command readback `0x00010200` and status `0x00000300`, without changing
QPI status.  Repeating the complete 17-descriptor RCVEN seed set followed by
`0x20600`, `0x30200`, and `0x27301` still produced status `0x00000100`.
Every non-ECC data lane returned primary `0x011` and secondary `0x1ff`.
Thus this precommand is real and operational, but is not the missing global
prerequisite by itself.

## Broad PHY initializer and Intel structural match

MSI function `0xfffca5fa`, called from the B7 path at `0xfffcb6cb`, is the
broad PHY setup which precedes the later RD and RCVEN routines.  Its scan
writer is `0xfffc7e18`.  The Intel DX58SO `63C0690C...` PEIM contains the
structurally corresponding function at `0xffe5686c`, with writer
`0xffe5578c`.  Both functions perform the same four common-PHY writes,
revision-dependent common writes, channel-local fixed writes, one computed
channel write, and the following nested per-DIMM/per-lane loops.  MSI uses
packed three-byte descriptors; Intel uses four-byte descriptors.  This is an
exact control-flow/data-layout correlation, not a claim that either module has
a callable standalone ABI.

The first four fields are common PHY fields.  They require selector 3 and the
Westmere-EP common read-chain base `0x27 + 0x2d4 = 0x2fb`, not channel 2:

```text
0x00ab/5 = 0x07 mode 1
0x0086/5 = 0x07 mode 1
0x00c2/6 = 0x0a mode 1
0x00bc/6 = 0x04 mode 1
```

For live PCI ID `8086:2c70`, revision 2, the additional unconditional common
fields are:

```text
0x0043/4  = 0x02 mode 1
0x021b/8  = 0x21 mode 1
0x00db/5  = 0x02 mode 1
0x01e9/10 = 0x01 mode 0
0x0209/8  = 0x01 mode 0
0x00e0/9  = 0x12 mode 1
0x0197/8  = 0x06 mode 1
```

The two mode-0 writes read back zero.  They may be pulse or control fields;
no semantic name is assigned.  The fixed channel-2 prefix for the pinned
non-ECC UDIMM path resolves to:

```text
0x09c3/4=3  0x09ad/4=3  0x0a5c/6=0x0f
0x0988/8=0x3f  0x0973/14=0xfff  0x0a62/3=1
0x0a0a/5=3  0x099a/9=0  0x09d9/9=0
0x0a1c/9=0  0x0a53/9=0
```

`0x0a1c/9` retained value 7 after the zero write, so it too must not yet be
treated as an ordinary stored payload field.

MSI helper `0xfffc80f9` and Intel helper `0xffe5861c` calculate descriptor
`0x0a02/8`.  The inputs are the selected memory-frequency table entry, a
channel-class index, and channel flag bit `0x20`.  The Intel UDIMM adjustment
table at `0xffe1b1b8` is eight bytes long (raw bytes omitted; SHA-256
`b949ee3037e33df053eb6bfe2d77f4b7aac9ebeb738704d645a625b36bbce763`); the alternate table at
`0xffe1b1c0` differs only at its first three entries.  For the pinned DDR
ratio-6, CL6 path, the recovered formula gives value 3 without the optional
channel flag and 5 with it.  Both values were tested separately and restored.

The complete fixed-prefix RD sweep used value 3 and covered every even point
from `0x00` through `0x80`.  All 65 points completed at status `0x100` without
a pass bit; every lane returned `[3,0]`.  All common and channel fields were
restored and CPU-side QPI stayed `0x030f0f03`.  Log SHA-256:

```text
6bffce99261fc589825b8a46a6f432a3d0a0ecfaf9b88c4cb6f71a03b2c8a97b
```

This rules out a missed coarse-RD window under the fixed prefix.  The next
bounded reconstruction target is the dynamic per-DIMM/per-lane body beginning
near MSI `0xfffcaf77` / Intel `0xffe57772`; blindly extending the coarse sweep
cannot substitute for that setup.

## Intel dynamic-topology body

The Intel body at `0xffe57772..0xffe58341` makes the missing dependency more
concrete.  It is not a single fixed PHY profile.  The outer loops cover two
DIMMs, four rank records per populated DIMM, and then either nine non-ECC lanes
or eighteen lane records when the channel policy requests the wider path.
Presence, rank, lane-map, timing, module-class, memory-frequency, and stepping
fields are read from the live MRC context before each descriptor is selected
and written.

In particular, the code obtains descriptor indices from runtime policy
structures and converts each index into the descriptor array at context offset
`0x978`.  Several payloads are calculated from frequency (`context + 0x50`),
DIMM type and per-rank byte-lane metadata; later paths also perform read/modify/
write operations on shared rank masks.  The corresponding MSI routine has the
same control-flow shape but uses its packed three-byte descriptor table.

This is evidence that copying only the final values observed after a vendor
boot, or inventing descriptor indices from the raw MSI workspace template,
would skip required policy construction.  The Intel PEIM itself does not
contain a self-contained fixed policy for the tested MSI one-UDIMM topology;
that context is populated earlier from PEI board policy and SPD data.  The next
safe reconstruction step is therefore to recover the exact one-DIMM runtime
inputs (or their MSI equivalents) before issuing more live PHY writes.
