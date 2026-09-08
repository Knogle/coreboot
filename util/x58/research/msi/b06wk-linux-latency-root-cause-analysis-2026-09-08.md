> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WK: Linux latency, PCI/IRQ warnings and Memtest follow-up

## Scope and conclusion

Offline diagnostic analysis requested on 2026-09-08. No new hardware test,
UART input, reset, register write, firmware build or medium change occurred.
Existing raw logs are unchanged. The operator now explicitly reports that
Memtest did not run after HW09's Intenso/GRUB handoff.

The strongest concrete firmware defect is the missing write-back MTRR for
the remapped RAM at physical 4–5 GiB, despite publishing and actually using
that RAM in Linux. This is a strong cause candidate for severe slowness,
not yet an A/B-proven explanation of every latency warning or Windows A5.
The PCI bus-ff conflict and DMAR-IR erratum warning also occur under the
vendor BIOS and are not evidence of a new port-specific routing failure.
Linux's eight USB IRQ assignments match the reference exactly. Radeon has
a separate, explicit OS-firmware-loading failure. One unclassified MCE
notification must remain an open hardware-error item.

## Evidence and comparison limits

Primary run: [HW07 raw](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
622646 bytes, SHA256
`b08f7670ba9f8cc6e0b586cd8c22bdd59dd734518ba1ac43c5137efe214e81a4`.
[Metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) SHA256
`40c31209585dc4ec5aacce151f15e0dcf6ed46735f7c17d9f6b096a20945b5f9`.
Capture 16:07:47–16:32:31 UTC; one requested two-second target AC interruption,
sole transmitted byte ESC at 16:13:51.984531 UTC. The operator selected
Intenso and entered the Linux command. Full inventory and reset limits are
in the [HW07 record](b06wk-linux-initcall-hw-2026-09-08.md).

Intended ROM SHA256
`bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0`,
coreboot `a2eb375438c85cd7908140636cbee8407bee8c0d` plus archived WK delta;
no programmer readback was obtained. Target is **E5649 @ 2.53 GHz**, CPUID
206c2/step 2/microcode 1f, one 4-GiB DIMM at SPD54 and HD5450. The earlier
E5645 target label was an inventory error. Initialization remains the
documented optional local vendor-assisted CSI/MINIT path.

Reference: private local
`blobs-local/msi-x58-pro-e/x58-acpi-nvs-reference-20260908-01/dmesg.txt`,
SHA256 `9254a38907087f6a39e2fce5961646ef726039ca5275876b273a5c5d5b0947df`.
It uses the same Ubuntu 6.8.0-31 kernel family but vendor BIOS, E5645,
12 logical CPUs and 12 GiB. Target exposes one logical CPU and adds verbose
console diagnostics. This is a useful chipset comparison, not a controlled
CPU/RAM/boot-options performance comparison. Source interpretation below
uses upstream Linux v6.8, not a claimed bit-identical Ubuntu source build.

## What actually ran

| Kernel time | Evidence in HW07 |
|---|---|
| 0.244668 s | `NODE_DATA` allocated at 0x13ffd3000–0x13fffdfff, inside high RAM |
| 9.142184 s | ACPI initialization returned 0; IOAPIC mode used |
| 16.063438 s | PCI resource assignment returned 0 after the bus-ff warnings |
| 57.381136 s | CMOS/RTC initialization returned 0 |
| 107.050646 s | `/init` started: real initramfs userspace |
| 187.917800 s | eudev started |
| 326.599992 s | `hid_init` returned 0, beyond the operator photo's last line |
| 337.482773 s | `hid_init [usbhid]` returned 0 |
| about 391–393 s | Intenso and JetFlash disks/partitions enumerated |
| 405.123031 s | RTL8168 registered; no Linux IP/session proven |
| 408.568189 s | ISO9660/Joliet/Rock Ridge reads reported |
| 473–481 s | Radeon removed VGA console, then failed to obtain CEDAR firmware |
| 964.387633 s | `kvm_x86_init` returned 0; last captured kernel progress |

There is no captured kernel panic, Oops, ACPI AML exception, IRQ storm
diagnostic, `nobody cared`, or BAR allocation failure explaining a halt.
Absence of these messages does not prove those subsystems fully correct.
The desktop/completed live session remains unconfirmed.

The 613.836352→964.375516 s gap precedes the KVM initcall; it is not a
350-second KVM function duration. Host receipt bounds for that gap are
approximately 331–391 seconds, consistent with real elapsed silence.
Collector heartbeats alone are not board-liveness evidence.

## Interrupt-latency warnings: symptom, not an IRQ routing diagnosis

HW07 has **11 distinct perf warnings**, each printed twice. The reported
handler averages rise from 2588 to 26862 nanoseconds (2.588–26.862 us);
the allowed sampling ceiling falls from 77000 to 7000 samples/second.
These are not milliseconds, lost-interrupt counts or CPU-frequency values.

The x86 caller times the PMU NMI handler. Linux averages its durations and
reduces the permitted sampling rate when the budget is exceeded. The
average is initially biased low, so the rising series alone does not prove
progressive CPU degradation. The NMI watchdog is enabled in this run;
a user-started perf command is not required for PMU use.
[PMU caller](https://github.com/torvalds/linux/blob/v6.8/arch/x86/events/core.c),
[averaging and throttling](https://github.com/torvalds/linux/blob/v6.8/kernel/events/core.c).

The separate photographed `hrtimer` duration of 1644846 ns (1.645 ms)
belongs to HW06, not HW07. HW07 contains no such hrtimer warning. That HW06
warning reports delayed timer handling, not the identity of a defective device IRQ.
[Timer interrupt implementation](https://github.com/torvalds/linux/blob/v6.8/kernel/time/hrtimer.c).

At 257.808628 s, the clocksource watchdog skips a late check: its two elapsed
measurements are 1243833177 and 1243832892 ns. Difference: just 285 ns over
1.244 seconds, about 0.23 ppm. This is a too-long sampling interval, not
the separate Linux verdict that a clocksource is unstable. Initial/refined
TSC calibration is also close: 2527.192/2526.999 MHz. Target/reference CPU
SKUs differ; their approximately 2.53/2.40-GHz TSC values are not by
themselves evidence of an erroneous timer or overclock.
[Watchdog decision branches](https://github.com/torvalds/linux/blob/v6.8/kernel/time/clocksource.c).

## PCI bus ff, DMAR and actual USB IRQ assignments

The peer bus `ff` exposes CPU uncore functions. Its separately inserted
bus-number resource overlaps the existing root resource `00–ff`. The two
warnings correspond to insertion/reinsertion of that resource, not two
failed GPU memory BARs. `(null)` is the absent resource name in this message,
not a demonstrated null-pointer crash. The vendor log contains the same
two warnings at 0.490039/0.494491 s; target at 9.893233/10.509095 s.
Enumeration continues in both.
[Peer discovery](https://github.com/torvalds/linux/blob/v6.8/arch/x86/pci/legacy.c),
[bus resource insertion](https://github.com/torvalds/linux/blob/v6.8/drivers/pci/probe.c).

The DMAR-IR text is misleading if read literally as proof that this firmware
enabled interrupt remapping. Linux's early X58 quirk sets `irq_remap_broken`
from PCI ID/revision; target 8086:3405 revision13 matches it. The warning
branch tests that flag before `dmar_table_init()`, without measuring active
remapping hardware. The vendor emits the same warning. HW07 publishes no
DMAR table. Do not add speculative DMAR programming because of this text.
[Early quirk](https://github.com/torvalds/linux/blob/v6.8/arch/x86/kernel/early-quirks.c),
[warning before DMAR discovery](https://github.com/torvalds/linux/blob/v6.8/drivers/iommu/intel/irq_remapping.c).

The Linux USB driver IRQ numbers match all eight reference controllers:

| PCI device | Controller | Target IRQ | Vendor IRQ |
|---|---|---:|---:|
| 00:1a.0 | UHCI | 16 | 16 |
| 00:1a.1 | UHCI | 21 | 21 |
| 00:1a.2 | UHCI | 19 | 19 |
| 00:1a.7 | EHCI | 18 | 18 |
| 00:1d.0 | UHCI | 23 | 23 |
| 00:1d.1 | UHCI | 19 | 19 |
| 00:1d.2 | UHCI | 18 | 18 |
| 00:1d.7 | EHCI | 23 | 23 |

Target raw lines 6878–7127; reference lines 609–690. Linux uses ACPI for
IRQ routing and subsequently accesses both USB media. IRQ assignment is
not exhaustive proof of every route or electrical GPIO state, but this is
positive evidence against a blanket claim that USB interrupts are misrouted.

## Strongest firmware defect: RAM resources and cache policy disagree

The active WK configuration inherits the B06VM postcar policy. Source and
the successful exact MSR-readback gate at ramstage entry establish:

| Physical range | Inherited MTRR policy |
|---|---|
| 0x00000000–0x7fffffff | Write-back |
| 0x80000000–0xbfffffff | Write-back |
| 0x000a0000–0x000bffff | Uncacheable overlay for VGA |
| 0xfffc0000–0xffffffff | Write-protected ROM |
| 0x100000000–0x13fffffff | No covering variable MTRR: default uncacheable |

See [postcar setup](../../../../src/mainboard/msi/x58_pro_e/fail_closed_memmap.c)
lines 37–54 and [exact gate](../../../../src/mainboard/msi/x58_pro_e/ramstage_rommon.c)
lines 802–834/1099. The gate requires all other variable entries invalid;
it does not merely omit logging them. The code explicitly leaves the high
range uncovered because its postcar API accepts 32-bit addresses. General
ramstage CPU initialization remains deferred, rather than supplying a later
normal complete MTRR policy.

Nevertheless, E820 publishes that GiB as usable and Linux allocates its
`NODE_DATA` there. The reported PAT slot setup is not proof that missing
MTRR coverage was repaired. A post-Linux full MSR dump is still unavailable;
the cache condition at firmware entry is measured, while its persistence
throughout this run is a source-supported inference.

The Linux v6.8 startup audit strengthens that inference: `mtrr_bp_init()`
reads the inherited state and invokes cleanup, rather than synthesizing
WB coverage from E820. Both cleanup and uncached-RAM trimming reject valid
variable types other than WB/UC; our valid WP-ROM entry therefore excludes
both paths. Finalization does not add the missing high-RAM WB range.
[MTRR startup](https://github.com/torvalds/linux/blob/v6.8/arch/x86/kernel/cpu/mtrr/mtrr.c),
[cleanup and trimming](https://github.com/torvalds/linux/blob/v6.8/arch/x86/kernel/cpu/mtrr/cleanup.c).
Intel's effective-type table specifies MTRR-UC plus PAT-WB as UC; the PAT
slot initialization cannot override this range into WB.
[Intel SDM, Table 13-7](https://cdrdv2-public.intel.com/868137/325462-089-sdm-vol-1-2abcd-3abcd-4.pdf).

This does **not** mean all caches are disabled: vendor-return CR0=0x11 has
CD/NW clear, and postcar explicitly clears them again. The problem is the
memory type for a particular usable RAM range. It is a firmware handoff
defect distinct from DDR training or defective DIMMs.

The earlier two 8-MiB scalar clear loops together take approximately
241–301 seconds from capture receipt bounds, about 55–68 KiB/s. That is a
measurement of the complete early UC execution path, not isolated DIMM
bandwidth or proof that high RAM has identical performance. It reinforces
why leaving normal OS memory UC deserves priority over cosmetic warnings.

## Other genuine defects and limitations

- At 85.658163 s, `mce: [Hardware Error]: Machine check events logged`.
  No bank/STATUS/ADDR/MISC accompanies it. The vendor also has generic MCE
  notifications, but that proves neither the same cause nor harmlessness.
  New fault versus retained status and CPU/cache/interconnect/DRAM origin
  cannot be classified from this summary. No RAM error count is established.
- Radeon requests `radeon/CEDAR_pfp.bin` and gets `-2` at 479.860093 s;
  GPU probe fails at 481.094597 s after VGA-console deactivation. This is
  missing/unavailable Linux driver firmware at probe time, not the card's
  missing VBIOS: ATOM C09302 was already recognized. Other required CEDAR
  firmware files must also be checked, not just this first request.
  [Firmware loader](https://github.com/torvalds/linux/blob/v6.8/drivers/gpu/drm/radeon/r600.c).
- The photo ends at 326.434071 s, before the later GPU failure. Therefore
  that later failure cannot fully explain the earlier display boundary.
  Serial does prove HID completion and continuing execution beyond it.
- Linux activates one logical CPU, matching `CONFIG_MAX_CPUS=1` and the
  BSP-only firmware path. This is incomplete platform support, not evidence
  that the other cores physically failed. No successful CPU-frequency
  scaling driver is documented; TSC frequency does not measure live core
  frequency or rule out thermal throttling.
- SMI_EN, alternate GPIO SMI enables and PM/GPE event enables read zero
  before payload; EHCI legacy SMI controls also read zero. TCO is halted.
  An SMI storm is not measured here and should not be asserted. These boot
  snapshots are not proof that every possible SMI remains absent later.
- The missing `obex-check-device` helper is a userspace packaging error,
  not proof that the kernel stopped at that line. The later trace continues.

## Diagnostic output is a confounder, not a complete explanation

HW07 contains one Linux banner and monotonically advancing kernel timestamps.
3525 exact adjacent two-copy message runs begin when ttyS0 becomes enabled
at 2.809800 s. The multiline DMAR message is likewise repeated as a block.
This matches `keep_bootcon` retaining earlycon alongside the normal serial
console, not two executions of each initcall or two Linux boots.

About 513 kB of Linux serial data requires approximately 44.6 seconds of
nominal 115200/8N1 wire time. After kernel time 100 s, only about 49.8 kB
remains, about 4.3 seconds nominal wire time, yet timestamps advance to
964 s. This is not an upper bound on slow PIO/synchronous-console overhead;
it does refute treating byte volume alone as a demonstrated explanation.
Serial work outside the measured PMU handler does not automatically enter
the perf duration measurement.
[Earlycon polling](https://github.com/torvalds/linux/blob/v6.8/drivers/tty/serial/8250/8250_early.c),
[8250 console locking](https://github.com/torvalds/linux/blob/v6.8/drivers/tty/serial/8250/8250_port.c).

The replay-heavy preboot portion of HW08 is excluded. It exceeds physical
115200 throughput and is not a second Linux run. HW09's large LF-only
prefix is unexplained and also not progress evidence. Its coherent suffix
proves Intenso boot-sector execution and GRUB; the operator now confirms
Memtest did not run. There is no memory-test pass or failing address to
interpret, nor enough evidence to blame its media format or tester ABI.

## Next discriminating actions — proposed, not performed

1. Prioritize complete high-RAM WB coverage and resource/cache agreement.
   A Linux-only high-memory exclusion is a useful reversible A/B before a
   firmware repair; keep RAM/QPI training policy unchanged. A final fix must
   not cache the 3–4-GiB MMIO hole or silently invalidate exact handoff gates.
   On x86, `mem=3G` is a physical address ceiling at 0xc0000000 and excludes
   the high GiB, but removed RAM can become available for PCI assignment.
   Reserving precisely the known high interval with literal `memmap=1G$4G`
   is an alternative; any bootloader must pass the dollar sign unchanged.
   Review the resulting E820/resource map. Even a successful exclusion test
   isolates high-RAM use, not cache policy versus a remapping defect; the
   decisive counterpart restores that RAM with corrected, read-back WB.
   [E820 parser](https://github.com/torvalds/linux/blob/v6.8/arch/x86/kernel/e820.c),
   [kernel parameters](https://www.kernel.org/doc/html/v6.8/admin-guide/kernel-parameters.html).
2. Eliminate duplicate bootconsole output in a separate comparison while
   retaining ordinary serial capture. Do not combine this with memory,
   ACPI/APIC, clocksource and NMI changes in the same causal experiment.
3. Once reachable, capture effective MTRRs/PAT, existing MCE records,
   interrupt counters, clocksource and CPU performance/thermal state.
   Preserve raw MCE evidence before any consuming or clearing interface.
4. Supply the required Linux CEDAR firmware or separately test text-only
   graphics handoff. This is an OS-media issue, not a reason to replace the
   onboard VGA ROM.
5. Identify the exact Memtest product/version and visible GRUB outcome.
   Do not call this a failed memory test until the tester actually runs.

Windows A5 remains a separate unresolved ACPI bugcheck without its four
parameters. Linux's progress is meaningful but does not establish Windows
ACPI compliance or identify that earlier stop's subtype.
