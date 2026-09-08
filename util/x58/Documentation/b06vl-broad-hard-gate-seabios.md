> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VL broad hard-gated SeaBIOS experiment

Status: **BUILT REPEATEDLY / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED /
LOCAL COMPOSITE VERIFIED / HARDWARE EXECUTED ONCE THROUGH REDUCED SEABIOS**.

B06VL is a separate, default-off successor to B06VK.  It deliberately stops
using an exact MINIT-workspace profile as the admission key.  Instead, it
admits the finite cross-product of seven explicitly observed CPU A0 values,
two observed CPU `9c` values and two exact CSI representations to one
persistent-guarded MINIT call.  A return can advance only through a new
version-9 producer/consumer contract after independent hard state checks and
bounded destructive DRAM tests.

This is the intentionally bolder experiment requested after B06VK repeatedly
stopped on individually familiar but crossed CPU/CSI combinations.  It is not
a claim that the A0 values form a meaningful numeric range, that an arbitrary
intermediate value is safe, or that workspace variation is understood.

## Evidence and bounded expansion

B06VK hardware observations included:

```text
17000 / b00502 / CSI 0c / raw 8b38506a
17600 / b00502 / CSI 08 / raw 03e3d24e
17a00 / a00502 / CSI 08 / raw 03e3d24e
17200 stable before CSI, rejected solely by the old six-value list
```

B06VL names exactly these seven A0 values in a switch:

```text
00017000  00017200  00017400  00017600
00017800  00017a00  00017c00
```

There is no range or mask predicate.  Candidate admission is the complete
seven-by-two-by-two cross-product:

```text
CPU A0:   one of the seven literals above; saved/pre/post reads must agree
CPU 9c:   00a00502 or 00b00502
CSI 2a6:  08 iff raw digest 03e3d24e
          0c iff raw digest 8b38506a
CSI canonical digest: 908dabb6
```

That makes 28 explicitly test-covered tuples.  Workspace raw and canonical
digests are no longer profile selectors.  They remain computed, logged,
stored in the handoff, recomputed for local self-consistency, and covered by
the handoff integrity digest; they are telemetry, not known-good-state proof.

## Before the one MINIT call

The broad class suppresses the inherited observation-only terminal only when
all of the following remain exact:

- two stable pre-call CPU A0 reads, with one of the seven literal values;
- CPU `9c` equal to `00a00502` or `00b00502`;
- one of the two exact CSI byte/raw-digest pairs above;
- CSI canonical digest `908dabb6`, exact CSI ABI, and existing marker helper;
- the common measured High-QPI, IOH and memory-clock endpoint;
- the existing CPU/microcode, SPD, policy, MTRR, canary and vendor-image gates;
- the persistent one-shot CMOS guard and exact I801 pre-call state.

The serial trace labels this path `BROAD_UNSAFE`, explicitly says `not
trained`, writes POST `d3`, and authorizes exactly one MINIT call.  A reset,
hang, or return into the CAR recovery ROMMON remains a valid failure outcome.
Manual vendor calls remain disabled.

## After MINIT returns

An `EAX=0` return is necessary but insufficient.  Promotion requires the
complete independent return contract, including:

- clean call-frame ABI, signatures and canaries;
- stable saved/pre/post CPU A0 and `9c` values;
- exact CSI raw/canonical pairing and policy consistency;
- MINIT result self-consistency and locally recomputed workspace digest;
- universal completion bytes `[1]=00`, `[2]=00`, `[4f]=02`, `[e79]=01`;
- exact I801 return tuple and CMOS guard state;
- exact common QPI/IOH/memory-clock endpoint;
- expected memory-controller and SPD state;
- the existing MTRR contract.

For B06VL only, the existing bounded probe may report success or the already
understood platform-state status because the relevant platform fields are
checked independently.  Neither fixed workspace hash nor an O/Q workspace
marker pattern is accepted as a substitute for the hard gates above.  The
first positive message is consequently `HARD_RETURN_GATE=PASS;
DRAM_NOT_YET_PROVEN`, not a training-success claim.

## DRAM, handoff and payload boundary

Every B06VL profile is forced onto the deferred-I801 path.  After the hard
return gate, DRAM remains uncached while the inherited bounded validation does:

```text
complete 0..640-KiB destructive test and clear
transactional 14-address alias test and rollback
complete 8-MiB CBMEM-window test and clear
complete 8-MiB object-window test and clear
CBMEM creation and exact version-9 handoff read-back
```

Only after every test, CBMEM operation and handoff read-back succeeds may the
code re-arm the measured I801 cookie, finalize the persistent guard, enter
postcar, relocate ramstage, and call the reduced SeaBIOS payload.

The handoff is deliberately incompatible with B06VK consumers:

```text
magic:       X6HO
version:     9
size:        160 bytes
profile:     7 (BROAD)
required:    X58_B06VL_HANDOFF_BROAD_POST_MINIT, bit 7
CSI variant: reconstructed from the exact CSI raw digest, never inferred
             from deferred-rearm state
```

With the B06VL option disabled, executable source-contract tests preserve the
B06VK version-8/profile-Q behavior and reject A0 `17200`.

The payload is SeaBIOS rel-1.17.0 in the existing reduced serial-only,
deliberately non-booting configuration.  Success means reaching its COM1
terminal `Boot support not compiled in.`  B06VL does not add VGA, option-ROM,
storage, USB, NIC, TFTP, SSH, or operating-system boot support.

## Artifact identity

```text
image ID: X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905
mainboard part: X58 Pro-E B06VL broad hard gate SeaBIOS probe
coreboot base: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
config: configs/x58-pro-e-b06vl.config
config SHA-256: a30296218fd128ade0c198424bf4b125188a9760460bff2eb84822d1c42f6ebf
generated .config SHA-256: 8f8d3845fba3146eac28fe2b541c0163d47164cab764db22b4cda03edbe38054
fixed SOURCE_DATE_EPOCH: 1788602400
payload: SeaBIOS rel-1.17.0, b52ca86e094d19b58e2304417787e96b940e39c6
COM1: 03f8, 115200 8N1, no flow control
W25Q128 placement: 4-MiB composite at 0x00c00000..0x00ffffff
```

Released and local-only artifact hashes:

```text
dd9567411ccd4abdbe5b3efdaadef63d1192cef20d3dbfa421c9962df05c7a58  builds/experimental/msi-x58-pro-e-b06vl-coreboot-base-4MiB.rom
330bba12fe59a7d1e6605d72a38e051bf519a6655d48d84c1e15e97e4dba8eb0  blobs-local/msi-x58-pro-e/b06vl/msi-x58-pro-e-b06vl-unpatched-4MiB.rom
c5df9648d408f9c7985179c4ad81a663ff1bf357a3c560d08f831fdcd6efec36  blobs-local/msi-x58-pro-e/b06vl/msi-x58-pro-e-b06vl-deterministic-4MiB.rom
9f8e1085d09207073983bbb3e26ce51474ced604e020a4206406e1a485c3f5e7  blobs-local/msi-x58-pro-e/b06vl/msi-x58-pro-e-b06vl-deterministic-w25q128-16MiB.rom
```

The public base contains no MSI CSI/MINIT bytes.  The local 16-MiB candidate
has an erased lower 12 MiB and the deterministic composite at the flash top.
No B06VL image has been programmed, read back from a programmer, or executed
on hardware as part of this build task.

## Validation and reproduction

- full Python suite: 178/178 passed;
- focused B06VI/B06VJ/B06VK/B06VL contracts: 29/29 passed;
- the executable truth table covers all 28 broad tuples, representative
  arbitrary workspace digests, wrong CSI pairings, unequal A0 reads, unknown
  A0/9c values, wrong canonical state, version/profile/flag mutations, and the
  disabled-option B06VK regression;
- three ccache-disabled clean builds at the fixed epoch produced byte-identical
  public and local artifacts;
- CBFS, payload ELF entry and loaded segment, local composition, seven-byte
  wrapper patch, erased lower region, and W25Q128 top placement passed.

Reproduce with the hash-pinned, user-supplied MSI image kept outside the public
tree:

```bash
./scripts/build_x58_b06vl.sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

The helper fails closed if an existing released artifact differs.  Before any
hardware attempt, compare the complete programmer read-back against SHA-256
`9f8e1085d09207073983bbb3e26ce51474ced604e020a4206406e1a485c3f5e7` and
retain the socketed-flash recovery path.  The established target remains the
E5645/CPUID-206c2 stepping-2 system with microcode `0x1f`, one SPD-0x54
BLS4G3D1609DS1S00. 4-GiB dual-rank x8 DIMM at ratio 6, and unchanged GPU/PSU.

No automatic Shelly cycle is recommended as a G3 test.  Earlier one-second
interruptions did not establish G3.  The controlled G3 observations used
15- or 60-second AC removal, after which the board remained in S5 and required
a manual power-button start.  No power action was invoked while producing this
image.

## Post-build hardware result

The original build-only validation record below is preserved as release-time
provenance. B06VL was
subsequently executed once on 2026-09-05 as `B06VL-HW-02`. The retained trace
proved the complete gated path: three CSI phases, guarded MINIT return with
EAX zero, hard-return gate, complete uncached low-memory/alias/two-window
tests, version-9 handoff, postcar, DRAM-backed ramstage, coreboot tables, PAM
decode, SeaBIOS SELF load, and execution of SeaBIOS rel-1.17.0.

SeaBIOS found the coreboot table, memory map, CBMEM console and mainboard
identity; its raw bus-0 probe reported 65 functions and maximum bus 0. It then
reached the reduced payload's intentional `Boot support not compiled in.`
terminal. This does not establish downstream PCI bridge enumeration, GPU,
graphics, storage, network, an OS boot, confirmed electrical G3, or repetition.

The normalized 602-line transcript has SHA-256
`61bcab5ad23e4f8a5efa4899ee85c061acaeffb6ea9cb35233653f413f859a93`.
See the complete [B06VL-HW-02 evidence report](../research/msi/b06vl-coreboot-seabios-hw-02.md)
for immutable fragment hashes and the exact evidence index.

See the [B06VL manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
for the complete artifact, CBFS and proprietary-source provenance.
