> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI X58 Pro-E: coreboot research tree

> Public historical research overview. Start with the
> [current mainboard guide](../../Documentation/mainboard/msi/x58_pro_e.md)
> for the supported development workflow and current limitations, and the
> [research index](Documentation/PUBLIC_RESEARCH_INDEX.md) for a topic map.
> This long-form chronology is retained for engineering context. Its original
> local-workspace commands and build names are not a promise that old private
> release artifacts can be reproduced from the public export.

This repository records the clean-room research needed for a native coreboot
port of the MSI X58 Pro-E (MS-7522).  The payload can eventually be SeaBIOS or
EDK2, but neither payload replaces the pre-RAM work: CPU/cache-as-RAM setup,
DDR3 training, CPU-to-X58 QPI initialization, X58/ICH10R setup, and resource
construction must already be complete before a payload runs.

## Current result

Pending source-only change (2026-09-08): the working configuration now enables
[serial-independent SPD admission](Documentation/spd-compatibility.md) for an
otherwise byte-identical replacement DIMM. Raw SPD identity stays logged;
only serial bytes122–125 are excluded from the compatibility fingerprint.
**No successor image has been built or flashed.** The latest available image
remains the unchanged archived B06WK, with its original full-SPD hash gate.

Latest assessment (2026-09-08): B06WH has reached SeaBIOS, display, working
USB input/storage and graphical Windows PE, which stops with ACPI error
`0xA5`. SATA SSD detection and basic reads are also recorded. B06WI-HW-01 has
now passed its new IRQ transaction, ACPI construction and JetFlash boot-sector
handoff, but the operator's subsequent screenshot shows
`ACPI_BIOS_ERROR (0xA5)` again. The four bugcheck parameters remain unavailable; no completed
OS boot is recorded. B06WJ now extends native CPU/LPC/HPET ACPI descriptions
while retaining the working RAM/QPI and USB path. Its two release builds are
byte-identical and 469 host tests pass. The 8 September local-time
[B06WJ hardware run](research/msi/b06wj-jetflash-hw-2026-09-08.md) now confirms
the new CPU/LPC/HPET construction marker, SeaBIOS and JetFlash handoff.
The operator subsequently confirms the same top-level Windows ACPI error;
B06WJ therefore does not fix the failure. Fresh vendor-live captures and MSI/
Intel ACPI comparisons found omitted IOH interrupt routes and a reserved bit
in the CTBL SSDT descriptor. Both have now been corrected in target RAM,
fully read back and retained through a JetFlash MBR handoff. The operator
subsequently reports A5 again; no successful Windows boot or exact bugcheck
subtype is established, and kernel-side table selection is not captured.
That RAM experiment left the ROM unchanged. See the
[RAM experiment and evidence](research/msi/b06wj-acpi-a5-runtime-lab-2026-09-08.md),
[overall state](Documentation/overall-status-2026-09-07.md) and
[B06WJ release manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
for the hardware-tested predecessor.

The newest image is **B06WK**. The operator activated it, and
[B06WK-HW-01](research/msi/b06wk-intenso-hw-2026-09-08.md) now records its
ramstage identity, repaired FADT marker, SeaBIOS and actual Intenso selection.
SeaBIOS rejected the received USB boot sector as nonbootable and fell back
to iPXE; this attempt did not reach an OS or test Windows A5.
With the operator-reprepared Intenso, the subsequent
[B06WK-HW-02 retry](research/msi/b06wk-intenso-retry-hw-2026-09-08.md)
passes that boundary: SeaBIOS executes its boot sector, ISOLINUX 6.03 starts,
and `/pmagic/bzImage`, `initrd.img`, `fu.img` and `m.img` all load with `ok`.
Linux's early setup reaches `Probing EDD ... ok`. The subsequent operator
photo confirms Linux 6.8.0-31-generic executing UHCI/PnP/RTC driver init
through 28 seconds; the operator reports a stall with the final visible
line `rtc_cmos 00:00: registered as rtc0`. A completed Parted Magic session
is not confirmed; the last line does not establish the root cause.
The [subsequent diagnostic retries HW03–05](research/msi/b06wk-linux-initcall-hw-2026-09-08.md)
all stop earlier at the exact USB preflight: five UHCI controllers retain
an unexpected suspend/resume tuple after 1/1/2-second mains interruptions.
HW06's operator cold start restores all six expected UHCI tuples and passes
the existing USB gate. Automatic Intenso selection and the ISOLINUX TAB
editor are captured. The original command starts before diagnostic editing;
the old helper misses that transition and sends stale edit bytes, with no
successful readback or Enter. A global Syslinux timeout is a source-supported
possibility, not a measured cause. HW07 uses the corrected menu-only helper:
only ESC is sent, and the operator personally enters the kernel arguments.
Native COM1 plus `initcall_debug` now confirms Linux passing RTC initialization,
starting `/init` and eudev, and attaching SATA storage, both USB sticks,
USB HID and the RTL8168 NIC. ISO9660 reads are captured. The Radeon driver fails
because its requested `radeon/CEDAR_pfp.bin` file is unavailable (`-2`);
other module initialization continues afterward, including KVM at964seconds.
The final622646-byte capture is archived; the board is left unchanged.
A completed desktop/session is still unconfirmed. Significant slowness,
an unclassified MCE notification
and a source-supported high-RAM cache-coverage gap remain open.
B06WK generates the CTBL/IOH route repairs natively and adds vendor-supported
legacy VGA resource windows and the fixed-power-button FADT description.
No new GPIO/IRQ hardware sequence, RAM/QPI policy or payload change is added.
Two clean builds are byte-identical; 515 host tests and 7 native C generator
tests pass. The [B06WK release manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
identifies the complete 16-MiB flash target and preserved source/logs.
Windows A5 is not claimed fixed. Next is to complete the Linux observation
and obtain runtime cache/error data if a session becomes available, without
repeating the accepted RAM/QPI bring-up tests.
The subsequent [HW08 Memtest boot attempt](research/msi/b06wk-memtest-hw-2026-09-08.md)
is blocked before SeaBIOS by the previously observed UHCI short-reset state;
the newly prepared Intenso tester has not executed in HW08. After the
operator's next cold start, HW09 passes the USB gate and automatically selects
Intenso AluLine5.00; its boot sector and GRUB execute. The operator subsequently
confirms that Memtest did not run: no memory-test pass or failing address is
established. The [detailed Linux latency analysis](research/msi/b06wk-linux-latency-root-cause-analysis-2026-09-08.md)
prioritizes the uncovered high-RAM cache policy; the bus-ff conflict and DMAR
warning also occur under vendor firmware, while all eight USB IRQ assignments
match the reference. No firmware fix or new hardware test occurred during
that analysis. A later operator iPXE photo now proves Memtest5.01 running
with23 reported errors and zero completed passes. Its ten visible failures
fall in the last low-RAM page, inside HW07's SeaBIOS reservation. The
[photo/source analysis](research/msi/b06wk-memtest-hw-2026-09-08.md)
identifies Memtest5.01's preference for the earlier coreboot map over BIOS
E820 as a specific possible firmware/USB-memory conflict, not a proven DIMM
defect or a passing test. The actual HW07 CPU
brand string also corrects the target inventory to E5649@2.53GHz; E5645 is
the vendor reference, not the measured HW07 target. The history below records
earlier milestones and their then-current limits.

B00 through B06WK, plus the isolated B06VQ-USBTRACE1, B06VQ-PLATRO1 and
B06VQ-ICHBASE1 derivatives, are now represented by a hash-verified, private
local corpus. Proprietary bytes remain ignored; the repository contains only their
manifest, extraction/analysis tools, and independently written observations.
The main inputs are:

- MSI `A7522IMS.8F0`: complete descriptorless 4 MiB AMIBIOS8 EEPROM image;
  its package says X58 Pro SLI, while the user reports a byte-identical match
  to the target X58 Pro-E chip contents; SHA-256
  `ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8`;
- Intel DX58SO2/DX58OG 0920 (`SOX5820J`), original DX58SO 5600
  (`SOX5810J`), and Intel S55xx R0033/R0069;
- ASUS Rampage III Formula 0402/0701/0702/0903 and Apple MacPro4,1/5,1 EFI
  images.

The MSI and ASUS AMIBIOS8 bootblocks contain 32-bit `CSI_INITDLL` and
`MINITDLL` images.  The MSI bootblock calls CSI/QPI initialization first and
memory initialization second.  Intel desktop/server firmware instead uses
GUID `63C0690C...`; S55xx UI/PDB metadata identifies it as
`UnCoreInitPlatform`.  Apple has an independent `D71C8BA4...` candidate,
externally labeled `UncoreInitPeim` by the pinned UEFITool GUID database.
MSI `MINITDLL` shares exact 888/890-byte executable regions with the server and
Apple candidates.  These are strong common-code-ancestry anchors, not proof of
authorship and not a standalone MRC/FSP ABI.

The targeted ASUS 0701→0702 “Memory Recheck” diff isolates a private branch
change inside `MINITDLL`.  ASUS 0903 changes memory compatibility while keeping
both embedded init DLLs byte-identical to 0702, directing the next search to
caller policy/tables and other modules.

Experimental images B00 through B04 have each reached their intended terminal
halt once on real hardware; repetition criteria remain open.
B00 proves reset, the MSI-derived 64-KiB CAR window at `0xfff80000`, stack, and
C entry for the E5645 test system.  B01's verified control flow accepts `df`
only after the read-only CF8/CFC probe returns exact ICH10R LPC ID
`8086:3a16`.  B02 accepts it only after narrow LPC read-back, Fintek-family
verification, COM1 activation, scratch/internal-loopback tests, and bounded
pre-RAM banner transmission all pass.  Only the terminal B02 code was
reported, so physical serial reception and boot provenance remain unverified.

B03 preserves B02, then uses the normal CBFS loader to enter an uncompressed
XIP romstage in the top flash window and halt there in CAR at unique code
`0xde`; the user has reported that terminal code once.  The complete trace,
serial text, cold/warm provenance, and repetition count remain unverified.

B04 has two byte-identical clean builds from the same local source state.  It
keeps the proven narrow LPC/UART path and invokes only the existing coreboot
ICH10 early BAR helper plus the common SMBus BAR/enable-register helper, with
strict preflight checks and exact read-backs.  The user has reported its
terminal `dd` once.  The full trace, cold/warm provenance, 60-second terminal
stability, programmer read-back, and repetition count remain unverified.  B04
performs no SMBus transaction or SPD read.

B05 is byte-reproducible and statically audited.  Its first user-reported
hardware run reached terminal `fa`, which the verified control flow assigns
to exact I801 `DEV_ERR` after the single bounded byte-data command to SPD
address `0x50`, offset `0x02`.  It did not reach the `dc` success gate.  The
run's DIMM population, complete trace, cold/warm provenance, serial output,
terminal stability, and programmer read-back remain unverified.

On the identical reference machine, a later vendor-booted Linux runtime check
found valid DDR3 SPDs at `0x50`, `0x52`, and `0x54` and observed
`SMBus_PIN_CTL=0x07`.  This proves the responding runtime topology only; it
does not reconstruct B05's DIMM population or establish the pre-DRAM pin
state.

B06A is byte-reproducible and statically audited.  Its first user-reported
hardware execution reached terminal `0x45`.  In the verified B06A control
flow, that terminal is possible only after the exact-CPUID and inherited B04
gates, the `PIN_CTL=0x04` write, idle `PIN_CTL[2:0]=0x07`, and all eight
bounded address probes complete.  It identifies exactly one DDR3 type
response at `0x54`; the other seven probes completed with exact `DEV_ERR`.
The complete trace, physical serial reception, cold/warm provenance, DIMM
inventory, terminal stability, and programmer read-back remain unverified.

B06B keeps QPI, IMC, and DDR registers untouched and reads DDR3 base-SPD bytes
`0x00..0x7f` from address `0x54`, then checks the revision-selected JEDEC CRC.
Two clean B06B builds are byte-identical.  Its first user-reported hardware
execution reached exact terminal `0x64`.  In the verified control flow, that
requires all 128 one-shot reads, DDR3 type `0x0b`, a matching stored base CRC,
and the internal UART/TEMT gate.  It does not prove the reference byte values,
external serial reception, physical slot, cold/warm provenance, or repetition.

B06C is built twice byte-identically and statically audited.  Its first
user-reported hardware execution reached exact terminal `0x76`.  That path is
possible only after all 128 base reads, DDR3 type, and the base CRC pass; it
then rejects a byte-0 declaration other than the one assumed 176-used/256-total
tuple.  It performed no upper-SPD read, decoder call, or policy derivation.
The subsequent B06H measurement identified the reason: raw byte 0 is `0x93`,
not the assumed `0x92` tuple.

B06H is the byte-reproducible, read-only measurement follow-up.  It repeats
the proven 128-byte/CRC path, emits `0x7a`, and halts with
`0x80 | (SPD[0] & 0x7f)`, while the optional UART dump retains raw byte 0.
It issues no additional SMBus command and contains no upper-SPD reader or
DDR3 decoder.  One hardware execution reached terminal `0x93` and delivered
the complete bootblock/romstage log over COM1 at 115200 8N1.  It establishes
256 used bytes in a 256-byte SPD and matching base CRC `0xec4f`; UART receive
and interactivity are not established by that run.  No autonomous experimental
boot path yet initializes the IMC, trains DDR3 or High-Speed QPI, establishes
usable DRAM, executes a payload, or writes SPI.  B06J deliberately exposes
manual low-level accesses, which are outside that boot-path claim.

B06I is the corresponding 256-byte read-only experiment.  It reads the proven
base once and bytes `0x80..0xff` twice, requires exact agreement, fingerprints
the result, invokes the existing coreboot DDR3 decoder behind strict structural
checks, and derives but does not program a DDR3-800 cycle candidate.  Two clean
builds are byte-identical; hardware execution remains pending.

B06J adds the first interactive CAR ROM monitor.  At POST `0xbc`, COM1 accepts
bounded line commands for CPUID, MSR, I/O, PCI configuration, MMIO, dumps, and
on-demand B06I SPD telemetry.  Human writes require an exact one-operation
unlock.  A separate permanent mode implements the documented SerialICE v1.5
byte stream so a compatible emulator can forward hardware operations to the
board.  A 2026-09-01 hardware session proved physical UART receive, human
command parsing, full 256-byte SPD telemetry, PCI reads/writes, one-channel
RCOMP completion, and ZQCL completion on both DIMM ranks.  Rank-0 automatic
PHY training reached `INIT_CMPLT` deterministically but set none of the four
training pass bits, so usable DRAM is not claimed.  SerialICE-QEMU forwarding
remains untested; the permanent SerialICE byte stream itself has now driven
repeatable live PCI/PHY reads and bounded, restored writes from an external
Python controller.

B06K is a byte-reproducible, statically audited extension of the same monitor
with bounded X58 PHY scan-chain commands and fixed coarse-RD/RCVEN profiles;
its binary has not run on hardware.  The underlying B06J session proved live
lane-result mutation but no training pass.  B06L is the byte-reproducible
follow-up: its opt-in `rdexact`/`rdsweep` commands reproduce the newly
reconstructed MSI per-point order of MRS, both-rank ZQCL, and direct RD
training without the previously inserted FIFO reset.  B06L has not yet run on
hardware and does not autonomously initialize DRAM or QPI.

B06M keeps all those paths opt-in and adds an armed `baseinit` command for the
exact one-channel state already exercised manually in B06J-HW-01.  It gates
the reset state, reads back every policy write, and bounds RCOMP/MRS/ZQCL.  A
later audit found that its serialized UART phase messages do not explicitly
wait for the final character to reach TEMT, while its MRS path has only an
unmeasured PCI-read gap.  It was superseded before hardware execution.

B06N is the byte-reproducible corrected successor.  It adds an explicit
bounded UART-TEMT completion after every reset-phase marker and after each of
the eight MRS commands; at 115200 8N1, each completed progress frame provides
about 86.8 microseconds of directly clocked spacing.  The compiled ELF
contains the expected `TEMT=0x40` polls and MSI-order MRS/ZQCL/direct-RD
commands.  Three independent builds are byte-identical; hardware execution is
pending.  Like B06M, it leaves
`MC_CONTROL.INIT_DONE` clear, never accesses ordinary DRAM, and does not alter
or retrain QPI.

B06V0 is the first default-off vendor-assisted observation build. Its public
4 MiB base contains coreboot plus the pinned CPUID-206c2 Intel microcode; a
hash-pinned local composer inserts the exact MSI CSI wrapper/helper,
`CSI_INITDLL` and `MINITDLL` at their original XIP addresses. No vendor call
runs at boot. ROMMON gates the path on E5645/microcode `0x1f`, a sole SPD
responder at `0x54`, operator-confirmed full-SPD FNV, Slow QPI, ratio 6, X58
identity, complete-range fingerprints and a guarded private CAR stack. CSI,
result acceptance, policy construction/editing, hash-confirmed installation
and MINIT are separate one-shot commands; generic hardware writes dirty and
close the path until a cold reset. Even a candidate
MINIT return never causes B06V0 to access DRAM, leave CAR or start a payload.
Two clean builds and both local composite layouts are byte-verified; no B06V0
image has yet been flashed or executed on hardware.

B06V1 is the default-off transactional scripting successor and remains a
terminal CAR ROMMON. It accepts sealed tables of at most 32 read, write, mask,
bounded poll, pause-delay, and assert operations over I/O, PCI configuration,
MMIO, and MSRs. An exact canonical program digest and a fresh one-shot arm are
required before execution. Runtime PRE/POST traces retain old and observed
values; explicitly reversible mutations can be restored in reverse order with
verified read-back, either manually or automatically after failure. Failed
rollback attempts remain inspectable and retryable. The engine rejects scripted
writes into CAR, but cannot recover from a reset, fault, stopped CPU, lost QPI,
or hardware side effects. A matching host compiler and portable regression
tests are included. A 2026-09-04 hardware session proved passive tables,
digest and lock rejection, bounded polling, 64-bit MSR reads, the CAR write
guard, and both manual and failure-triggered automatic rollback using the
16550 scratch register. Repeated confirmed cold boots remain pending.

B06V2 is the byte-reproducible corrected successor. B06V1 exposed that its
CSI signature gate used an incorrect DOS `e_lfanew` value of `0x80`; the
active flash and hash-pinned local CSI image both contain `0xb0`. B06V2 pins
that metadata in the extractor and compiled runtime, has a distinct serial
identity, and retains all B06V1 safety behavior. A 2026-09-04 passive hardware
session reached its CAR ROMMON and completed three identical full-SPD command
passes. Four runtime checks reported `PROBE STATUS=ok CODE=00` with
`SIG WRAPPER=01 CSI=01 MINIT=01 CANARY=01`. The exact SPD/QPI tuple and empty
script state remained stable, and all vendor-attempt flags remained zero.
Boot provenance and programmer read-back were not independently established,
so this is one type-unknown pass rather
than a confirmed cold-boot repetition. No vendor entry point was called.

A subsequent user-confirmed cold boot passed the same passive gates and the
separately armed `vprep fb66b530` path with both status codes zero. The first
isolated `vcsi 00` then caused the documented CSI reset without returning.
The reset reached the coreboot romstage banner but not ROMMON. Source
correlation identifies the next stop as the deliberately cold-only SMBus
preflight: the reset retains the exact configured I801 BAR/command/HOSTC
state, which B06V2 rejects before printing its ICH10 report. A warm-resume
build is now required to reach the monitor and repeat CSI's second phase;
MINIT remains unattempted.

The user subsequently confirmed that an external cold boot recovered and ran
again, which separates the retained warm-reset state from a persistent flash
or board failure. B06V3 is the fail-closed observation build for that boundary.
It accepts either the established cold I801 tuple or exactly the tuple written
and verified by this firmware before CSI (`BAR4=00000401`, `HOSTC=01`,
`CMD=0001`), logs `COLD_DEFAULT` or `CONFIGURED_AFTER_RESET`, and rejects every
other entry state with the existing `f3` diagnostic. No vendor module or script
is called automatically. Both the bootblock and ROMMON now carry the B06V3
identity, fixing the inherited B06L header seen after the first CSI reset.
On 2026-09-04 B06V3 passed the retained-I801 boundary after the first CSI
reset. A second separately armed CSI call returned to ROMMON with
`EAX/EBX/ECX=2/2/0x106`, complete-state FNV `c4eab5a4`, intact vendor/CAR
guards, and unchanged Slow-QPI/ratio-6 observations. The exact result was
accepted after all `0x304` bytes were dumped. This proves the two-pass
reset/return path, not successful CSI, DDR training, or usable DRAM.

That run also exposed that the old policy builder confused standard CMOS
diagnostic `0x0e` with extended CMOS `0x8e`. B06V4's selector-controlled
telemetry corrected the earlier ad-hoc read: standard diagnostic `0x0e=0x6c`
selects the default policy path, extended `0x8e=0x6c` is bounded to `0x30`,
and extended `0xca=0x62` is a separate policy input.
`PMBASE+0x38` is ICH10 `ALT_GP_SMI_EN`, whose low-byte bits 6:4 select further
branches. B06V4 adds `vinputs`, implements the diagnostic-invalid MSI path
without writing CMOS, and keeps the valid path restricted to the sole captured
field-1 profile.

B06V4 then completed the two-pass CSI sequence, accepted the exact returned
tuple `2/2/0x106`, installed the reviewed one-DIMM policy candidate, and made
a real direct MINIT call. MINIT returned with `EAX=0`, but the complete
`0x2bcc`-byte workspace and post-call Uncore snapshot prove that this was an
early return after internal phase B3, not trained memory. Workspace byte
`0x4f=0x06` had bit 2 set and the full-path marker at `0xe79` remained zero;
the hardware memory mapper and channel registers remained untrained. B06V5 is
the next isolated experiment: it accepts only the exact B06V4 candidate FNV
`2e0ccce9`, clears policy status byte `0x0a` and status-derived flag bit 18,
requires resulting FNV `3c0f3a0b`, and reports the B4-B8 completion marker and
selected Uncore registers after a return. It still performs no DRAM access.

On 2026-09-04 one B06V5 sequence started from a CF9 full reset, not a verified
AC-cold boot. The cold-policy transform let MINIT execute its B4--B8 path and
return with completion marker `workspace[0xe79]=1`, mapper `0x24489`, channel-2
dual-rank geometry, and complete workspace FNV `6d87f4e4`. After that return,
separately commanded UC reads and reversible writes proved real DRAM access in
the fixed one-DIMM configuration. A ten-address manual test and a sealed
A22--A31 script both passed; the latter recorded 11 mutations and completed
verified reverse-order rollback. This is a vendor-assisted one-run milestone,
not native raminit, cold-boot stability, ramstage, or a payload boot.

B06V6 is the first automatic post-MINIT transition experiment. It recognizes
only the exact two-pass CSI state, SPD/policy/workspace fingerprints and final
Uncore tuple proven in the B06V5 session, with a persistent CMOS/I801 reset-
loop guard. After a matching return it tests and clears complete 8-MiB CBMEM
and object windows, enters normal coreboot postcar, relocates ramstage into
CBMEM, and stops before device enumeration in a DRAM ROMMON. The monitor can
inspect state and accept a CRC/read-back-verified object of up to 4 MiB over
COM1. Uploaded bytes cannot be executed. Its passive `timer` and `netprobe`
commands are evidence gates for a later polling TFTP file-transfer command;
that future command is temporary RAM loading for debug, not network boot.
Two fixed-epoch builds and the local W25Q128 composition are byte-verified,
but B06V6 has not yet run on hardware.

B06V7 preserves that complete B06V6 path as a selectable known baseline and
adds a separate robust Slow-QPI gate for two run-variable observations. CSI
byte `0x2a6` is accepted only as `08` or `0c` and is zeroed only while
calculating canonical FNV `8403ac98`. Nine bounded MINIT-workspace ranges are
likewise zeroed only for canonical FNV `7a34f363`; raw buffers and raw digests
remain available as telemetry and in a version-2 handoff. All CPU, SPD,
policy, logical CSI tuple, MINIT return/stack/canary, Uncore, MTRR and DRAM
gates remain fixed. A captured High-QPI CSI state canonicalizes differently
and is explicitly rejected. B06V7 is built and locally composed but has not
yet run on hardware.

B06V8 was executed once and safely fell back to CAR ROMMON before CSI. The
run exposed a missing ICH10 prerequisite: `RCBA+0x3400[2]` was clear, so
ports `0x72/0x73` aliased the standard RTC bank. Earlier values labeled as
extended CMOS were therefore clock/calendar registers. This does not
invalidate the measured High-QPI transition, but it makes the B06V8 input
recipe unsafe and non-deterministic. B06V9 supersedes it by exact-gating and
enabling the upper RTC bank and applying the already observed successful CSI
caller-state profile through seven hash-pinned local wrapper-byte changes.
It remains a terminal three-pass CSI experiment: no MINIT, DRAM handoff,
ramstage, or payload.

The controlled B06V9 hardware run subsequently completed CSI pass 1, returned
successfully from pass 2, accepted the exact High-QPI endpoint and completed
the outer IOH SYRE reset. Pass 3 then stopped fail-closed before CSI because
both duplicate predicates required CPU `ff:02.1 + 0xa0=00017600`, while the
stable target and vendor-booted reference both reported `00017000`. B06VA is
the byte-reproducible follow-up: its sole source change accepts exactly
`{00017000,00017600}` in both gates, retains the full preselector and all
B06V9 guards, and adds explicit preselector telemetry plus POST `13` after
selection. One live B06VA sequence has now returned from the third CSI call
with an intact CAR ABI, complete state FNV `8b38506a`, and stable High-QPI.
The whole CPU PHY `ff:02.1` config space matched the vendor ratio-6 High-QPI
snapshot. The initial power/reset type and early marker stream were not
captured, so repetitions remain pending. B06VA remains terminal in CAR and
cannot invoke MINIT, use DRAM, enter ramstage or start a payload.

B06VB exact-gates that B06VA result (`0/0/11`, CSI FNV `8b38506a`), derives
the reviewed seven-edit policy (`bc4268bf -> 3c0f3a0b`), and permits one
guarded MINIT observation. Four controlled G3 runs completed pass 1 and pass
2, but stopped safely at the pass-3 preselector because CPU `ff:02.1 + 0xa0`
was `17a00/17800/17a00/17a00`, outside its two-value set; no pass-3 CSI or
MINIT call occurred.

B06VC admits the exact four values observed across the earlier and B06VB runs,
`{17000,17600,17800,17a00}`, only at the pre-pass-3 read. It neither masks nor
writes A0 and claims no field semantics. Four controlled five-second G3 runs
completed pass 1/pass 2 and both reset transitions 4/4. Their pre-pass-3 A0
sequence was `17c00/17a00/17c00/17000`: runs 1/3 failed closed before CSI,
while runs 2/4 called pass-3 CSI and returned ABI-clean with `0/0/11`, raw
state FNV `03e3d24e`, and byte `0x2a6=08`. Run 4's complete post-CSI platform
tuple matched; its 772-byte CSI state differed from the saved B06VA state at
exactly `0x2a6` (`08` versus `0c`). Both variants canonicalize to FNV
`908dabb6` with that byte zeroed. The strict gate stopped both returns before
MINIT, so there was no POST `d3/d4`, MINIT, DRAM, ramstage, graphics, or
payload execution. The exact observed preselector A0 evidence is now
`{17000,17600,17800,17a00,17c00}`, but it establishes neither a valid
mask/range nor field semantics. Adding exactly `17c00` and canonicalizing only
the observed `0x2a6={08,0c}` variants is a proposed successor experiment, not
behavior implemented by B06VC.

B06VD has now completed two controlled G3 sequences.  One failed closed on a
nonmatching post-CSI endpoint; the other accepted the exact
`2a6=08/raw=03e3d24e/canonical=908dabb6` High-QPI state and returned from
MINIT with EAX zero and workspace FNV `94299f43`.  In the retained CAR ROMMON,
a simultaneous 14-address test and D0--D31 walking-one test passed under UC
and restored every original dword.  This is one vendor-assisted full-path
return plus targeted memory evidence, not full-capacity, repetition, native
raminit, ramstage, or payload proof.

B06VE has now had three recorded hardware captures.  One entered its persistent
guard fallback immediately, and one rejected a nonmatching pass-3 CPU endpoint
before MINIT.  The third completed the exact `2a6=08` CSI path and returned
from MINIT with EAX zero, High-QPI status `070f0f03` and the expected
IOH/MC/channel endpoint.  Its workspace raw FNV was the newly observed
`b6346533`, however, rather than B06VE's sole admitted `94299f43`; B06VE
therefore failed closed in the post-MINIT **CAR recovery ROMMON**.  It did not
enter postcar, run the automatic full-window tests or reach DRAM ROMMON.
Reversible scripts in that retained state subsequently proved the selected PAM
values and representative C--F shadow points, including the exact future
SeaBIOS load and entry words, with exact rollback.

B06VF is the byte-reproducible payload-entry predecessor.  It couples both
observed raw workspace forms to their exact byte patterns and a common
canonical digest, adds a complete destructive/clearing test of 0--640 KiB,
rechecks the version-4 CBMEM handoff at the payload boundary, publishes only
the three tested RAM apertures, opens and verifies PAM shadow, and loads a
single reduced SeaBIOS segment at `000f8800..000fffff` before entry
`000fecd6`.  Its SeaBIOS is deliberately serial-only and non-booting: no VGA,
storage, USB or boot path is enabled.  Its first controlled long-G3 hardware
run completed both CSI reset continuations and returned from pass 3 with
`0/0/11`, CSI byte/raw/canonical `0c/8b38506a/908dabb6`, High-QPI
`070f0f03`, and CPU A0 preserved as `17800`.  CPU register 9c was `a00502`,
not B06VF's required `b00502`; B06VF therefore failed closed before MINIT.
No DRAM test, postcar, ramstage, payload, or SeaBIOS code ran.

B06VG is the byte-reproducible coupled-state successor.  It double-samples
pre-CSI CPU A0 and accepts only the six literal observations `17000`,
`17400`, `17600`, `17800`, `17a00`, and `17c00`.  Its two profiles are
deliberately disjoint.  The unchanged PRIMARY profile (`17000 -> 17000`,
CPU 9c `b00502`, CSI `08/03e3d24e`) may use the inherited B06VF path through
tested DRAM windows, ramstage, and the reduced SeaBIOS payload.  A non-170
OBSERVATION profile requires A0 to remain exactly equal, CPU 9c `a00502`,
and one exact CSI byte/digest pair; it may invoke MINIT once under the
persistent guard, prints the complete returned CSI/workspace state, and then
must return to the CAR ROMMON.  It cannot promote to DRAM or SeaBIOS.  B06VG
has been built twice byte-identically and statically verified, but has not
yet run on hardware.

B06VH subsequently exercised the coupled High-QPI path on hardware. Three
controlled G3 attempts failed safely on crossed profiles; the fourth exact
OBSERVATION profile returned from MINIT with EAX zero, QPI `070f0f03`,
workspace raw/canonical digests `50f67315/92c70df4`, and the measured I801
return tuple. In its retained default-UC CAR session, both a reversible
14-address test and a one-address D0--D31 walking-one test passed and restored
their originals. That is targeted evidence, not full-DIMM or repeated
stability proof.

B06VI is the byte-reproducible, local-composite-verified payload attempt based
on that exact observation. It adds profile O as one indivisible row beside
B06VH's PRIMARY A/B/P/N rows, extends the handoff to version 6, and defers O's
I801 re-arm until all bounded low-memory, address-alias, two 8-MiB window,
CBMEM, and handoff read-back checks pass. The user has authorized provisional
RAM stability as the bring-up assumption for this run. The payload is still a
serial-only, deliberately non-booting SeaBIOS entry probe. Its first execution
rejected a crossed CPU/CSI row before MINIT. A following controlled 60-second
G3 execution reproduced exact O, returned from guarded MINIT with EAX zero and
the expected High-QPI/IOH/MC endpoint, then rejected a new complete workspace
`69b4c386/0b161f01` before any ordinary DRAM access. The full workspace, CSI,
and policy were captured read-only and the persistent CMOS guard was cleared.

B06VJ is the byte-reproducible successor for that exact second O-family
workspace. It adds it as profile Q/wire ID 6 in the same producer/consumer
truth table and pins both workspace digests plus all 26 observed marker bytes;
the previous five rows and canonicalization ranges remain unchanged. O and Q
both defer I801 re-arm until every bounded UC memory, CBMEM, and version-7
handoff read-back check succeeds. The public base, private local composite,
reduced SeaBIOS segment, and W25Q128 top placement are verified. One retained
B06VJ hardware execution later reproduced the exact O-family CPU/CSI/policy
and High-QPI/IOH/MC endpoint and returned from guarded MINIT with EAX zero.
Its workspace retained canonical FNV `0b161f01` and the complete 26-byte Q
marker tuple, but had new raw FNV/SHA-256
`dd61cb51/370ca2366b44bfc66882f286686600851a60bf834ae5c79c249ba365ad845cef`.
B06VJ therefore rejected the non-enumerated raw digest at the coupled exact
gate before any ordinary DRAM access. It retained clean CAR ROMMON; no CBMEM,
postcar, DRAM-backed ramstage, SeaBIOS or graphics path ran. Initial AC/G3
provenance and programmer read-back were not independently captured, so this
is one type-unknown execution rather than a confirmed cold repetition.

B06VK is the byte-reproducible Q-canonical-class successor. It changes only
Q/ID 6 so the raw workspace FNV is integrity-covered telemetry rather than an
admission key; canonical FNV `0b161f01`, all 26 Q markers and every other gate
remain exact. PRIMARY A/B/P/N and O keep exact raw digests. The handoff is
version 8. Repeated clean builds, B06VI/B06VJ regressions, 170/170 full tests
and 21/21 focused tests passed; the local W25Q128 image is
`51db5981f881e0cac7155543a8e8e620ac6cad9803137bfdc44dce8a1cbd3864`.

Seven later hardware runs all terminated safely in CAR. One OBSERVATION row
returned from MINIT but produced workspace `0395518f/95abbb4b`, not Q's
canonical class. Five runs exposed repeatable cross-combinations of known
CPU/CSI states and stopped before MINIT; one further run observed pre-CSI A0
`17200` and stopped at the old allowlist. No B06VK run made an ordinary DRAM
access or reached CBMEM, postcar, ramstage, SeaBIOS, or graphics. This evidence
supports a separately gated broader experiment; it does not prove memory
training or payload entry.

B06VL is that separate, explicitly `BROAD_UNSAFE`, default-off experiment. It
admits only the finite cross-product of seven literal A0 values, both observed
CPU-`9c` values, and the two exact CSI byte/raw/canonical forms to one guarded
MINIT call. Workspace hashes are telemetry rather than admission keys, while
ABI, signatures, canaries, policy, completion bytes, I801/CMOS, SPD and the
full QPI/IOH/MC endpoint remain mandatory. A version-9/profile-7 handoff is
created only after the complete uncached low-memory, alias, two 8-MiB window,
CBMEM and handoff-readback sequence succeeds. Three clean builds were
byte-identical and 178/178 tests passed. The local W25Q128 candidate is
`9f8e1085d09207073983bbb3e26ce51474ced604e020a4206406e1a485c3f5e7`.
A subsequent retained hardware execution completed all three CSI phases,
returned from guarded MINIT with EAX zero, passed the independent hard-return
gate and every bounded uncached low-memory/alias/8-MiB-window test, then entered
real coreboot postcar and DRAM-backed ramstage. Coreboot emitted its tables and
jumped to SeaBIOS 1.17.0; SeaBIOS found the coreboot/CBMEM data, probed 65 PCI
functions on bus 0, and reached the reduced payload's intended `Boot support
not compiled in.` terminal. This is one complete diagnostic payload entry, not
yet a repeated cold-boot, downstream GPU, storage, graphics, or OS-boot result.
The complete retained transcript and bounded claim are in the
[B06VL-HW-02 evidence report](research/msi/b06vl-coreboot-seabios-hw-02.md).

B06VM was the first deliberately boot-capable successor.  It keeps the exact
B06VL version-9 memory/QPI/IOH handoff and all fail-closed gates, but removes
the successful-path RAMMON stop.  Ramstage then admits only the measured
GF108 GPU, RTL8168, ICH10R USB and legacy-IDE SATA topology, allocates from a
fixed non-overlapping map, and starts SeaBIOS 1.17.0 with real boot/menu,
ATA/USB/CD-ROM, physical VGA-ROM and HTTPS+trust iPXE support.  The complete
16-MiB W25Q128 artifact was built twice byte-identically and independently
audited.  Its first hardware run passed memory, CBMEM, postcar and ramstage;
the RTL8168 enumerated behind ICH10R, but no endpoint answered behind X58
`00:03.0`, so the strict topology guard stopped at POST `38` before SeaBIOS.
This proves selective downstream PCI on the NIC path, not GPU link training,
graphics, device boot, or network boot.  SMP/AP init,
full ACPI/PIRQ/SMM/S3 support and generalized hardware support also remain
future stabilization work; these do not intentionally stop the first boot
attempt.  See the [B06VM design and run procedure](Documentation/b06vm-auto-pci-vga-ipxe.md).

B06VN is the isolated PEG-link successor for the installed AMD Radeon HD 5450.
After the inherited hard gates it logs X58 ports `00:01.0`, `00:03.0`, and
`00:07.0`, conditionally writes the documented IOU0 x16/start value `0x000c`
once to `00:03.0 + 0x190`, and polls `DLLA`/`LT` for at most one second.  It
accepts the runtime-observed AMD VGA identity without guessing a device ID;
an absent card continues toward serial SeaBIOS.  The HD 5450's own onboard
VBIOS is used: no AMD ROM is embedded, normal checksum validation remains on,
and the CBFS RTL8168 iPXE ROM stays independent.  The 16-MiB B06VN image is
byte-reproducible and passed the complete 202-test suite plus independent
object/CBFS/top-placement audit.

Two payload-reaching executions passed the RAM windows, CBMEM, postcar and
ramstage and entered boot-enabled SeaBIOS: HW-02 is retained in exact split
fragments, while HW-04 is one end-to-end 68,088-byte exclusive capture whose
aggregate line-rate envelope passed and which has no HW-02-style large replay
prefix. IOU0 was already active at Gen1 x16 (`DLLA=1`, `LT=0`) in both, so the
conditional write was skipped. The GPU still did not answer at `01:00.0`;
SeaBIOS therefore found no VGA and did not run the Radeon VBIOS. Both paths
enumerated the RTL8168 and executed the embedded iPXE ROM through the Ctrl-B
prompt and first logged INT 16h call; immediate serial Ctrl-B input on the
repeat was silent as an unretained operator observation. No Ethernet link,
DHCP, TFTP or device boot is claimed. A separate
retry safely rejected out-of-set `PRE_A0=0x16e00`, so the pre-call state is
not yet deterministic. Both payload scans exposed 32 bus-2 aliases of the X58
internal functions, matching the documented default `IOHBUSNO.Valid=0`
behavior; repairing that routing state is the next isolated experiment. See
the [B06VN link-start procedure](Documentation/b06vn-iou0-physical-vbios.md),
[first hardware analysis](research/msi/b06vn-hw-02-2026-09-06.md), and
[hardened repeat](research/msi/b06vn-hw-03-hw-04-2026-09-06.md).

B06VO adds one exact X58 configuration-routing correction before PCI scan.
Its first retained hardware execution changed `00:14.0+0x10a IOHBUSNO` from
`0000` to the vendor-observed `0100`; the two sampled internal-function aliases
immediately disappeared.  SeaBIOS then enumerated 68 functions instead of 98:
all 32 known bus-2 X58 aliases were absent, while the real Radeon VGA/audio
functions appeared on bus 1.  Paired CF8 and direct-PCIEXBAR reads saw the
HD 5450 (`1002:68f9`) immediately and consistently.  Coreboot assigned the
VGA function's resources and forwarding, and SeaBIOS copied, validated,
entered and returned from the card's physical 59,904-byte VBIOS.  The operator
supplied a display photo showing SeaBIOS, RTL8168 iPXE on `02:00.0`, and its
Ctrl-B prompt.  SeaBIOS subsequently executed the RTL8168
iPXE ROM.  A second complete run repeated the RAM, IOHBUSNO, 68-function PCI,
physical-VBIOS and visible-display result.  Its recorder then observed no UART
byte for at least 120 seconds after iPXE's first logged `INT 16h AH=01` entry.
Source inspection shows that the non-blocking status call is followed by
iPXE's `sti; hlt` wait for a BIOS timer tick.  The leading, not-yet-hardware-
proved explanation is the custom single-CPU ramstage's missing standard LAPIC
LINT0 ExtINT virtual-wire setup, preventing legacy PIC IRQ0 delivery.  Two
one-second-relay successes are not the required cold/warm repetition set, and
no network, storage or OS boot has occurred.  See the
[first B06VO hardware analysis](research/msi/b06vo-hw-02-2026-09-06.md) and
[long-run repeat](research/msi/b06vo-hw-04-2026-09-06.md).

B06VP is the independently audited one-delta follow-up.  During the
single BSP CPU-cluster initialization it validates `IA32_APIC_BASE`, records
TPR/SVR/LVT0/LVT1, calls coreboot's existing `setup_lapic_interrupts()` once,
and requires the selected post-state to be unmasked LINT0 ExtINT plus LINT1
NMI.  It leaves B06VO's memory, QPI, IOHBUSNO, PCI/GPU, SeaBIOS and prompted
iPXE policy unchanged.  Two fixed-epoch builds are byte-identical, 226/226
repository tests and checkpatch pass, and a separate clean B06VO regression
reproduced the published base exactly.  A complete retained hardware run then
passed the selected LAPIC gate, advanced SeaBIOS/iPXE beyond the old timer
wait, brought the RTL8168 link up, acquired DHCP, downloaded the 388,848-byte
netboot.xyz NBP by TFTP and displayed its menu.  SeaBIOS initialized both EHCI
and all six UHCI controllers, but logged no USB device; i8042 status remained
`ff` and timed out.  USB and PS/2 input therefore remain unproved.  This is one
successful run, not the required repetition milestone.

B06VQ is the default-off, one-register USB successor.  It factors the existing
ICH10 EHCI required-field sequence into a reusable helper and applies only the
Intel-documented masked `EHCIIR2` update to `00:1a.7` and `00:1d.7` before PCI
enumeration.  It requires exact whole-dword readback, then records bounded,
read-only EHCI/UHCI, ownership, port, power-routing and GPIO telemetry before
SeaBIOS.  The full coupled ICH10 driver remains disabled.  The identical
vendor-booted board confirms `EHCIIR2=2002130a` on both controllers while its
ownership/SMI enables and global PPO/MAP/UPRWC controls are clear.  Two clean
fixed-epoch builds are byte-identical; 234/234 repository tests, five normal
ICH10 board builds, binary inspection and W25Q128 top-placement verification
pass. B06VQ has not yet run on target hardware, so no USB enumeration claim is
made.

B06VQ-USBTRACE1 keeps B06VQ's hardware writes exactly and explicitly excludes
the later SATA experiments. Its pinned SeaBIOS derivative stores up to 96 USB
controller/port/control-transfer events in a 1536-byte append-only RAM journal
and emits them only after USB setup. Additional pre-payload ICH10 USB gate
state is read-only. The trace reached `USB keyboard initialized` under QEMU
i440fx with 24 contiguous events and zero drops; this validates the
instrumentation, not MSI hardware. Two fixed-epoch builds were byte-identical,
all 277 repository tests passed, the ordinary B06VQ regression retained its
published hash, and the diagnostic image has not yet run on the target.

B06VQ-PLATRO1 is a second, independent diagnostic derivative. It keeps the
ordinary unmodified SeaBIOS payload and adds one non-fatal read-only census
after PCI resource enablement: LPC/PM/GPIO/RCBA bases and decodes, PIRQ bytes,
PM/SMI state and bounded PM-timer movement, OIC/HPTC plus strictly gated HPET
state, all eight domain resources and fixed-range overlap checks. It performs
no new platform write, does not read IOAPIC MMIO, creates no resource and
emits no ACPI table. Two clean builds were byte-identical and the local
W25Q128 image is ready, but it has not run on target hardware.

B06VQ-ICHBASE1 is a separate, default-off six-field ICH10 baseline. It
factors the six BIOS-required RCBA updates from the historical ICH10 driver
into one narrow reusable helper, while keeping GCS and every unrelated field
outside the experiment. The board wrapper requires exact LPC/RCBA/FDSW gates
and either the complete live-observed PRE tuple or its complete TARGET tuple;
every mixed state stops before the first write. The target tuple was first
proved by a sealed 24-operation B06VP CAR transaction and direct readback.
Two fixed-epoch builds are byte-identical, all 297 repository tests and the
CBFS/composite/top-placement audits pass, and a W25Q128 image is ready. The
automatic ICHBASE1 path has not yet run on target hardware.

B06VR is the next default-off, single-policy storage experiment.  It admits
only the exact quiescent ICH10 dual-IDE reset identity or its own retained
AHCI identity, programs only SATA MAP bits 7:5 to `011b`, and immediately
clears BAR5 as required when the component changes from I/O to memory type.
Exact MAP `0060`, F2 `8086:3a22/010601`, F5 absence and BAR state are required
before normal PCI enumeration.  PCS, SATA clocks, AHCI MMIO, PHY policy and
the broad ICH10 driver remain untouched.  Two fixed-epoch builds are
byte-identical; 244/244 tests and W25Q128 top-placement pass.  B06VR has not
run on hardware and does not yet claim an AHCI controller or disk.

B06VS layers one further documented SATA prerequisite on B06VR.  Only after
the complete PCI resource audit passes, it requires an exact assigned/stored
2-KiB ABAR and changes only PCS port-enable bits `5:0` to `3f` with an 8-bit
masked write.  Presence fields remain read-only telemetry, reserved bit 14
must be zero, and ORM bit 15 is preserved.  The released W25Q128 image was
built twice byte-identically after independent review; 257/257 tests, binary,
CBFS, composite and top-placement audits pass.  It is not hardware-tested and
does not program SATA clocks, AHCI MMIO, PHY state or a disk.

B06VT isolates the following SATA prerequisite without enabling the HBA. It
admits SCLKCG only as exact reset `00000000` or retained target `00000193`,
then changes only documented Field 1 bits 8:0 to `0x193`. Port-clock-disable
bits and every reserved field—including bit 30—must remain zero. The exact
complete dword, inherited AHCI/PCS/resource gates and unchanged PCS policy are
required before normal device enablement. Two fixed-epoch builds are
byte-identical; 265/265 tests, machine-code, CBFS, composite and W25Q128
top-placement audits pass. B06VT is not hardware-tested and performs no AHCI
MMIO, reset, COMRESET/OOB or disk access.

B06WC is the first deliberately integrated experimental convergence image.
Instead of requiring each intervening B06VY/B06VZ/B06WA/B06WB register delta
to receive separate hardware qualification, it combines their exact-gated
IOAPIC, fixed-resource, quiescent-HPET and TCO work with the live-proven
standard 8259/ELCR sequence, a quiet native ACPI-mode transition, minimal
FADT/MADT/MCFG/DSDT tables, read-only USB admission and an explicitly armed
ROMMON PIT/ExtINT probe. It retains the corrected AHCI, Radeon physical-VBIOS,
SeaBIOS and RTL8168 iPXE path. Two clean builds are byte-identical; 403/403
repository tests, IASL/AML, CBFS, vendor-composite and W25Q128 top-placement
audits pass. The local full-chip SHA-256 is
`c7482e03098f1e9bb8f845401d1bdc33e97a1b131ceffd9bd9fc27e089254831`.
B06WC subsequently ran once on target hardware.  It completed the fixed
memory/QPI path, DRAM tests, CBMEM, postcar, ramstage, PCI discovery and fixed
resource allocation, then correctly stopped before the first SATA PCS write
because coreboot's in-memory SATA command policy still included legacy I/O.

B06WD corrected only that software policy, retained hardware CMD `0000`, and
then reached PCS/SCLK/AHCI ready, device init/finalize and ACPI table
construction once.  It also opened ICH10R KBC decode, but the primary PS/2
interface test returned `03` and keyboard reset ACK was `ff`.  The read-only
USB census found no connected endpoint.  ACPI then stopped before MCFG and
SeaBIOS because its callback mistakenly read PCIEXBAR from absent `00:00.1`;
the expected ECAM host identity remained readable.

B06WE is the clean single-functional-delta successor on the SeaBIOS branch.
It moves that read-only gate to the measured CPU-side X58 SAD at `ff:00.1`,
requires ID `8086:2d81`, and leaves all hardware writes and the payload path
unchanged.  Two release builds are byte-identical, 415/415 tests pass, and the
16-MiB W25Q128 artifact is top-placement verified.  B06WE has not yet run on
target hardware; its first decisive boundary is POST `8e` followed by `8f`
and then ACPI/SeaBIOS continuation.  PS/2 and USB remain unresolved and COM1
115200 8N1 remains the preferred control path.

B06WF is the default-off late interactive USB-lab successor. It stops at
`BS_WRITE_TABLES` / `BS_ON_ENTRY`, after normal PCI resource assignment and
device finalization but before ACPI and SeaBIOS. Its fixed, independently
unlocked commands can test GPIO56 high/low with rollback, EHCI CONFIGFLAG
ownership with rollback, a dedicated non-reversible OC-latch acknowledgement,
and the vendor-correlated GPIO57/H_PWRGD output-low/delay/high sequence. The
latter may remain verified output/high into SeaBIOS. Live B06WD/XRS evidence
already showed that transition clearing OCA on every EHCI and companion-UHCI
port while sticky OCC/OCI and real CCS/LSDA remained dynamic. B06WF itself is
build-tested only: two clean outputs are byte-identical, 427/427 tests pass,
and the W25Q128 full-chip image is top-placement verified.

B06WG is the separate automatic USB/SeaBIOS successor. It leaves the B06WF
monitor disabled and promotes only the live-proven GPIO57/H_PWRGD sequence:
an exact late static/controller preflight, GPIO57 output-low, a calibrated
65.536-ms delay, GPIO57 high, bounded EHCI/UHCI sampling and an all-live-OCA-
clear gate. Sticky OCC/OCI and dynamic connection/speed state remain telemetry.
A second read-only gate immediately before the payload requires GPIO57 still
output/high and all OCA clear. Coreboot never writes a USB controller register;
the pinned SeaBIOS build retains complete EHCI/UHCI, hub, mass-storage and HID
keyboard initialization plus the deferred USB trace. Its isolated SeaBIOS
configuration uses debug level 8 so the trace remains visible without the
level-9 INT 16h polling flood observed after the successful B06WF GPIO57
release. Its explicitly experimental `ASSUMED_STABLE` post-memory path keeps
the exact result/MTRR gates, the transactional alias smoke and deterministic
single-pass clearing with nine sparse readbacks per window, but omits the
three exhaustive full-window pattern/invert/read passes only for B06WG. All
historical builds retain the exhaustive path. Two release builds are
byte-identical, 447/447 tests pass, and the W25Q128 image is top-placement
verified. One target run has now completed the automatic QPI/MINIT, postcar,
ramstage, PCI/resource, GPIO57, ACPI and SeaBIOS path. SeaBIOS initialized the
HD 5450 display, USB HID keyboard and a Transcend 128GB USB mass-storage
device; physical keyboard input worked, and RTL8168 iPXE completed DHCP plus
the first TFTP chainload. A later local Hiren's/Windows PE attempt sustained
large EHCI reads and reached a graphical BSOD before automatically resetting;
the STOP code was not captured. This is a major single-run boot milestone, not
full-memory or cold-boot stability. Later iPXE TLS/HTTP failures and the BSOD
leave high RAM, ACPI/interrupt, ICH10 root-port and device handoff work open.

B06WH is the controlled low-noise successor for the next local-boot test. It
compiles the same B06WG platform, fast-postmem and automatic GPIO57 code, with
the same USB/HID/AHCI/iPXE feature set. Its only functional delta is SeaBIOS
debug level 6 instead of 8; the bounded level-1 USB trace remains enabled.
This removes the level-7 per-transfer stream that produced 41,549
`ehci_send_pipe` lines in the retained Hiren's capture. Two clean builds are
byte-identical, 454/454 current tests pass, and the W25Q128 image is
top-placement verified at SHA-256
`b5f894d4a2fc1552c141d656ef4564cc4be5d4f7ae5ca75de638bb4315024c6b`.
B06WH subsequently reproduced display, USB HID/MSC and graphical Windows PE,
which stopped with operator-observed `ACPI_BIOS_ERROR (0xA5)`; the four
parameters and therefore the exact subtype remain unknown.

B06WI is the build-tested, hardware-untested successor. It exact-gates a small
vendor-correlated D26/D31/D29/D28/D27 IRQ-route transaction, gives the
BSP-only IOAPIC unique ID 1, and publishes matching direct-GSI `_PRT`, MADT
IRQ0-to-GSI2 and disabled GPE0 interfaces while suppressing the unproved
i8042 advertisement. PIRQ, `INT_LINE`, IOAPIC redirection entries, GPIO,
Vendor AML/SMM and the `0xffffff00` Vendor-DSDT operation region are not
copied. The W25Q128 artifact is SHA-256
`3d9e6e03e2ed5a59dcdb95b34e2f0cd82e4becee9aa188b81efe2597374c6b04`;
there is not yet a B06WI POST, serial, payload or Windows result.

Continued B06J SerialICE work recovered the broad pre-training PHY initializer
from MSI `MINITDLL` and found its nearly instruction-for-instruction Intel
DX58SO counterpart in PEIM `63C0690C...`.  Applying the fixed common and
channel-2 prefix plus the ratio-6/CL6-derived channel field changes all lane
results deterministically, but a complete `0x00..0x80` coarse-RD sweep still
has no pass.  All experimental PHY fields were restored and QPI remained
`0x030f0f03`; the missing work is now narrowed to the following dynamic
per-DIMM/per-lane initializer rather than another coarse sweep value.

A read-only Uncore snapshotter and matching diff utility now capture the
enumerated Bloomfield/Gulftown CPU-side QPI, SAD/TAD, memory-controller, and
channel PCI functions under vendor firmware.  On 2026-08-04 the tool captured
the real MSI board with an E5645 and three 4-GiB UDIMMs.  The live firmware's
94,749-byte `MINITDLL` code section is byte-identical to the statically
analyzed `8F0` code.  The resulting code/register correlation reconstructs
PCIEXBAR setup, DIMM reset/CKE, triple-channel mapping, SPD geometry, QPI link
state, and an unpublished `03.4:0xf8` bit-30 completion poll.

A controlled vendor-firmware comparison then held memory policy at DDR ratio
6 while changing only QPI Slow Mode to High Speed.  Three captures per state
were byte-identical.  The transition changed link-0 PLL status/ratio from
`0x160c0110/0x10` to `0x160c0112/0x12` and restored the older High-Speed PHY
tuple, including `PH_CTR=0x0040a0a8`, `PH_PIS=0x070f0f03`, and
`PH_PRT=0x00322808`; the DDR policy remained unchanged.  This separates QPI
speed policy from the DDR divider, but the snapshots are states rather than a
safe programming order.  High-Speed QPI was deliberately deferred in B06B;
the later vendor-assisted B06VL path reached the expected High-Speed endpoint
once, but a native open paired CPU/IOH setup, reset, calibration and training
sequence remains unfinished.

## Documents

- [Firmware analysis](Documentation/firmware-analysis.md)
- [Multi-firmware corpus analysis](research/comparisons/firmware-corpus-analysis.md)
- [Vendor-state capture procedure](Documentation/vendor-state-capture.md)
- [Uncore register/code correlation](research/comparisons/uncore-register-correlation.md)
- [Live X58 Uncore, SPD, and firmware-code correlation](research/msi/live-uncore-2026-08-04.md)
- [First-build architecture](Documentation/first-build-architecture.md)
- [Concrete implementation plan](Documentation/implementation-plan.md)
- [Flash and recovery prerequisites](Documentation/flash-recovery.md)
- [Bring-up log](Documentation/bringup-log.md)
- [Test matrix](Documentation/test-matrix.md)
- [B06B experimental image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06C experimental image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06H header-telemetry image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06I full-used-SPD image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06J CAR ROMMON image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06J ROMMON and SerialICE command reference](Documentation/b06j-rommon.md)
- [B06N timed-MRS baseinit procedure](Documentation/b06n-timed-mrs.md)
- [B06N timed-MRS image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V0 vendor-assisted image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V0 vendor ROMMON and recovery procedure](Documentation/b06v0-vendor-rommon.md)
- [B06V1 transactional script image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V1 register language and transaction procedure](Documentation/b06v1-register-scripting.md)
- [B06V2 corrected CSI-gate image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V3 CSI warm-resume image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V3 CSI return hardware observation](research/msi/b06v3-csi-return-2026-09-04.md)
- [B06V4 policy-input telemetry image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V4 MINIT B3-return analysis](research/msi/b06v4-minit-b3-return-2026-09-04.md)
- [B06V5 cold-MINIT gate image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V5 full MINIT and UC-DRAM hardware result](research/msi/b06v5-minit-dram-smoke-2026-09-04.md)
- [B06V6 automatic handoff image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V6 automatic handoff and DRAM ROMMON](Documentation/b06v6-ramstage-rommon.md)
- [B06V7 robust Slow-QPI image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06V7 robust Slow-QPI handoff](Documentation/b06v7-robust-slowqpi.md)
- [B06V8 retired High-QPI three-pass experiment](Documentation/b06v8-highqpi-three-pass.md)
- [B06V9 deterministic High-QPI three-pass build](Documentation/b06v9-deterministic-highqpi.md)
- [B06V9 image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VA exact High-QPI CPU-A0 allowlist probe](Documentation/b06va-highqpi-a0-allowlist.md)
- [B06VA image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VB one-shot High-QPI MINIT observation](Documentation/b06vb-highqpi-minit-observe.md)
- [B06VB image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VC exact-four-value High-QPI MINIT observation](Documentation/b06vc-highqpi-a0-4set-minit.md)
- [B06VC image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VD paired CSI-byte-2a6 High-QPI MINIT observation](Documentation/b06vd-highqpi-csi2a6-minit.md)
- [B06VD image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VE exact High-QPI automatic handoff](Documentation/b06ve-highqpi-auto-handoff.md)
- [B06VE image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VF minimal SeaBIOS entry probe](Documentation/b06vf-seabios-entry.md)
- [B06VF image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VG coupled MINIT/SeaBIOS experiment](Documentation/b06vg-coupled-minit-seabios.md)
- [B06VG image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VH PRIMARY workspace/rearm SeaBIOS experiment](Documentation/b06vh-primary-workspace-rearm-seabios.md)
- [B06VH image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VI coupled profile-O SeaBIOS experiment](Documentation/b06vi-coupled-profile-o-seabios.md)
- [B06VI image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VJ exact profile-Q SeaBIOS experiment](Documentation/b06vj-coupled-profile-q-seabios.md)
- [B06VJ image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VK profile-Q canonical-class SeaBIOS experiment](Documentation/b06vk-q-canonical-class-seabios.md)
- [B06VK image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VL broad hard-gated SeaBIOS experiment](Documentation/b06vl-broad-hard-gate-seabios.md)
- [B06VL image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VM automatic PCI/VGA/iPXE first-boot candidate](Documentation/b06vm-auto-pci-vga-ipxe.md)
- [B06VM image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VN IOU0/AMD physical-VBIOS experiment](Documentation/b06vn-iou0-physical-vbios.md)
- [B06VN image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VN hardened hardware repeat](research/msi/b06vn-hw-03-hw-04-2026-09-06.md)
- [B06VO IOHBUSNO configuration-routing experiment](Documentation/b06vo-iohbusno-routing.md)
- [B06VO image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VO first GPU/VBIOS/display hardware result](research/msi/b06vo-hw-02-2026-09-06.md)
- [B06VO long-run display/timer result](research/msi/b06vo-hw-04-2026-09-06.md)
- [B06VP BSP LAPIC ExtINT experiment](Documentation/b06vp-lapic-extint.md)
- [B06VP image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VP complete payload/network hardware result](research/msi/b06vp-hw-03-2026-09-06.md)
- [B06VQ isolated ICH10 EHCI experiment](Documentation/b06vq-ich10-ehci-init.md)
- [B06VQ image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VQ deferred SeaBIOS USB trace](Documentation/b06vq-usbtrace1.md)
- [B06VQ-USBTRACE1 image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [SeaBIOS USB-trace QEMU validation](research/msi/seabios-usbtrace-qemu-validation-2026-09-06.md)
- [B06VQ-PLATRO1 read-only platform census](Documentation/b06vq-platro1.md)
- [B06VQ-PLATRO1 image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VQ-ICHBASE1 six-field ICH10 baseline](Documentation/b06vq-ichbase1.md)
- [B06VQ-ICHBASE1 image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VR isolated ICH10 AHCI-map experiment](Documentation/b06vr-ich10-ahci-map.md)
- [B06VR image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VS isolated ICH10 AHCI-port experiment](Documentation/b06vs-ich10-ahci-ports.md)
- [B06VS image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VT isolated ICH10 SATA clock-field experiment](Documentation/b06vt-ich10-sata-clock.md)
- [B06VT image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VY deterministic IOAPIC experiment](Documentation/b06vy-ich10-ioapic-mask.md)
- [B06VY image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06VZ fixed platform resources](Documentation/b06vz-fixed-platform-resources.md)
- [B06VZ image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06WA quiescent HPET decode](Documentation/b06wa-quiescent-hpet-decode.md)
- [B06WA image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06WB early TCO halt](Documentation/b06wb-ich10-tco-halt.md)
- [B06WB image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06WC integrated experimental platform](Documentation/b06wc-integrated-platform.md)
- [B06WC image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06WD hardware analysis](research/msi/b06wd-hw-01-2026-09-07.md)
- [B06WE corrected ACPI SAD gate](Documentation/b06we-acpi-sad-bdf-fix.md)
- [B06WE image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06WF late interactive USB lab](Documentation/b06wf-late-usb-lab.md)
- [B06WF image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06WG automatic GPIO57 USB/SeaBIOS path](Documentation/b06wg-automatic-usb-seabios.md)
- [B06WG image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06WG USB/keyboard/local-Hiren's hardware result](research/msi/b06wg-usb-boot-hw-2026-09-07.md)
- [B06WH quieter USB/SeaBIOS successor](Documentation/b06wh-quiet-usb-seabios.md)
- [B06WH image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B06WI exact-gated vendor-correlated IRQ/ACPI experiment](Documentation/b06wi-vendor-irq-acpi.md)
- [MSI platform-initialization audit](research/msi/platform-init-audit-2026-09-06.md)
- [MSI ACPI template and DSDT analysis](research/msi/acpitbl-static-analysis-2026-09-06.md)
- [X58 ROMMON RAM-loader protocol](Documentation/ram-loader-protocol.md)
- [B06A experimental image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [B05 experimental image manifest](Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [Live I801/SPD reference capture](research/msi/live-i801-2026-08-31.md)
- [DDR-ratio-6 QPI Slow/High comparison](research/msi/qpi-ratio6-slow-high-2026-08-31.md)
- [CSI_INITDLL PCI-access reconstruction](research/msi/csi-pci-accesses-2026-09-01.md)
- [MSI init-module interface notes](research/comparisons/candidate-mrc-interface.md)
- [Experimental vendor-assisted memory-init assessment](Documentation/vendor-assisted-memory-init.md)
- [MSI reset/CAR/Super-I/O evidence](research/msi/early-boot.md)
- [Cross-firmware sequence comparison](research/comparisons/shared-sequences.md)
- [Public X58/coreboot prior-art survey](research/prior-art.md)
- [MSI module map](research/msi/module-map.csv)
- [Intel FV/PEIM map](research/intel/fv-map.csv)
- [Firmware corpus manifest](research/firmware-corpus.json)
- [UEFI report provenance](research/uefi-report-provenance.json)
- [Tool versions and extraction method](research/tool-versions.md)

## Reproducible inventory

The inventory and verification scripts use only the Python standard library.
They do not download, extract, or modify firmware:

```bash
python3 scripts/verify_firmware_corpus.py --allow-missing

python3 scripts/firmware_inventory.py \
  blobs-local/msi-x58-pro-e/7522v8F/A7522IMS.8F0 \
  blobs-local/intel-dx58so/SO0920P.bio
```

They report hashes, valid Intel microcode headers, firmware-volume headers,
and embedded PE images as JSON.  Proprietary input and extracted derivatives
live below `blobs-local/`; both supplied archives and that directory are
ignored by Git.  Do not commit or redistribute them.

`scripts/run_uefi_report.py` regenerates UEFIExtract reports with input/tool
hashes and preserved diagnostics.  `scripts/uefi_report_matrix.py` compares
pinned reports; `scripts/pe_inventory.py` inventories PE32/PE32+/TE bodies and
computes relocation-normalized duplicate hashes.  Long exact matches between
locally extracted sections can be reproduced with
`scripts/find_shared_sequences.py`.
A byte or canonical-hash match is an analysis anchor, not by itself a semantic
or ABI match.

On the target board, the conservative first Uncore capture is:

```bash
sudo python3 scripts/dump_nehalem_uncore.py --require-target \
  > vendor-cold-01-uncore.json
```

See the capture procedure before attempting 4 KiB PCI reads or broader
`inteltool` categories.

For a locally extracted, hash-matching MSI `MINITDLL`, reproduce the static
MMCONFIG call-site inventory with:

```bash
python3 scripts/analyze_minit_pci.py \
  blobs-local/msi-x58-pro-e/extracted/MINITDLL-region.bin \
  --only-decoded-uncore
```

## Evidence labels

Research notes use these labels:

- **verified-static**: directly observed in supplied bytes or disassembly;
- **documented**: supported by a cited public datasheet or upstream source;
- **inference**: consistent with evidence but not yet proven dynamically;
- **hardware-unverified**: requires a real-board capture or measurement.
