> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI `RUN_CSEG` ICH10R SATA/AHCI policy analysis

Date: 2026-09-06

Status: static reverse engineering, cross-checked against public Intel
documentation, historical coreboot code, SeaBIOS, and the separately retained
ROMMON observations named below.  No vendor executable bytes are included in
this document.  No hardware write was performed for this analysis.

## Result

The code at `F000:6527..68e0` is mostly ICH10 SATA *firmware policy*, not an
AHCI disk-initialization routine.  The AHCI-specific tail does exactly this:

1. set D31:F2 `PCS.ORM` (`PCI+0x93`, bit 7 of that byte);
2. continue only when the SATA subclass byte is greater than `0x01`;
3. derive an ABAR address from BAR5;
4. read one one-bit AMIBIOS setup field whose human-readable meaning is not
   yet established;
5. write `PI[5:0]=0x3f`;
6. perform one 32-bit read/OR/write of `PxCMD` for each implemented port,
   setting either `HPCP` or `ALPE|ASP` according to the setup field; and
7. program the write-once platform fields in `CAP`.

It does **not** set `GHC.AE`, issue `GHC.HR`, allocate a command list or FIS
receive area, set `PxCMD.FRE`, set `PxCMD.SUD`, request `ICC=Active`, set
`PxCMD.ST`, clear `PxSERR`, enable bus mastering, poll a link, wait for task-file
readiness, or issue an ATA command.  Those absences are decisive: copying this
tail cannot by itself make a disk usable.

The minimum defensible pre-payload policy remains:

```text
GHC.AE = 1                 public AHCI/ICH10 ordering requirement
PI      = 0000003f         exact MSI write, independently proved live
```

This pair is not attributable to the MSI block alone: the MSI block proves
`PI=3f`, while the public specification supplies the required `AE` ordering.
The conditional vendor `PxCMD` and broad `CAP` policy should not be copied into
the first storage build.

## Input identity and reproducibility

Locally supplied input, intentionally ignored by version control:

```text
path:   blobs-local/msi-x58-pro-e/extracted/RUN_CSEG.bin
size:   65536 bytes
sha256: dbabc6b1385be15fc6f83deb14bbe39c48166362539626e8d4a87bf52e43a291
```

The offsets in this note can be reproduced without committing the input:

```bash
objdump -D -b binary -m i8086 --adjust-vma=0 -M intel \
  --start-address=0x6500 --stop-address=0x68e1 \
  blobs-local/msi-x58-pro-e/extracted/RUN_CSEG.bin
```

The review used GNU objdump 2.46.1.  Public comparisons are:

- Intel, *I/O Controller Hub 10 (ICH10) Family Datasheet*, document
  `319973-003`, especially sections 14.1.30--14.1.32 and
  14.4.1/14.4.3;
- Intel, [Serial ATA AHCI Specification 1.3.1](https://www.intel.com/content/www/us/en/io/serial-ata/serial-ata-ahci-spec-rev1-3-1.html),
  especially sections 3.1, 3.3.7, 10.1, and 10.4.3;
- [`coreboot/src/southbridge/intel/i82801jx/sata.c`](../../../../src/southbridge/intel/i82801jx/sata.c)
  at coreboot base `fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2`;
- [`SeaBIOS src/hw/ahci.c`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
  at `b52ca86e094d19b58e2304417787e96b940e39c6`.

## Helper ABI and call context

The PCI helper calls around this routine establish the following local ABI:

- `DX=0x00fa` selects D31:F2 (`0xf8` is D31:F0);
- `AH` is the PCI configuration offset;
- the helper at `0x2cb6` reads one byte into `AL`;
- `0x2ccf` writes the byte in `AL`;
- `0x2d45` reads a dword into `EBX`;
- `0x2d5c` writes the dword in `EBX`; and
- the far helper reached through `F000:34d5` is a byte RMW-OR wrapper for
  D31:F2.

The only direct static call found is the wrapper at `F000:06b1`.  It calls
`F000:6527` and then `F000:68e1`.  The latter function operates on a different
PCIe configuration window and is not a continuation of AHCI link or disk
initialization.

At `0x6527..654a`, the routine selects `ES=0x68` only when the relevant BIOS
execution-state flag and protected mode are both active; otherwise it selects
`ES=0`.  All later ABAR references use a 32-bit address-size override through
this `ES`.  This is an addressing setup, not a SATA state transition.

## Indexed SATA setup before the AHCI tail

Offsets `0x654b..6817` program D31:F2's index/data pair (`SIDX=PCI+0xa0`,
`SDAT=PCI+0xa4`) and `SCLKCG`.  This is relevant as a prerequisite in the
vendor ordering, but the index semantics remain undocumented here.  Exact
access width matters.

The predicate used several times reads the D31:F0 device ID and returns equal
only for `0x3a10`, `0x3a14`, or `0x3a1a`.  The target LPC ID `0x3a16` takes the
*not-equal* branch.

| Code range | Exact operation |
|---|---|
| `654b` | Select D31:F2 in `DX`. |
| `654e..6557` | Select index `0x40`; write byte `0x22` to `SDAT+2`. |
| `655a..6563` | Select index `0x78`; write byte `0x22` to `SDAT+2`. |
| `6566..656f` | Select index `0x94`; write **one byte** `0x22` to `SDAT`. |
| `6572..65f0` | Select `0x88`; 32-bit RMW using the sequential masks and values shown below. |
| `65f3..6671` | Select `0x8c`; 32-bit RMW using the sequential masks and values shown below. |
| `6674..66ca` | Select `0xa8`; 32-bit RMW using the sequential masks and values shown below. |
| `66cd..6731` | Select `0xac`; 32-bit RMW using the sequential masks and values shown below. |
| `6734..676d` | Select `0x18`; 32-bit RMW; for the fixed target branch its low nine bits end as `0x012`. |
| `6770..677e` | Select `0x28`; write dword `0x01cc2080`. |
| `6781..67b7` | Select `0x84`; 32-bit RMW; for the fixed target branch its low six bits end as `0x12`. |
| `67ba..67fb` | Select `0xa0`; 32-bit RMW; the final unconditional step makes its low byte `0x1b`. |
| `67fe..6801` | Select index `0x00`. |
| `6804..6817` | Write `SCLKCG` dword `0x183` for the three-ID predicate, otherwise `0x193`; target `0x3a16` therefore gets `0x193`. |

For auditing, the exact sequential dword transformations are below.  They are
shown as operations rather than collapsed “magic” final values so that field
overrides and access order remain visible.  `family` means the three-ID
predicate above; the temporary policy word is literally zero in this image.

```text
SIDX 88:
  r &= c0c0c0c0
  r |= family ? 24242424 : 24241212
  r = (r & fff8fff8) | 00020001
  r = (r & ffc7ffc7) | 00100008
  r = (r & f8fff8ff) | 02000100
  r = (r & c7ffc7ff) | 10000800

SIDX 8c:
  r &= c0c00000
  r |= family ? 242400aa : 121200aa
  r = (r & fff8ffff) | 00010000
  r = (r & ffc7ffff) | 00080000
  r = (r & f8ffffff) | 01000000
  r = (r & c7ffffff) | 08000000

SIDX a8:
  r &= ffc0ffc0
  r |= family ? 00240024 : 00240012
  r = (r & fff8fff8) | 00020001
  r = (r & ffc7ffc7) | 00100008

SIDX ac:
  r &= ffc0fff0
  r |= family ? 0024000a : 0012000a
  r = (r & fff8ffff) | 00020000
  r = (r & ffc7ffff) | 00100000
  r = (r & ff00fff0) | 0009000a

SIDX 18:
  r = (r & fffffe00) | 00000024
  r = (r & fffffff8) | 00000002
  r = (r & ffffffc7) | 00000010

SIDX 84:
  r = (r & ffffffc0) | 00000024
  r = (r & fffffff8) | 00000002
  r = (r & ffffffc7) | 00000010

SIDX a0:
  r = (r & ffffffc0) | 00000024
  r = (r & fffffff8) | 00000002
  r = (r & ffffffc7) | 00000010
  r = (r & ffffff00) | 0000001b
```

This table is not proposed for implementation.  In particular, the historical
i82801jx code is similar in shape but is not byte-for-byte equivalent:

- its index-`0x28` literal is `0x00cc2080`, not MSI's `0x01cc2080`;
- it writes a dword at index `0x94`, whereas MSI writes only the low byte;
- its fixed formulas for several indexed fields differ from the actual
  `0x3a16` MSI path; and
- it uses a different operation order.

The known `SCLKCG=0x193` field has already been isolated separately; the
indexed table has not.

## Exact AHCI control flow, `0x681a..68e0`

| Offset | Width | Condition/effect |
|---|---:|---|
| `681a` | PCI byte RMW | D31:F2 `PCI+0x93 |= 0x80`, i.e. `PCS.ORM=1`. |
| `6822` | PCI byte read | Read subclass at `PCI+0x0a`; `AL <= 1` branches directly to return at `68df`.  AHCI subclass `0x06` continues. |
| `682d` | PCI dword read | Read BAR5 at `PCI+0x24` into `EBX`, then clear only the low ten bits: `EBX &= 0xfffffc00`. |
| `6836` | register | Set `DX=0` and save it around the setup-service call. |
| `6839` | setup-field read | Query encoded field `AX=0x15c1`.  The helper decodes this as width 1 and setup bit offset `0x5c1`; it returns the value in `AX` and leaves ZF reflecting zero/nonzero. |
| `6842` | branch | If the setup value is zero, set `DH.bit3`; nonzero leaves it clear. |
| `6847` | MMIO byte write | `ABAR+0x0c = 0x3f`, i.e. write PI low byte only. |
| `684d` | MMIO byte read | Read PI low byte back into `CL`; set `CH=0`.  There is no equality assertion. |
| `6854..6859` | register | Copy PI to `DL`, set `DH |= 0x06`, then immediately overwrite `DL=0`. |
| `685b` | stack | Save the resulting policy word for the later CAP write. |
| `685c..68a1` | MMIO dword RMW loop | Visit implemented ports selected by PI; exact reconstruction follows. |
| `68a3..68da` | MMIO dword RMW | Program CAP platform fields; exact reconstruction follows. |
| `68df..68e0` | — | Restore `ES`; far return. |

Two prerequisites are assumptions rather than actions in this block:

- BAR5 must already be assigned and memory decoding must make ABAR reachable.
- `GHC.AE` must already be 1 for standards-compliant access.  The block does
  not set it.  Whether the original caller did so elsewhere is not proved by
  this local function.

The BAR operation must be described literally: it clears ten low bits, not
eleven.  ICH10 exposes a 2-KiB BAR in the live target, but this instruction by
itself is only `BAR5 & 0xfffffc00`.

## Exact `PxCMD` loop, `0x685c..68a1`

For this exact binary, the policy inputs at loop entry are:

```text
CL = readback(PI[7:0])       normally 3f after the preceding write
CH = 00
DL = 00                      PI was copied here, then overwritten
DH = 0e if SETUP[bit 5c1] == 0
DH = 06 if SETUP[bit 5c1] != 0
BX = (BAR5 & fc00) | 0118
```

The loop is equivalent to:

```c
unsigned port = 0;

do {
        bool implemented = CL & 1;
        CL >>= 1;

        if (implemented) {
                uint32_t cmd = mmio_read32(ABAR + 0x118 + port * 0x80);

                if (setup_bit_0x5c1 == 0)
                        cmd |= 1u << 18;          /* HPCP */
                else
                        cmd |= (1u << 26) |       /* ALPE */
                               (1u << 27);        /* ASP */

                /* DL==0 makes the ISP path unreachable. */
                /* CH==0 makes the ESP path unreachable. */
                mmio_write32(ABAR + 0x118 + port * 0x80, cmd);
        }

        if (CL == 0)
                break;
        CH >>= 1;
        port++;
} while (true);
```

The two `BSWAP EAX` instructions around the byte operations obscure, but do
not change, that result:

- `OR AH,0x04` in the swapped representation sets native bit 18 (`HPCP`);
- `OR AL,0x0c` sets native bits 26 and 27 (`ALPE|ASP`);
- the dead `OR AH,0x08` path would set native bit 19 (`ISP`); and
- the dead `OR EAX,0x00200000` after swapping back would set native bit 21
  (`ESP`).

With PI readback `0x3f`, exactly six full-dword RMW writes occur in order:

```text
port 0: ABAR + 118
port 1: ABAR + 198
port 2: ABAR + 218
port 3: ABAR + 298
port 4: ABAR + 318
port 5: ABAR + 398
```

For a sparse PI value, a clear bit skips the port write while the address
still advances as long as any higher PI bit remains.  There is no delay,
readback assertion, or poll.  The loop terminates after the highest set PI bit,
not after a hard-coded count of six; the immediately preceding byte write of
`0x3f` makes the concrete execution six ports.

### Semantics and write-once caveat

ICH10 defines `HPCP` (18), `ISP` (19), and `ESP` (21) as platform firmware
fields with R/WO behavior.  `HPCP` and `ESP` describe physical connector
policy; they do not start a link.  `ALPE` and `ASP` are runtime aggressive
link-power policy and only have meaning when `CAP.SALP=1`.

Consequently this loop is best classified as *platform-description and power
policy*.  It is not PHY initialization.  A full `PxCMD` write intended only to
set `SUD`, `FRE`, or `ICC` also presents values for the R/WO fields and can
consume their current write-once programming opportunity.  The ICH10
datasheet defines R/WO generically as one write after power-up; its CAP section
additionally states that CAP's R/WO bits are reset only by `PLTRST#`.  For an
initial build, zero
is a valid deliberate conservative policy only if hot-plug/external/interlock
support is explicitly out of scope; it must not be mistaken for a reversible
default.

The setup selector must remain named neutrally as `SETUP[bit 0x5c1]`.  The
control flow proves its two effects, but neither the extracted setup strings
nor a runtime setup-variable capture currently proves its user-facing name.

## CAP follow-up, `0x68a3..68da`

After the port loop, the code returns `BX` to ABAR, reads CAP as a dword, and
performs:

```text
cap = (cap & e30dffff) | 402060a0
```

It then byte-swaps and tests the saved policy word.  Because `DH` always has
bits 1 and 2 set and `DL` is always zero, the concrete result collapses to:

```text
CAP' = (CAP & e30dffff) | 4c2060a0
```

The exact forced fields are:

- set bit 30 `SCQA`;
- clear bit 28 `SIS`;
- set bits 27 and 26 `SSS|SALP`;
- replace `ISS[3:0]` with `2` (3.0 Gb/s);
- clear bit 17 `PMS`;
- set bits 14 and 13 `SSC|PSC`;
- set bit 7 `CCCS`; and
- set bit 5 `SXS`.

All other bits are preserved by the mask.  The unresolved setup bit has no
effect on this final CAP value.  Starting from the observed target value
`ff22ffc5`, this formula predicts `ef20ffe5`.

This is another full dword containing R/WO platform fields.  The code neither
reads it back nor verifies that every write-once field accepted the value.

## Comparison with historical coreboot i82801jx

The historical driver has useful ancestry but is broader than the MSI block:

| Operation | MSI `RUN_CSEG` | Historical i82801jx |
|---|---|---|
| `GHC.AE` | not set in this function | set before other ABAR accesses |
| CAP | `(old & e30dffff) | 4c2060a0` | set `0x0c006080`, clear `0x00020060` |
| PI | byte write `3f`, one byte readback | dword `port_map`, two dword readbacks |
| VSP | untouched here | clear VSP bit 0 (`SLPD`) |
| PxCMD | OR conditional `HPCP` or `ALPE|ASP` | read dword, write same dword (“lock R/WO bits”) |
| HBA/link reset | none | none |
| link/receive/command engine | none | none |
| SIDX `0x28` | `01cc2080` | `00cc2080` |
| SIDX `0x94` | byte write to SDAT | dword write to SDAT |

For the observed CAP input `ff22ffc5`, the historical coreboot formula would
produce `ff20ff85`, versus `ef20ffe5` for the MSI formula.  Important policy
differences include `SIS`, `SXS`, and `EMS`; therefore neither formula should
be substituted for the other without platform evidence.

## Comparison with SeaBIOS

SeaBIOS performs the missing operational half:

1. enable the PCI memory BAR and bus mastering;
2. set `GHC.HR`, poll it for at most 500 ms;
3. set `GHC.AE` after HR completes;
4. read CAP and PI;
5. allocate aligned command-list, command-table, and receive-FIS memory;
6. program `PxCLB/PxFB`;
7. set `PxCMD.FRE`;
8. set `SUD|POD|ICC_ACTIVE`;
9. poll `PxSSTS.DET`;
10. clear `PxSERR` by writing back its set bits;
11. wait boundedly for `PxTFD.BSY|DRQ` to clear;
12. set `PxCMD.ST` and issue IDENTIFY.

SeaBIOS does not depend on MSI's `HPCP`, `ALPE`, or `ASP` policy for basic
device discovery.  In its current source it uses CAP only to decide whether
to program the high halves of DMA pointers; it discovers ports from PI.

AHCI 1.3.1 section 10.4.3 adds an important reset qualification.  HR clears
`GHC.AE` on this non-AHCI-only HBA, but it does not clear the other global
registers such as PI.  When staggered spin-up is supported, HR resets
`PxCMD.SUD` and software must initiate spin-up; automatic per-port COMRESET is
only required in the non-staggered case.  This matches the target observations
below.

## Dynamic cross-check already retained by the project

These observations were made independently of this static-analysis task:

- [`PI=3f` trace](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index):
  the write exposed all six port banks; all `PxCMD` registers materialized as
  `00002004`, and port 5 showed `DET=1`.
- [port-5 spin-up trace](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index):
  only `SUD` plus `ICC_ACTIVE`, with `ST=FRE=BME=0`, produced stable
  `SSTS=00000123` (DET=3, Gen2, active); `TFD=80` and
  `SERR=04050002` remained.
- [HBA-reset trace](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index):
  HR completed immediately, cleared `GHC.AE`, retained `PI=3f` and CAP, reset
  `PxCMD` to `00002004`, retained the established link/signature, and left
  `TFD=80` with `SERR=04050000`.

These results prove that the vendor policy loop is not required to bring the
physical port-5 link up.  They also show why HR alone cannot be expected to
clear task-file BSY on the CAP.SSS=1 path.

## Smallest next live ROMMON test

Do **not** test the vendor CAP formula or conditional `PxCMD` policy next.
Their fields are write-once/platform-specific, the setup selector is unknown,
and neither can explain basic link establishment.

The smallest new discriminating test is a port-5 `PxSERR` clear around one
already-proved non-DMA spin-up.  This tests a required AHCI transition that was
missing from the current sequence without introducing DMA memory:

1. Require the established exact gates: D31:F2 `8086:3a22`, class `010601`,
   MAP `0060`, PCS enable bits `3f`, `SCLKCG=00000193`, exact assigned ABAR,
   PCI memory decode on and BME off, `GHC.AE=1`, `GHC.IE=0`, `PI=3f`, and
   port-5 `ST=CR=FRE=FR=0`.
2. Record port-5 `CMD/TFD/SIG/SSTS/SERR/IS`.
3. Read port-5 `PxSERR` at `ABAR+0x3b0` and write the same implemented status
   bits back as W1C.  Require `PxSERR=0` before continuing.
4. Reuse the already-proved port-5 RMW to create an `SUD` 0-to-1 edge and
   request `ICC_ACTIVE`, preserving every other readable bit and keeping
   `ST=FRE=0`.
5. Poll `ICC` back to idle and `PxSSTS.DET=3` with a hard timeout.
6. Immediately clear the newly set `PxSERR` bits with one W1C write, require
   `PxSERR=0`, then read `PxTFD/SIG/SSTS` repeatedly for at most 32 seconds.
7. Never write `PxCI`, `PxIE`, `GHC.IE`, `PxCLB`, `PxFB`, `FRE`, `ST`, or PCI
   BME in this test.

Why this is the next boundary: the observed `0x04000000` in `PxSERR` is
`DIAG.X` (device-presence exchanged).  AHCI 1.3.1 section 10.1 states that
`DIAG.X` must be clear for the initial Register FIS to update `PxTFD`.  ICH10's
`PxCMD.FRE` definition also permits the first D2H Register FIS after
initialization even while FRE is zero.  Therefore:

- `TFD.BSY` clearing proves the no-DMA initial-FIS path and lets the next build
  proceed directly to SeaBIOS allocation/IDENTIFY;
- stable `TFD=80` after the bounded test proves that valid receive-FIS memory
  plus FRE is the next missing boundary, not MSI's CAP/hot-plug/ALPM policy.

Failure mode is loss of link, a non-clearing status bit, or timeout; record it
and stop.  The known recovery is the exact one-second AC interruption.  Since
the `PxCMD` write can also latch R/WO zeros, perform this only in a disposable
write-once epoch with the known AC-cycle recovery available, and do not use the
result to infer permanent connector policy.

For the following payload build, the cleaner architecture is to set
`GHC.AE`, write `PI=3f` before SeaBIOS, and then let unmodified SeaBIOS perform
its bounded HR, re-enable AE, allocate valid DMA structures, clear SERR, and
identify port 5.  No vendor `CAP`, `VSP`, `PxCMD`-policy, or indexed-table copy
is required for that first storage attempt.
