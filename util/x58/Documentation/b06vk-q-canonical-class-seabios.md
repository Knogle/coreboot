> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VK profile-Q canonical-class SeaBIOS experiment

Status: **BUILT REPEATEDLY / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED /
LOCAL COMPOSITE VERIFIED / HARDWARE EXECUTED 7 TIMES / NO DRAM OR PAYLOAD
ENTRY**.

B06VK is the narrow successor to B06VJ. It changes only profile Q, wire ID 6:
the raw `0x2bcc`-byte MINIT-workspace FNV remains calculated, printed and
integrity-covered telemetry, but is no longer an acceptance key. Q still
requires canonical workspace FNV `0b161f01`, all 26 exact marker bytes, the
four universal completion bytes, and every existing CPU, CSI, policy, I801,
CMOS, MTRR, QPI, IOH and memory-controller gate. PRIMARY A/B/P/N and profile O
continue to require their exact raw workspace FNVs.

This is an experimentally bounded equivalence class, not a claim that all
values in the canonicalized ranges are harmless. Its evidence is limited to
the two observed Q-family workspaces:

```text
B06VI: raw/canonical 69b4c386/0b161f01
B06VJ: raw/canonical dd61cb51/0b161f01
```

Those workspaces differ at 16 bytes, all inside the already frozen
canonicalization ranges, with zero differences outside; all 26 Q-specific
markers match. B06VK adds neither a raw row nor a canonical range.

## Artifact identity

```text
image ID: X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905
coreboot base: fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local X58 changes
config: configs/x58-pro-e-b06vk.config
config SHA-256: cc1d16f94c93f79f2124a16918caf3e303f1a0b75e5d150613994ea8c6a3f779
generated .config SHA-256: 05ab4d35dcd02c51b40df31e943ab6144dcf1128d054f77674dce851cc57de21
handoff: X6HO version 8, 160 bytes
payload: reduced serial-only SeaBIOS rel-1.17.0, deliberately non-booting
COM1: 03f8, 115200 8N1, no flow control
```

Released artifacts:

```text
594ff8ed582c81799269e66edb1c896c9909f65bbb42e29dc7e616aa58d1e53d  builds/experimental/msi-x58-pro-e-b06vk-coreboot-base-4MiB.rom
9864055f60f81880f36e0b4c6877c95bd10e503ed89eea1f42fd2cb7bade8328  blobs-local/msi-x58-pro-e/b06vk/msi-x58-pro-e-b06vk-unpatched-4MiB.rom
9863beac69de51a473d99ffbbf60a2c05f7a7aae3cebbc5f12887782a1c2435e  blobs-local/msi-x58-pro-e/b06vk/msi-x58-pro-e-b06vk-deterministic-4MiB.rom
51db5981f881e0cac7155543a8e8e620ac6cad9803137bfdc44dce8a1cbd3864  blobs-local/msi-x58-pro-e/b06vk/msi-x58-pro-e-b06vk-deterministic-w25q128-16MiB.rom
```

The public base contains coreboot, CPUID-206c2 microcode and the reduced
SeaBIOS payload, but no MSI CSI/MINIT bytes. The local W25Q128 image is
16,777,216 bytes with an erased lower 12 MiB and the deterministic 4-MiB
composite at `0x00c00000..0x00ffffff`.

## Intended first hardware boundary

After the exact Q canonical class and all unchanged gates match, the first
novel hardware action is the inherited bounded UC memory-validation sequence:

```text
complete 0..640-KiB destructive test and clear
transactional 14-address alias test and rollback
complete 8-MiB CBMEM-window test and clear
complete 8-MiB object-window test and clear
CBMEM creation and exact version-8 handoff read-back
```

Only if every step succeeds may B06VK re-arm the measured I801 cookie,
finalize the persistent guard, enter postcar, relocate ramstage and start the
reduced SeaBIOS payload. The intended payload terminal is the serial message
`Boot support not compiled in.` It has no VGA, option-ROM, storage, USB,
network, TFTP, SSH or operating-system boot path. Any failed gate must retain
the CAR recovery ROMMON where CAR itself remains operational.

## Validation and first-run procedure

The completed build passed 170/170 full tests and 21/21 focused
B06VI/B06VJ/B06VK tests. Complete B06VI and B06VJ regression builds succeeded,
and repeated fixed-epoch clean B06VK builds were byte-identical. CBFS/payload,
local composition, wrapper patch, erased lower region and W25Q128 top placement
were verified. Those are static/build results. B06VK was subsequently executed
seven times on the target, as recorded below; no programmer read-back was
captured.

Use only the established E5645/CPUID-206c2 stepping-2 system, microcode `0x1f`,
sole SPD-0x54 BLS4G3D1609DS1S00. 4-GiB dual-rank x8 DIMM with SPD FNV
`fb66b530`, DDR ratio 6, unchanged GPU/PSU and socketed W25Q128 recovery path.
Verify the complete programmer read-back against
`51db5981f881e0cac7155543a8e8e620ac6cad9803137bfdc44dce8a1cbd3864`.

The retained B06VJ cleanup verified CMOS diagnostic byte `0x0e=0x2c`; that is
preparation evidence, not a B06VK boot or confirmed G3 start. If a lab AC cycle
is needed, address the Shelly directly at `192.0.2.215` and use only
**OFF, wait exactly 1 second, ON**. Power-on-after-AC-loss is not reliable, so
a physical power-button start may still be required. Keep COM1 armed before
the start and retain the known-good external flash recovery path.

## Hardware results

Seven retained executions used the fixed E5645 and sole SPD-0x54 DIMM. Runs
01--04 followed a Shelly `OFF -> 1 second -> ON` interruption; this is not
claimed as a fully discharged G3 cycle. Runs 05--07 used controlled AC removal
followed by AC restoration and a manual power-button start from S5. COM1 was
armed before every recorded power action.

| Run | Power provenance | Pass-3 result | MINIT | Terminal result |
|---|---|---|---|---|
| HW-01 | Shelly 1 s | `17a00/a00502`, CSI `08/03e3d24e` | returned `EAX=0`; workspace raw/canonical `0395518f/95abbb4b` | mandatory OBSERVATION terminal; no DRAM |
| HW-02 | Shelly 1 s | `17600/b00502`, CSI `08/03e3d24e` | not called | crossed-profile gate |
| HW-03 | Shelly 1 s | `17600/b00502`, CSI `08/03e3d24e` | not called | crossed-profile gate |
| HW-04 | Shelly 1 s | `17600/b00502`, CSI `08/03e3d24e` | not called | crossed-profile gate |
| HW-05 | controlled 15 s G3, manual start | `17000/b00502`, CSI `0c/8b38506a` | not called | crossed-profile gate |
| HW-06 | controlled 60 s G3, manual start | stable pre-CSI `A0=17200` | not called | pre-A0 allowlist gate |
| HW-07 | controlled 60 s G3, manual start | `17000/b00502`, CSI `0c/8b38506a` | not called | crossed-profile gate |

HW-01's MINIT return retained the expected I801 return tuple, High-QPI state,
memory-controller endpoint, completion bytes and clean ABI/canaries. It was
nevertheless not profile Q: its canonical workspace digest was `95abbb4b`, and
12 of Q's 26 marker bytes differed. B06VK therefore correctly kept the
inherited OBSERVATION terminal. Runs 02--05 and 07 stopped before MINIT because
they recombined individually known CPU and CSI values into rows which B06VK did
not admit. HW-06 stopped before the third CSI call because `17200` was outside
the frozen pre-A0 set.

No run performed an ordinary DRAM access, created CBMEM, entered postcar or
ramstage, or started SeaBIOS. The seven immutable captures are:

```text
41d57ef04f3363e92d21f18386b105a1e6d2f055e25b6f4a436861174ce5ce24  research/msi/captures/2026-09-05-b06vk-hw-01-observation-17a-workspace-0395518f.raw
2c5bfbed4d95b5de488ed4decd75959993d2dd791bb19e3ce3ea8596e0074f37  research/msi/captures/2026-09-05-b06vk-hw-02-cross-176-b-csi08.raw
5a7c390b25b1b1dc17eda1b492b79a0cee39bc20c2340a5b9abd9d5fa58241e9  research/msi/captures/2026-09-05-b06vk-hw-03-cross-176-b-csi08.raw
f225bea3bde7ba85153da752bb220683d0c2c605fde13ece6b05f14722927329  research/msi/captures/2026-09-05-b06vk-hw-04-cross-176-b-csi08.raw
b3770a9fba2678a7476470b4dab5120984d5ea8b7003fbbb02d98bb872484d30  research/msi/captures/2026-09-05-b06vk-hw-05-g3-15s-cross-170-b-csi0c.raw
266a57546feabd00d1e09dc9ff6e64170e245eb76a7cd8af7752b157ea821cf3  research/msi/captures/2026-09-05-b06vk-hw-06-g3-60s-pre-a0-172-gate.raw
f2e9f01c1bdad7043e6e239d9143a4406bc62da36d7d7a65cdf57e0d87741dad  research/msi/captures/2026-09-05-b06vk-hw-07-g3-60s-cross-170-b-csi0c.raw
```

After HW-07 was archived, ROMMON `autoguard clear` and `vinputs` verified CMOS
diagnostic byte `0x0e=0x2c`. No reset followed that cleanup.

See the [B06VK manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
for the complete producer/consumer contract, CBFS layout and reproduction
commands.
