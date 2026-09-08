> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VD paired CSI-byte-2a6 High-QPI MINIT observation

Status: **BUILT TWICE / STATIC CONTRACT VERIFIED / HARDWARE RUN; ONE EXACT
HIGH-QPI MINIT RETURN AND TARGETED UC-DRAM TEST PASS**.

B06VD is the narrow successor to the four controlled B06VC G3 runs.  B06VC
observed a fifth pre-pass-3 CPU PCI `ff:02.1 + 0xa0` value, `00017c00`, and
two ABI-clean pass-3 CSI returns.  The returned 772-byte CSI states differed
from the earlier B06VA state at exactly byte `0x2a6`: B06VC returned `08`
with raw FNV `03e3d24e`; B06VA returned `0c` with raw FNV `8b38506a`.
Treating only byte `0x2a6` as zero in the hash stream produces canonical FNV
`908dabb6` for both states.  This is measured variability, not a decoded
register or state-field meaning.

## Deliberately narrow change

B06VD changes only two inherited decisions:

1. The two redundant pre-pass-3 predicates accept the exact finite A0 set
   `{00017000,00017600,00017800,00017a00,00017c00}`.  There is no mask,
   numeric range, or write to A0.
2. The two redundant post-CSI enforcement layers accept only these paired
   state variants:

   ```text
   state[0x2a6] = 08  and raw FNV = 03e3d24e
   state[0x2a6] = 0c  and raw FNV = 8b38506a
   canonical FNV with only 0x2a6 zeroed = 908dabb6
   ```

The raw state buffer is never modified.  Romstage selects a fixed compiled
raw digest only after the exact byte/digest pair and canonical digest pass;
that selected constant is then supplied to `x58_vendor_accept_csi_result()`.
The runtime digest cannot authorize itself.

Every other B06VB/B06VC check remains mandatory: wrapper/module signatures,
CAR canaries, ABI registers, return stack and state pointers, all sparse state
bytes, I801 and CMOS one-shot guards, policy construction, and the complete
post-CSI platform endpoint.  In particular, post-CSI still requires:

```text
CPU ff:02.1 6c = 0040a0a8
CPU ff:02.1 9c = 00b00502
CPU ff:02.1 a0 = 00017000
IOH 00:10.0 c8 = 0616fc00
IOH stage 9c   = ea000000
MC  ff:03.4 50/54 = 0a000006/00000006
```

Therefore the B06VC run-2 endpoint `A0=00017a00 / 9c=00a00502` still fails
closed before MINIT, even though its CSI state matches the new `08` variant.

## Identity and artifacts

```text
image ID: X58PROE-B06VD-HIGHQPI-CSI2A6-MINIT-ROMMON-20260905
config: configs/x58-pro-e-b06vd.config
config SHA-256: 37b7c38d7e542b4766efcbf8f5a6fc6f038cd434414fc71ea1c53127a56185c1
fixed SOURCE_DATE_EPOCH: 1788523200
payload: none
terminal state: CAR ROMMON
COM1: 0x3f8, 115200 8N1, no flow control
```

The redistributable coreboot-only 4-MiB base has SHA-256
`ea498ad961c8569f9a13598e1abb2bd5d9a685c2a8f05f44cb842ba2908032c1`.
The local compositions and proprietary provenance are recorded in the
[B06VD manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

Two ccache-disabled fixed-epoch builds were byte-identical.  A fresh B06VC
regression build remained byte-exact to its published base SHA-256
`398c631b6bf68b2bfb9c344c05b3adc477c6ea67e58093efadc2e5ec1c241b1d`.
The repository tests passed 98/98 and the vendor-blob, ROMMON-script, and
RAM-loader support suites passed 28/28.  An independent source audit repeated
40 focused predecessor/variant tests and found no blocker.  These are
static/build results, not hardware validation.

## Expected hardware branches

Use the same fixed E5645, microcode `0x1f`, one-DIMM SPD-`0x54`, DDR-ratio-6
configuration as B06VC.  Capture COM1 before removing G3 power.  If the
persistent guard is `0xec`, use ROMMON `unlock RESET` and `autoguard clear`
and verify that CMOS `0x0e` returns to `0x2c` before the controlled cold run.

Pass 1, pass 2, both expected reset continuations, and pass-3 selection remain
unchanged.  After pass-3 CSI returns:

- any pair/canonical/ABI/platform mismatch must stop in CAR before MINIT;
- POST `d3` means only that the one-shot MINIT call was entered;
- POST `d4` means only that MINIT returned;
- a returned MINIT workspace and telemetry remain observational.

Neither `d3` nor `d4` proves trained memory.  B06VD never accesses ordinary
DRAM, never records a DRAM handoff, and cannot enter postcar, ramstage,
graphics initialization, or a payload.  Manual vendor calls remain locked;
the existing read-only policy/runtime inspection stays available.

## Hardware result

Two controlled G3 sequences were captured on 2026-09-05.  The first stopped
fail-closed before MINIT on a nonmatching pass-three endpoint.  The second
matched the `2a6=08`, raw-FNV `03e3d24e`, canonical-FNV `908dabb6` CSI state,
entered and returned from MINIT, and produced workspace raw FNV `94299f43`
with completion bytes `01=00`, `02=00`, `4f=02`, and `e79=01`.

While B06VD remained in CAR ROMMON, reversible uncached tests passed at all 14
selected addresses from 16 through 40 MiB and for every D0--D31 walking-one
data bit.  Exact rollback restored the original words; the High-QPI and
selected memory-controller state remained unchanged and six sampled RAS/ECC
dwords remained zero.  This is strong evidence for the tested aperture, not a
full-capacity or repeated cold-boot validation.  The immutable record and raw
capture hashes are in the [bring-up log](bringup-log.md) and
[test matrix](test-matrix.md).

## Failure and recovery boundary

Pass-3 CSI and MINIT are proprietary early-init routines and may reset, hang,
or fail to return.  The complemented I801 marker plus CMOS `0x0e=ec` prevents
an unexpected MINIT reset from repeating the call automatically.  Preserve
the full serial stream, POST trace, exact reset provenance, image hash, and a
programmer read-back when available.  Recovery is external restoration of the
verified vendor image to the socketed W25Q128.V..M.  A single returned run is
not milestone validation; no DRAM-success claim is permitted without the
specified memory tests and repeated controlled cold boots.
