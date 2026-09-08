> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VF minimal SeaBIOS entry probe

Status: **BUILT TWICE / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED / FOUR
G3-LABELLED HARDWARE CAPTURES / SAFE PRE-MINIT FALLBACK / NO PAYLOAD ENTRY**.

The exact build, payload and local W25Q128 hashes are recorded in the
[B06VF image manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

B06VF is the first deliberately bounded attempt to continue from the exact
B06VE High-QPI promotion path into a coreboot payload.  Its success criterion is
only entry into a reduced SeaBIOS image with serial diagnostics.  Boot support,
graphics, storage and USB are intentionally absent.  Reaching SeaBIOS is not a
first operating-system boot and is not evidence that the unreported parts of
DRAM or the platform PCI topology are usable.

Like B06VE, this is a locally vendor-assisted research bridge.  The flashable
local composition uses hash-pinned MSI CSI/MINIT code; the repository does not
contain or redistribute those proprietary bytes.  B06VF is not native/open X58
memory initialization.

## Subsequent B06VF hardware result

Four retained G3-labelled captures completed CSI passes 1 and 2. Pass-3
pre-A0 was `17a00/17400/17400/17800`. The two `17400` runs stopped before
CSI because that newly observed literal was outside B06VF's allowlist. The
other two returned ABI-clean with `0/0/11`, preserved pre/post A0
`17a00->17a00` and `17800->17800`, CPU 9c `a00502`, and CSI
byte/raw/canonical `0c/8b38506a/908dabb6`. B06VF rejected both exact
coupled endpoints before MINIT because it still required post-A0 `17000`
and CPU 9c `b00502`.

Thus no B06VF run reached POST `d3/d4`, an ordinary DRAM access, postcar,
ramstage, the SELF loader, or SeaBIOS. The complete observation and immutable
capture hashes are appended to the
[bring-up log](bringup-log.md). B06VG supersedes the fixed post-CSI assumption
with two disjoint exact profiles; see the
[B06VG experiment](b06vg-coupled-minit-seabios.md).

## Architecture and one-way boundaries

The intended successful path is:

```text
reset
  -> inherited guarded three-pass High-QPI CSI sequence
  -> one guarded MINIT call
  -> exact post-MINIT raw-pair + canonical-workspace gate
  -> complete UC tests of 0..640 KiB, CBMEM and object windows
  -> digest-bound version-4 CBMEM handoff
  -> coreboot postcar and ramstage
  -> repeated CBMEM/handoff/UART/MTRR gate at BS_PRE_DEVICE
  -> static resource publication without a coreboot PCI scan
  -> coreboot tables
  -> X58 SAD identity, PAM read-back and 34-word shadow probe
  -> coreboot SELF loader places SeaBIOS at 000f8800..000fffff
  -> SeaBIOS entry 000fecd6 and COM1 diagnostics
```

Each arrow remains fallible.  A rejected automatic CSI/MINIT branch retains
the CAR recovery ROMMON.  A post-memory, PAM or shadow failure stops with a
specific POST code instead of continuing into the payload.  The persistent
MINIT guard is not finalized until the complete memory tests and version-4
handoff read-back have passed.  The PAM change occurs later and is volatile;
it does not modify SPI flash.

## Exact promotion gates

All B06VE gates remain prerequisites.  The narrow supported configuration is:

```text
CPU:                         CPUID 000206c2, microcode 0000001f
SPD:                         sole responder 0x54, FNV fb66b530
CSI state byte 0x2a6:        08
CSI raw/canonical FNV:       03e3d24e / 908dabb6
MINIT policy FNV:            3c0f3a0b before/current/direct
MINIT EAX:                   00000000
QPI status:                  070f0f03
post-MINIT IOH stage +0x9c:  bf000000
MC mapper/common:            00024489 / 00001545
channel-2 DOD/ranks/status:  000002ac / 00000003 / 00000140
```

Wrapper and module signatures, CAR canaries, ABI registers, stack and pointer
checks, I801 and CMOS phase markers, exact PCIEXBAR/X58 identities, policy
bytes, memory-clock registers, completion bytes and the existing finite QPI
state allowlists must also match.  No runtime digest is allowed to authorize
itself.

### Two admitted workspace observations

B06VF promotes two observed High-QPI MINIT workspace forms.  Each raw FNV is
coupled to its own exact byte tuple; components from the two rows cannot be
mixed:

| Variant | Raw workspace FNV | `workspace[2459..245c]` | `workspace[2461]` | Canonical FNV |
| --- | --- | --- | --- | --- |
| A | `94299f43` | `00 00 00 00` | `fe` | `a6f9c2e6` |
| B | `b6346533` | `ff ff ff ff` | `ff` | `a6f9c2e6` |

The raw workspace is never edited.  Canonicalization feeds zero instead of
the bytes in the exact compiled dynamic ranges
`132c..136d`, `13be..13fd`, `1859..185c`, `18e7..18e9`, `1aa4..1aa7`,
`23a6`, `2411..2434`, `2459..245c`, `2461`, `24a1..24c4` and
`251f..252e` into FNV-1a.  Both an admitted raw pair and canonical FNV
`a6f9c2e6` are required.  Variant B may admit the predecessor probe's single
`PLATFORM_STATE` result only after its pair and canonical checks pass; the
individual post-MINIT platform registers are still exact-gated independently.

## Version-4 handoff and memory proof

The B06VF handoff retains magic `X6HO`, has wire size 144 bytes and requires
version 4.  Its flags must equal `0x7f`, not merely contain a subset:

| Bit | Required evidence |
| ---: | --- |
| 0 | exact admitted MINIT result |
| 1 | exact pre-postcar MTRR frame |
| 2 | CBMEM window tested |
| 3 | object window tested |
| 4 | CBMEM created and handoff readable |
| 5 | exact B06VE High-QPI post-MINIT endpoint |
| 6 | complete B06VF conventional-low-memory test |

The structure also binds the complete exact result, MTRR values, region bases,
eight zero reserved dwords and its own FNV-1a digest.  The producer validates
the record after writing it.  Ramstage checks the exact CBMEM region, record
location, digest, UART state and live postcar MTRRs before it returns from the
otherwise terminal DRAM-ROMMON callback.  At the payload boundary, B06VF again
requires CBMEM online, verifies that the actual used region is contained in
the tested `01000000..017fffff` window and ends at `01800000`, bounds the
handoff pointer inside that region, and then checks its exact content, digest
and low-memory-test flag before touching SAD/PAM.

While DRAM still uses the measured default-UC MTRR frame, B06VF tests every
aligned dword in `00000000..0009ffff` with an address-derived pattern, its
inverse, and finally zero plus read-back.  This is a destructive full test of
the entire 640-KiB conventional-memory range; it is not a sample.  Only after
that pass does the inherited path run its simultaneous 14-address alias test
and the complete pattern/inverse/clear tests of:

```text
CBMEM: 01000000..017fffff
object: 02000000..027fffff
```

These checks prove only the stated apertures for this transition.  They do not
test the complete DIMM, cache coherency, retention, DMA or repeated cold-boot
stability.

## Conservative resource model

B06VF publishes only ranges backed by the tests above:

| Range | Coreboot resource type | Purpose |
| --- | --- | --- |
| `00000000..0009ffff` | RAM | SeaBIOS low memory, stack and legacy data |
| `000a0000..000bffff` | MMIO/reserved | legacy VGA aperture; not initialized |
| `000c0000..000fffff` | reserved RAM | PAM-backed option-ROM/BIOS shadow |
| `01000000..017fffff` | RAM | tested CBMEM/postcar/ramstage region |
| `02000000..027fffff` | RAM | tested object/high allocation region |

All gaps and all remaining installed DIMM capacity stay unreported.  Normal
CBMEM bookkeeping can further reserve used portions of the first 8-MiB
window in the generated coreboot memory table.  The x86 SELF-loader exception
permits the SeaBIOS segment below 1 MiB even though the C--F shadow aperture is
reported as reserved rather than general-purpose RAM.

The static domain and CPU-cluster operations deliberately have no `scan_bus`
method.  Consequently coreboot does not enumerate PCI, assign device BARs,
initialize the GPU, or enable storage before entering SeaBIOS.

## PAM and the 34-word shadow transaction

At `BS_PAYLOAD_LOAD` entry, after coreboot tables have been written, B06VF:

1. revalidates the exact handoff and low-memory flag;
2. requires SAD device `ff:00.1` to identify as `8086:2d81`;
3. saves PAM bytes `0x40..0x46`;
4. writes and reads back `30/33/33/33/33/33/33`;
5. tests 34 distinct aligned dwords in `000c0000..000fffff`;
6. restores and verifies all 34 original dwords in reverse order;
7. on success, leaves PAM read/write enabled for the SELF loader.

The 34 simultaneous words consist of the first and last dword of each of the
sixteen 16-KiB blocks in the 256-KiB C--F aperture, plus exact special points:

```text
000f8800  first byte/dword of the built SeaBIOS PT_LOAD segment
000fecd4  aligned dword containing entry point 000fecd6
```

Every word receives a distinct address- and index-derived pattern.  All
patterns remain resident until every read-back completes, so aliases between
the sampled blocks or either payload-specific point cause failure.  Restoration
is attempted even after a pattern mismatch and its read-back participates in
the result.  A failed shadow test also restores all seven original PAM bytes;
a failed PAM restoration is itself terminal.

The current reduced SeaBIOS ELF has one RWE `PT_LOAD` segment with physical and
virtual range `000f8800..000fffff`, size `0x7800`, and entry `000fecd6`.  Those
addresses are structural facts from the ELF, not evidence that B06VF has loaded
or executed it on hardware.

## Hardware prerequisite evidence from B06VE

The PAM hypothesis was tested interactively from B06VE's post-MINIT **CAR
recovery ROMMON** before being encoded into B06VF.  B06VE rejected the newly
observed raw workspace before its post-memory path, so it did not reach
postcar or the DRAM ROMMON.  This is real hardware evidence, but it must not be
confused with a B06VF execution.

In the B06VE run-3 session, PAM `ff:00.1 + 0x40..0x46` initially read
`00/00/00/00/00/00/00`.  Transactional scripts changed it to
`30/33/33/33/33/33/33`, simultaneously wrote and verified distinct patterns
at representative 16-KiB points spanning `000c0000..000fffff`, and performed
verified reverse rollback.  A final payload-specific script separately kept
patterns at `000f8800` and `000fecd4` resident together and returned
`RUN=ok`.  Its transaction FNV was `68a06b96`; reverse rollback returned both
original words, restored all PAM bytes to zero and produced transaction FNV
`6e389292`; transaction discard also returned `DISCARD=ok`.

The immutable private capture metadata for that final script is:

```text
718de5a02c0776c20df10e77aefacf37b9b2d57bcf013fb604762815dc6d2a08  x58-b06ve-run3-b06vf-seabios-exact-run.raw      (6844 bytes)
b8798c52aa355722cff2c9d7bdeb306d54abce57d35662a048372a89051daaca  x58-b06ve-run3-b06vf-seabios-exact-rollback.raw (4772 bytes)
1a735dc6fda14c89dfcd3162872eec5647b549d8f47fa9389017139f2cbb2693  x58-b06ve-run3-b06vf-seabios-exact-discard.raw   (553 bytes)
```

This establishes that the selected PAM values and representative C--F shadow
locations, including the exact new SeaBIOS load/entry words, were reversible
on that B06VE hardware state.  It does **not** validate B06VF's automatic
bootstate ordering, complete 34-word transaction, SELF loading or SeaBIOS
execution.  Those remain the purpose of the first B06VF hardware run.

## Deliberately minimal SeaBIOS

The payload uses SeaBIOS `rel-1.17.0` in coreboot mode with debug level 9 on
COM1 `0x3f8`.  The board-local configuration disables threads, init-code
relocation, upper-memory malloc, drives, USB, normal serial/sercon services,
LPT, PM timer, PCI BIOS configuration, APM/PNP BIOS, option ROMs, boot support,
keyboard, mouse, S3 and VGA hooks.  Coreboot likewise disables VGA-ROM
execution, graphics initialization and SMBIOS generation.  The payload is a
serial entry probe, not a usable SeaBIOS boot configuration.

These reductions do not make SeaBIOS passive.  In coreboot mode it still runs
`pci_probe_devices()` to build its internal list from PCI configuration reads.
Because this is a non-QEMU build, `pci_setup()` returns without assigning PCI
resources; disabling `PCIBIOS` separately removes the legacy PCI BIOS service.
The read-only discovery can nevertheless expose an incomplete or unstable
bus.  SeaBIOS also performs legacy I/O writes during normal POST, including
DMA reset/setup, PIC setup, PIT/timer setup and RTC/CMOS status/reset handling.
Therefore the first SeaBIOS run may hang or disturb legacy-controller state
even though GPU, storage, USB and option-ROM paths are disabled.

If it completes its intentionally non-booting POST, the expected terminal
message is:

```text
Boot support not compiled in.
```

## B06VF POST contract

The B06VF-specific ramstage/payload codes are:

| POST | Meaning |
| --- | --- |
| `21` | exact ramstage handoff accepted; leave the DRAM-ROMMON callback |
| `22` | conservative domain resources published |
| `23` | coreboot tables written |
| `24` | CBMEM/handoff/low-memory gate failure at payload boundary |
| `25` | SAD `ff:00.1` ID mismatch |
| `26` | PAM programming/read-back or PAM rollback failure |
| `27` | 34-word shadow pattern/alias/restore failure |
| `28` | C--F shadow verified and left read/write for SELF loading |
| `29` | SeaBIOS SELF image returned successfully from the loader |
| `2a` | immediately before coreboot enters SeaBIOS |

Codes `24..27` are terminal fail-closed stops.  Codes `21`, `22`, `23`,
`28`, `29` and `2a` can be brief.  Coreboot writes its generic bootstate POST
after entry callbacks, so in particular `21` is followed by `70`, `28` by
`7a`, and `2a` by `7b`; `29` is also quickly followed by the payload-boot
entry.  A board stuck immediately inside `payload_load()` will normally show
`7a`, and one stuck immediately after the SeaBIOS call will normally show
`7b`, not the preceding B06VF-specific code.  Preserve the full POST trace and
serial stream rather than recording only the final display.

The inherited pre-payload path retains B06VE's earlier `d3/d4`, `08..0f`,
`14..20` and postcar codes.  In B06VF, POST `0a` lasts longer because the
complete 640-KiB UC test precedes the two inherited 8-MiB tests.

## Expected serial sequence

Additional debug lines may appear, but a successful run should preserve this
ordered subsequence:

```text
[RAMINIT] B06VF RETURN POLICY_IN/NOW=...
[RAMINIT] B06VF WORK_CANON=a6f9c2e6 ... RAW_PATTERN_GATE=01 ...
[RAMINIT] B06VF EXACT WORK_RAW=94299f43 ...
    or:  [RAMINIT] B06VF EXACT WORK_RAW=b6346533 ...
[RAMINIT] B06VF_AUTO_HANDOFF=READY; canonical High-QPI workspace; ...
[RAMINIT] B06VF canonical High-QPI payload handoff entry; DRAM remains UC
[RAMINIT] B06VF UC lowmem full test PASS 00000000..0009ffff; range cleared
[RAMINIT] UC alias+full test PASS CBMEM=01000000..017fffff OBJECT=02000000..027fffff; windows cleared
[RAMINIT] B06VF persistent guard finalized after v4 CBMEM readback
[RAMINIT] CBMEM READY ...; entering postcar

[RAMSTAGE] X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905
[RAMSTAGE] exact handoff valid; CBMEM is WB, payload path remains fail-closed
[PAYLOAD] B06VF handoff accepted; leaving DRAM ROMMON callback
[PAYLOAD] conservative RAM map: 0-640K, 16-24MiB, 32-40MiB
[PAYLOAD] coreboot tables written
[PAYLOAD] PAM PRE=...
[PAYLOAD] PAM POST=30/33/33/33/33/33/33
[PAYLOAD] C0000-FFFFF PAM decode verified; left read/write for SELF loader
[PAYLOAD] SeaBIOS SELF image loaded
[PAYLOAD] entering SeaBIOS payload

SeaBIOS (version rel-1.17.0-0-gb52ca86e)
...
Found coreboot table forwarder.
...
PCI probe
...
Boot support not compiled in.
```

The first `SeaBIOS (version ...)` line is independent proof that entry
`000fecd6` executed and SeaBIOS initialized its debug UART.  Finding the
coreboot table forwarder is a later milestone.  The final non-booting message
proves substantially more SeaBIOS POST code ran, but still does not establish
working devices or a boot environment.

## First hardware procedure

Use only the configuration that produced the admitted B06VE workspaces and
the PAM prerequisite logs:

```text
board: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM: BLS4G3D1609DS1S00, sole SPD responder 0x54, FNV fb66b530
memory ratio: 6
initial QPI: Slow; inherited sequence promotes it to the exact High-QPI state
UART: COM1 03f8, 115200 8N1, no flow control
flash: spare socketed W25Q128.V..M, complete 16-MiB local composition
start: controlled true G3/AC-cold boot
```

Before applying power:

1. preserve a verified vendor image and a known-good recovery chip;
2. verify the full programmed/read-back 16-MiB chip as
   `7dfec8e9d93ffcb2b855dc111c5b4aed789d8a59755eea90700e87156b65317a`;
3. disconnect unnecessary storage and USB devices and do not expect VGA
   output;
4. start raw COM1 capture and POST-code capture before AC is applied;
5. allow the two CSI-produced reset continuations to occur without issuing a
   manual reset or sending ROMMON input.

Record the exact workspace row, complete POST trace, complete serial bytes,
reset provenance, terminal power state and programmer read-back.  Stop after
the first attempt for review even if SeaBIOS prints its terminal message.  One
successful entry is milestone evidence, not validation; repetition begins only
after the first trace has been audited.

## Failure and recovery

Do not loop automatic resets after a failure.  If the path falls back to the
CAR ROMMON, capture `vinfo` and the complete rejection reason before changing
state.  If the persistent guard reports `ec`/`ed`, use the documented guarded
`unlock RESET` plus `autoguard clear` procedure, verify return to CMOS
`0x0e=2c`, then remove AC completely before another attempt.

For a halt, lost serial, POST `14..19` or `24..27`, a reset storm, or a hang
after `7a/7b`, remove AC power completely.  PAM and the tested shadow contents
are volatile.  Recover with the externally verified vendor image or the
retained known-good B06VE image on the spare socketed chip; do not depend on
SeaBIOS, GPU output, storage or network recovery.  See
[flash recovery](flash-recovery.md), the
[B06VE handoff procedure](b06ve-highqpi-auto-handoff.md), and the
[register-script safety model](b06v1-register-scripting.md).

## Claim boundary

The captured B06VF traces prove only the inherited three-pass CSI path and
safe pre-MINIT rejection described above. All post-MINIT, DRAM, ramstage and
payload behavior in this document remains a source/build contract. The B06VE
PAM sessions prove only the stated prerequisites. B06VF currently provides
none of the following:

- coreboot PCI enumeration or BAR allocation;
- GPU initialization or graphical output;
- SATA, USB, NIC, TFTP or SSH operation;
- option-ROM execution;
- a SeaBIOS boot menu or boot-device path;
- ACPI/SMBIOS completeness;
- full-DIMM or repeated cold-boot validation;
- native/open X58 CSI, QPI or DDR3 initialization.

The payload claim would still require an independently captured exact
PRIMARY run proving that the guarded vendor-assisted X58 state survived
coreboot's tested-memory transition, coreboot produced the conservative
tables, the PAM shadow accepted the SELF image, and control reached the
minimal SeaBIOS payload over serial.
