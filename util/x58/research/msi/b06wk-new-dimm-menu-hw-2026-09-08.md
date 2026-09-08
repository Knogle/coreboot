> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WK-HW10: requested SeaBIOS menu, different DIMM rejected

## Request, scope and outcome

The operator reports an active system with a different single4-GiB DDR3
module and requests the SeaBIOS ESC menu. Initial passive COM1 attachment
instead finds the CAR ROMMON. Read-only status and SPD commands establish
that the current module is outside the build's exact supported profile.
**SeaBIOS/menu not reached. No reset or guard bypass performed.**

The leading boot trace was not captured, so the precise original automatic
fallback reason cannot be reconstructed from the suffix alone. Initial
`vinfo` has CSI/MINIT attempted0 and no prior captured SPD digest; that
default FAIL is not by itself proof the automatic path reached the SPD
check. The subsequent explicit `spd` command independently demonstrates
that the present DIMM cannot pass the active automatic profile.

```text
test ID: B06WK-HW10 (live diagnostic attachment, not a new boot)
build commit: intended a2eb375438c85cd7908140636cbee8407bee8c0d plus WK delta
ROM hash: intended bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0; no flash readback
flash chip: inherited W25Q128.V..M16MiB
board revision: MSI X58 Pro-E/MS-7522; PCB revision not restamped
CPU: inherited E5649; liveCPUID206c2
CPU stepping: live2
microcode revision: live0000001f
DIMM model: changed by operator; part number not obtained; base SPD describes4GiB1Rx8,4-Gbit devices,row16/col10
DIMM slot: live responding address54; physical label not restamped; complete topology scan skipped after header rejection
GPU: inherited HD5450
PSU: unchanged; model not restamped
boot type: existing operator-started state; no agent reset or new cold/warm count
POST trace: no independent card/fullboot trace; SPD query resultb8
serial: operator@serial-gateway.example.invalid /dev/ttyUSB1,1152008N1; no competing opener in preflight
serial logs: captures/b06wk-menu-new-dimm-{initial,vinfo,spd,status}-20260908-10.raw
result: BLOCKED BEFORE PAYLOAD; respondingROMMON; newDIMM outside exactSPD/profile contract
recovery required: previous supportedDIMM or separately authorized new-profile implementation
notes: onlyvinfo,spd,id,resetcause,vinputs; no DDR/QPI/MSR tuning, unlock, blobcall, CMOSdatawrite or firmware/media edit
```

## Actual readbacks

- Initial passive capture20:34:20.442–20.946UTC,127bytes, normalclose,
  no replay indication; only end of CAR_PREPARE and `rommon>` retained.
- `id` confirms `X58PROE-B06WK-ACPI-REPAIR-20260908`.
- `vinfo` confirms wrapper/canary/runtime ready, CSI/MINIT attempted0,
  no generated policy and no manual vendor mutation.
- `vinputs`: CMOS0E=2c, validcoldauthorization; no cookie clear is needed
  merely on the evidence of this current value. Initial entry signature
  and the original fallback line remain unavailable.
- `spd`:128 unique bytes read,0 verification bytes, base CRCbea5 matches.
  Header92 declares176used bytes in256byte EEPROM. The strict code expects
  header93 and stops withHEADER_BAD/resultb8 before upper data/topology.
- The printed footer saying full256bytes were read twice is misleading in
  this rejection path; counters show only128bytes. Do not claim completeSPD
  or identify the module part number from unread upper bytes.
- `SPD54_TARGET_GATE=FAIL`, FULL256_FNV1A=0 is unset, not a measured zero
  hash. Expected profile remains BLS4G3D1609DS1S00./CRCec4f/4GiB2Rx8;
  automatic code additionally requires full256FNVfb66b530.

Relevant newSPD bytes:4=04,5=21,7=01,8=03. Applying the existing coreboot
DDR3 decoder fields gives4-Gbit components,16rowbits,10columnbits,one x8
rank and64-bit module bus:4GiB total. The prior profile uses2-Gbit devices
and two ranks. Equal total capacity does not make the initialization policy
interchangeable. A valid CRC and a conventional header92 are not evidence
of a defective DIMM; this is the experiment's restrictive admission policy.

## Capacity versus physical address

The previous successfully initialized4-GiB configuration publishes roughly
3GiB low RAM plus1GiB at physical4–5GiB, with a3–4GiB PCI/MMIO hole and
smaller legacy/firmware reservations. The maximum address is not the amount
of installed storage. HW07 E820 explicitly reports the remapped region
and Linux allocates NODE_DATA within it. The previous high-RAM MTRR finding
therefore does not require more than one4-GiB DIMM.

This is the **previous initialized map**, not a new successful memory-map
measurement for the changed DIMM. No transition past CAR was performed here.

## Immutable artifacts

| Artifact suffix, common prefix `captures/b06wk-menu-new-dimm-` | SHA256 |
|---|---|
| initial-20260908-10.raw | 7bbd16e77fb7f81184076ffcb3105df25965875c42e697c1253e99ec9eb2d748 |
| initial-20260908-10.raw.metadata.json | 154829b42d7d06a0fdee54df6002e50fe31fa12bf42a891c5313196d1f19edc7 |
| vinfo-20260908-10.raw | 863069cd9f3c148007923cf47026480cddc630a1c7d97d86045f30ee6ae2c23d |
| spd-20260908-10.raw | df381a000d2321c5ff7155c0e25402a66877b36ca6841aa8c3759bf911853953 |
| status-20260908-10.raw | be5d7a2511e173ca632051e0d0fba90746ad566cd158a56c7f3a396e020a9021 |

The25menu-helper hosttests pass, but the helper is not launched against this
unsupported-DIMM state. No ESC or boot-device key was transmitted. Shelly
status was read once (outputtrue,69.9W); no switching request was issued.
All communication helpers closed normally and no capture remains active.

## Follow-up: request to force the SeaBIOS menu with the changed DIMM

The operator explicitly requests a forced menu entry. The existing runtime
is examined again, without a reset or another training attempt. Commands
`help`, `vinfo`, and argument-less `vprep` are sent under the exclusive
project lock. The last command is a rejection-path probe: both its compiled
High-QPI guard and its ordinary missing-argument path precede any vendor
operation. No unlock command is sent.

The actual response is:

```text
[VENDOR] ROMMON RUNTIME_BOOT=PASS SPD_GATE=FAIL ACKMAP=00 DDR3MAP=00 SPD_FNV=00000000 PCIEXBAR_STEP=PENDING POLICY_LOCAL=ABSENT
rommon> vprep
ERR B06VL broad/unsafe recovery ROMMON: manual vendor calls remain disabled
rommon>
```

CSI/MINIT attempted flags remain zero. The archived WK configuration enables
the unconditional manual-vendor-command rejection at `romstage.c:8406`.
`unlock VENDOR` cannot bypass that branch. There is no CAR `continue`, `go`
or generic function-call command; the register script language also has no
execution-transfer operation. `autoguard clear` only changes an eligible
CMOS recovery cookie, not the SPD gate or the current execution position.

Even a hypothetical first-check bypass would not produce a valid new-DIMM
handoff: `b06v6_handoff.h` and `.c` require the old SPD digest, channel-2 DOD
`0x2ac` and rank mask `0x03`. The current module is one-rank and its SPD CAS
mask also does not advertise the old CL5 target. These contracts must be
adapted to actual initialization results; fabricating old success values is
not support for the new module. Direct raw memory writes remain powerful,
so this finding is absence of an existing usable force/resume mechanism,
not a claim of absolute impossibility of separately engineered live code.

Outcome: **menu not reached; board left responding in CAR ROMMON**. A
new-profile firmware experiment or reinstalling the previously accepted DIMM
is needed for the next supported boot attempt. No firmware/media edit,
register mutation, reset, Shelly operation or new boot count occurred.
The inventory and recovery configuration are unchanged from HW10 above.

Immutable follow-up transcript:
`captures/b06wk-new-dimm-force-feasibility-20260908-11.raw`, SHA256
`e9396d35719045a274742099653f6809e15f13663613a9385c3237c6b4a85724`.
The filename suffix distinguishes this attachment; it is not an additional
cold/warm boot. Remote and local hashes agree. The command helper closed
normally, and the final remote `fuser /dev/ttyUSB1` check has no owner.
Current and archived WK `romstage.c` agree at SHA256
`22bab99e8df32452d7688a1a98bb3806035d2ac97e8806afe97044740c588822`.

## Later recheck: same model, different serial, not the HW10 one-rank DIMM

The operator requests another check and comparison of the current SPD serial
with historical runs, then confirms intentionally fitting an identical,
known-good replacement module for a Memtest comparison. New readbacks now
show the old supported **BLS4G3D1609DS1S00., 4-GiB 2Rx8** model again.
The earlier one-rank diagnosis describes HW10's previous module, not this
newly observed state.

Two separate explicit `spd` commands give identical complete 256-byte dumps.
A byte-by-byte comparison against the first complete HW07 SPD block finds
exactly one difference: byte125 (`0x7d`), within the module serial.
The individual serial values are redacted in this public derivative. The other255
bytes, including geometry, timing data and part number, are byte-identical.
DDR3 serial offsets122–125 are defined in coreboot's `device/dram/ddr3.h`.

| Field | Historical B06H/HW07 module | Current replacement |
|---|---|---|
| SPD serial, byte order122–125 | original identity redacted | replacement identity redacted |
| Full256 FNV-1a | `fb66b530` | `3f5e3f88` |
| Full256 CRC16 | `6d07` | `97d8` |
| Base CRC / upper128 CRC | `ec4f` / `a46c` | unchanged |
| Geometry and derived DDR3-800 timing candidate | 4GiB2Rx8, row15/col10,5-5-5-12 | unchanged |

Independent recomputation using the existing repository CRC/FNV functions
reproduces both complete hashes. Here the base CRC covers bytes0–116, not
the serial; its unchanged value is expected. B06H's older user-supplied base
dump identifies the same original module. The intermediate HW10 one-rank
module has a different private serial and is distinct from this comparison.

`vinfo` now reports `SPD_GATE=PASS`, topology ACKMAP/DD3MAP10 and
`SPD_FNV=3f5e3f88`; CSI/MINIT attempted flags are still0 and policy absent.
`vinputs` still reads validCMOS0E=2c. The build's additional automatic
comparison with `X58_B06V6_EXPECTED_SPD_FNV=fb66b530` at romstage7396 and
the repeated handoff digest contract would reject this replacement solely
because the full digest includes the serial. The first automatic fallback
message was not captured, so that is a verified admission-code explanation,
not an invented reconstruction of the earlier boot.

For this replacement the appropriate software change is a serial-independent
validated profile, or explicit admission of this second full digest through
all relevant handoff checks. There is no SPD evidence requiring different
rank/geometry/timing policy. No such change is implemented in this check,
no DIMM EEPROM data is written, and no initialized-RAM or Memtest result is
claimed for this new module. The user calls the replacement known-good; its
independent qualification was not performed by the agent.

### Recheck provenance and immutable logs

Existing board state only; no agent reboot/Shelly operation or new boot count.
Inventory is inherited from HW10 except the above replacement DIMM. `id`
again confirms WK, CPUID206c2; `vinfo` confirms microcode1f. COM1 remains
`/dev/ttyUSB1`,1152008N1. Commands are `id`, `spd`, `vinfo`, `vinputs`, `spd`.
No unlock, vendor call, generic register write, firmware or boot-media edit.
Normal SMBus address/control transactions are solely for EEPROM reads.

The initial passive10-second capture contains repeated `rommon> status`
text at an implausible UART byte rate and ends on its deadline despite a
seen prompt. That stream is archived but not counted as new boots or new
command responses. After archiving, the command helper flushes RX/TX queues;
the identity response follows repeated echo characters, while both SPD
command responses are complete and agree byte-for-byte. This separation is
important for freshness. Final `fuser` shows no UART owner; board remains
in CAR ROMMON, not the SeaBIOS menu.

Common artifact prefix `captures/b06wk-spd-serial-recheck-`:

| Suffix | SHA256 |
|---|---|
| initial-20260908-12.raw | `5ab90f7a4f9b705e88f5ef08a0be7a11373611fd86f907d2c94488818c484bba` |
| initial-20260908-12.raw.metadata.json | `201a2e37c75863416f2533aa21aab9b65dba01339767791cdea07edace01607c` |
| id-20260908-12.raw | `9a53f74ac377a49960bddb77c05697bbe8e98f576eb1e32813d13d24a879a910` |
| spd-20260908-12.raw | `5c44045ff3301dcf22ae21f334021baedb3c5cc573074583ab067bd9d582ceb3` |
| repeat-20260908-12.raw | `1c40ce25bd938bfbc8af5fa7a90c2ed19f188dbcf3b84f60e236d3d7ccba6928` |

Remote/local hashes match for all five artifacts. Suffix12 distinguishes
this diagnostic attachment, not a new cold/warm stability count.
