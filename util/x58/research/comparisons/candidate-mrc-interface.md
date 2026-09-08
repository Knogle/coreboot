> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Candidate MSI early-init interfaces

> **2026-09-04 RTC-bank correction:** historical `0x72/0x73` observations in
> this document were made with ICH10 U128E clear and therefore aliased the
> standard RTC bank. The recovered call edges remain valid; those bytes are
> not an upper-CMOS ABI or reusable policy input.

Status: **static ABI reconstructed and wired into the default-off B06V0
research monitor; no hardware call has been made**. Addresses, sizes, call
edges and the behavior below are verified from the hash-pinned MSI image.
Unestablished field meanings remain neutral or are marked as inference.

## Original XIP layout

The 4 MiB `A7522IMS.8F0` image maps at `0xffc00000`. B06V0 preserves these
four original-address ranges:

| Object | Raw interval | Preferred/runtime address | Size | SHA-256 |
|---|---:|---:|---:|---|
| CSI wrapper | `0x3c04e2..0x3c0d19` | `0xfffc04e2` | `0x837` | `546f0c4d...af6c3` |
| CSI helper | `0x3c1554..0x3c1579` | `0xfffc1554` | `0x25` | `6641f8cc...d9d75` |
| `MINITDLL` | `0x3c1dc0..0x3da4c0` | base `0xfffc1dc0`, entry `0xfffc2000` | `0x18700` | `54fb7d14...1264a` |
| `CSI_INITDLL` | `0x3e6de0..0x3ee3c0` | base `0xfffe6de0`, entry `0xfffe7000` | `0x75e0` | `e7c42f1a...eb150` |

Both PE32/i386 images have no imports or exports and contain relocation
directories. Directory presence does not prove complete relocation coverage;
B06V0 therefore does not relocate either image. Raw pointers and RVAs are
congruent in the authoritative ROM, so the exact raw slices are also the
loaded XIP images. The older `0x75c0` CSI repack with hash `b1d14415...` is
not address-correct and is rejected.

The audited wrapper support is a closed local call set:

```text
fffc0bf7 -> fffc1554
fffc1554 -> fffc1566
fffc0cfd -> EAX=fffe7000; call EAX
```

No additional flash-code target was found in those slices. This is static
evidence, not a guarantee that every hardware side effect is understood.

## CSI wrapper ABI

The original wrapper is called as 32-bit cdecl with three stack arguments:

```c
ami_csi_wrapper(arg1, arg2, seed);
```

Static analysis finds `arg1` and `arg2` unused. The low byte of `seed` becomes
state byte `+0x0b`. The wrapper allocates and zeroes a `0x304`-byte local
object, fills constants and CMOS/strap/CPUID-derived fields, then calls:

```c
csi_entry(&state, &state);
```

The CSI entry uses the first pointer; the second is statically unused. The
wrapper returns its result through registers rather than normal cdecl:

```text
EAX = zero-extended state[0x2fc]
EBX = zero-extended state[0x301]
ECX = dword state[0x2f8]
EDI = &state
```

EBX and EDI are intentionally not preserved. EDI points to transient stack
storage. B06V0 therefore uses a dedicated assembly trampoline, captures the
register tuple, and copies all `0x304` bytes before reusing the private stack.

The seed's production source was temporary MMCONFIG byte `0xe00a1080`; its
cold/warm meaning is not yet recovered. Any supplied value, including zero,
is explicitly experimental.

CSI can alter APIC/QPI state, reset, or enter a permanent HLT/self-loop during
retraining. Its wrapper also leaves port `0x70=0x8e`, hence NMI disabled. A
software timeout on the same CPU is impossible. A returned tuple has no known
success interpretation yet; B06V0 acceptance only echoes the tuple and full
state digest as proof of operator intent.

## Direct MINIT ABI

The selected B06V0 path does not transplant the large AMI MINIT wrapper. It
calls the PE entry directly as 32-bit cdecl:

```c
uint32_t minit_entry(void *policy_e0, void *workspace_2bcc);
```

The policy buffer must contain all `0xe0` bytes. MINIT copies `0xdf` bytes
internally and writes its result byte back to `policy[0x0a]`. The initially
zeroed `0x2bcc` workspace stores an internal saved ESP at `+0x65` and a fixed
continuation at `+0x69`; function `0xfffd90d0` uses these for a private
non-local failure return, not an external callback.

The original wrapper's accepted-completion observations are all retained:

```text
EAX == 0
workspace[1] != 1
workspace[2] == 0
policy[0x0a] == 0
```

The original firmware then copied the workspace to physical 1 MiB. B06V0
does not do that and does not access DRAM. A four-gate return is labeled only
`CANDIDATE_ONLY_DRAM_UNTESTED`.

## Policy status

Most of the caller policy is now reconstructed mechanically from constants,
two distinct CMOS banks and branches selected by the low byte of ICH10
`ALT_GP_SMI_EN`. B06V3 hardware and the full MSI wrapper disassembly corrected
the earlier template in several important ways:

- standard CMOS diagnostic byte `0x0e` is read through `0x70/0x71`; when bits
  7:6 are nonzero, the wrapper skips the conditional CMOS-policy block;
- extended CMOS `0x8e` is separately read through `0x72/0x73`, bounded to
  `0x30`, and used to derive `policy[0x02]`; it is not the diagnostic byte;
- in the diagnostic-invalid path observed on the target (`CMOS 0x0e=0x6c`),
  `policy[0x01]`,
  `policy[0x04]`, `policy+0xbc`, and fields `0xc3..0xdd` remain zero;
- `policy+0x24` uses the base flags without the valid-CMOS `0x82/0xca`
  modifications, but still incorporates the CSI result/seed status bit;
- the valid-diagnostic path is reconstructed only for
  `ALT_GP_SMI_EN[6:4]=1`; `policy+0xbc=0x0085` and the decoded setup fields in
  that branch remain based on the three-DIMM reference capture;
- `policy[0xde]=0` is appropriate for E5645 but the special X5550 path differs;
- the original wrapper's optional ROM-object lookup for `policy+0x46` is not
  yet reproduced, so the direct experimental path leaves it zero.

B06V4 reports all of these inputs without CMOS data writes. Its reviewed
one-DIMM policy candidate passed a direct MINIT return boundary, but full
workspace and control-flow analysis proved an early return after phase B3;
memory was not trained.

The monitor prints the template digest, permits bounded dump/edit operations,
and requires a separately armed install command with the exact complete FNV.
It rehashes the installed runtime copy; a later edit invalidates that copy.
After MINIT it reports the installed input digest separately from the current
possibly modified policy and complete workspace digests. Installation proves
byte identity and operator confirmation, not semantic correctness.

## CAR contract used by B06V0

The coreboot configuration selects `NO_CBFS_MCACHE`. At the final link,
runtime metadata is below `_car_unallocated_start=0xfff85038`; aligned
scratch begins at `0xfff85040`. Five guards separate CSI snapshot, policy,
workspace and the private `0x7fc0`-byte stack
`0xfff88030..0xfff8fff0`. The final guard ends exactly at
`_car_region_end=0xfff90000`.

Runtime preparation is one-shot. Every call recomputes and compares the
expected pointer layout, validates all canaries, checks CPU execution mode,
microcode `0x1f`, exact one-DIMM SPD topology, the operator-echoed full
256-byte SPD fingerprint, MMCONFIG/X58 identity, Slow-QPI and memory ratio 6,
and verifies the complete four code ranges by pinned FNV-1a plus entry/PE
anchors. The flat 32-bit segment state and returned cdecl stack position are
also exact gates. A physical CSI or MINIT attempt can occur only once per
cold boot, including a return that later fails stack/canary validation.

## Decision

These modules are technically isolatable and useful for a local experiment,
but they do not expose a portable, documented MRC interface. `MINITDLL` is
not independent: the verified MSI order requires CSI first, and the remaining
seed, reset-state, result-schema and policy questions are material. The B06V0
shim is therefore a staged observation tool, not a permanent dependency and
not evidence of successful memory initialization.

See [the implementation assessment](../../Documentation/vendor-assisted-memory-init.md)
and [the B06V0 ROMMON procedure](../../Documentation/b06v0-vendor-rommon.md).
