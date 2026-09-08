> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VP ICH10R PM, SMI, GPIO and interrupt-routing census

Date: 2026-09-06

Status: read-only decode and next-step design.  No target access, firmware
build, or coreboot-source change was performed for this note.

## Result

B06VP is fail-closed at this boundary: `SCI_EN=0`, `PM1_EN=0`, both 32-bit
halves of `GPE0_EN=0`, `SMI_EN=0`, `ALT_GP_SMI_EN=0`, and all sixteen
`GPIO_ROUT` fields are disabled.  No PM1, GPE, alternate-GPI SMI, or SMI
source is enabled for delivery.

That does **not** mean the status registers are empty.  Four write-one-to-clear
(W1C) families contain history:

- `PM1_STS=0801`: power-button override (`PRBTNOR_STS`, bit 11) and PM-timer
  overflow (`TMROF_STS`, bit 0);
- `GPE0_STS=00000000:6eff0000`: GPIO0--7, GPIO9--11, and GPIO13--14;
- `SMI_STS=00006100`: periodic, TCO, and PM1 summary status; and
- `ALT_GP_SMI_STS=6eff`: GPI0--7, GPI9--11, and GPI13--14.

The status is evidence, not an enable policy.  It must not be copied into an
enable register, and a generic “clear everything” call would destroy useful
evidence.  In particular, coreboot warns that pending `PRBTNOR_STS` must be
cleared before SCI operation or SCI can remain asserted even though OSPM is
required to ignore that status bit
(`coreboot/src/southbridge/intel/i82801jx/lpc.c:262-271`).

Therefore the first safe SCI-capable mutation is **not** simply setting
`SCI_EN`.  It is a separately armed, non-reversible, exact-gated W1C write of
`PRBTNOR_STS` alone (`outw(0800, 0500)`), while every event enable and the
IOAPIC SCI entry **and legacy PIC IRQ9** remain masked.  The PIC condition
matters because B06VP deliberately retains the 8259-to-LAPIC ExtINT path.
SMI must remain completely disabled because this board selects `NO_SMM`.

## Evidence identity and transaction boundary

| Input | SHA-256 |
|---|---|
| `captures/2026-09-06-b06vp-pm-smi-gpio-census-run.raw` | `81d22eb61f6bd42757b3ee320d4874117991454e9678aac8bce20436148151e4` |
| `../scripts/ich10-pm-smi-gpio-routing-census-b06vp.xrs` | `cd8890aee61bd20e64e5809d3ae8a6c53ed5b8bc388f9238222f7ee256bfdda6` |
| `captures/2026-09-06-b06vp-pm-smi-gpio-census.metadata.json` | `8c0ddae17a22925fd4d690183f4a83a44994179f5b88eb20868a607bf0e04549` |

The script asserts exact LPC identity and decode before any PM or GPIO I/O
read (`research/scripts/ich10-pm-smi-gpio-routing-census-b06vp.xrs:5-9`).
The raw transcript shows all 32 operations complete, `WROTE=00` for every
operation, `RUN=ok`, `MUT=00`, program FNV-1a `6c4022bd`, and transaction
FNV-1a `3cd8780a`
(`research/msi/captures/2026-09-06-b06vp-pm-smi-gpio-census-run.raw:7-72`).
The repeated trace preserves the same observations at lines 73-137, and the
host discarded the completed read-only transaction at lines 138-142.

The coreboot sub-tree was at Git `fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2`
with local changes.  Consequently the individual source hashes at the end of
this note, rather than the Git identity alone, pin the definitions used here.

## Exact register decode

The ICH10-specific inteltool table confirms the PM register widths and
offsets, including 64-bit GPE status and enable blocks
(`coreboot/util/inteltool/powermgt.c:121-176`).  The common ICH PM code proves
the W1C access model by reading each status family and writing the observed
ones back to clear it (`coreboot/src/southbridge/intel/common/pmutil.c:26-37`,
`:56-69`, `:97-114`, and `:191-204`).  Enable helpers instead use ordinary
read-modify-write (`pmutil.c:10-24`).

### LPC PCI configuration

| Register | Observed | Supported decode | Safety consequence |
|---|---:|---|---|
| D31:F0 ID | `3a168086` | Intel ICH10R LPC identity | Exact identity gate passed. |
| PMBASE `40h` | `00000501` | decoded base `0500`; common code masks the low two bits | Use the runtime-derived base, not a copied vendor base (`pmbase.c:16-35`). |
| ACPI_CNTL `44h` | `80` | PM I/O decode enabled by the ICH10 early helper | PM reads are admitted (`i82801jx/early_core.c:52-55`). |
| GPIOBASE `48h` | `00000581` | decoded base `0580`; common code masks bit 0 | Use runtime-derived `0580` (`common/gpio.c:37-51`). |
| GPIO_CNTL `4ch` | `10` | GPIO I/O decode enabled by the ICH10 early helper | GPIO reads are admitted (`i82801jx/early_core.c:57-60`). |
| GPIO_ROUT `b8h` | `00000000` | sixteen 2-bit fields, all `GPI_DISABLE`; encodings are disabled/SMI/SCI/NMI = 0/1/2/3 | No GPI0--15 is routed to SMI, SCI, or NMI (`common/pmutil.h:26-30`; `i82801jx/lpc.c:115-141`). |
| GEN_PMCON_1 `a0h` | `0200` | bit 9 set; the only bit defined by the exact common source here is `SMI_LOCK` bit 4, which is clear | Do not assign a name to bit 9 or rewrite this register.  SMI policy is not locked (`common/pmutil.h:12-13`). |
| GEN_PMCON_2 `a2h` | `0005` | bits 0 and 2 set; exact ICH10 source does not define their names/access | Preserve and capture before any clear.  The ICH10 early code itself leaves a TODO to inspect its power-state bits first (`i82801jx/early_init.c:47-51`). |
| GEN_PMCON_3 `a4h` | `02` | `RTC_POWER_FAILED` bit 1 set; known battery-dead bit 2 and sleep-after-power-fail bit 0 clear | Treat RTC/wake policy as unqualified.  Do not clear it or advertise S4 RTC wake until RTC recovery is separately specified and tested (`common/pmutil.h:15-18`). |
| GEN_PMCON_LOCK `a6h` | `0000` | known `ACPI_BASE_LOCK` bit 1 and `SLP_STR_POL_LOCK` bit 2 clear | Do not lock unproved policy (`common/pmutil.h:19-21`). |
| ETR3/PMIR `ach` | `00000000` | known `CWORWRE` bit 18, `CF9GR` bit 20, and `CF9LOCK` bit 31 clear | Reset/ME policy is separate from SCI and must not be bundled into it (`common/pmutil.h:22-25`; `i82801jx/early_init.c:37-45`). |

### PMBASE-relative I/O

| Register | Observed | Exact decode | Access/action |
|---|---:|---|---|
| `PM1_STS +00h` | `0801` | bit 11 `PRBTNOR_STS`; bit 0 `TMROF_STS` | W1C status.  Clear bit 11 alone before SCI; retain bit 0 until a timer experiment intentionally consumes it. |
| `PM1_EN +02h` | `0000` | all defined fixed-event enables clear | Ordinary enable.  Keep zero through table generation and the first `SCI_EN` transition. |
| `PM1_CNT +04h` | `00000000` | `SCI_EN`, `BM_RLD`, and `GBL_RLS` clear; sleep type/enable also zero | Ordinary control.  The FADT exposes only its low 16 bits; use a 16-bit RMW and preserve the reserved upper word (`common/pmutil.h:51-55`; `i82801jx/fadt.c:18-20`). |
| `PM1_TMR +08h` | `00d3fbe9` | one 24-bit-range sample | This transcript does not prove motion.  Do not enable timer-overflow delivery yet. |
| `GPE0_STS +20h/+24h` | `6eff0000` / `00000000` | low bits 16--23, 25--27, 29--30 = GPIO0--7, 9--11, 13--14; high 32 events clear | W1C status; leave intact for provenance until a dedicated acknowledgement step (`common/pmutil.c:116-139`). |
| `GPE0_EN +28h/+2ch` | `00000000` / `00000000` | all 64 GPE enables clear | Ordinary enables; keep zero. |
| `SMI_EN +30h` | `00000000` | global SMI, EOS, APMC, sleep, TCO, periodic, legacy USB, legacy USB2 and Intel USB2 enables all clear | Ordinary enables; keep the complete dword zero while `NO_SMM` is selected (`common/pmutil.h:87-100`).  `BIOS_RLS` bit 7 is not a substitute for `PM1_CNT.SCI_EN`; writing it asserts SCI. |
| `SMI_STS +34h` | `00006100` | bit 14 periodic, bit 13 TCO, bit 8 PM1 | W1C summary/status.  With `SMI_EN=0` it does not prove active SMI delivery.  Preserve until the underlying TCO state is captured. |
| `ALT_GP_SMI_EN +38h` | `0000` | all GPI alternate-SMI enables clear | Ordinary enable; keep zero. |
| `ALT_GP_SMI_STS +3ah` | `6eff` | GPI0--7, 9--11, 13--14 | W1C status; it mirrors the set GPE GPIO indices but does not prove route or enable (`common/pmutil.c:181-204`). |
| `GPE_CNTL +42h` | word read `0000` | low byte `GPE_CNTL=00`; exact ICH10 AML names only its bit 1 (`GPEC`, TCO status control), which is clear; byte `+43h` is reserved | Other bit semantics/access are not established locally; do not repeat the script's 16-bit read as a write (`i82801jx/acpi/ich10.asl:9-21`; `util/inteltool/powermgt.c:149-152`). |

The capture contains no USB GPE status: documented USB1/2/3/4/6 status bits
3, 4, 12, 14, and 32 are clear; shared `AC97/USB5` bit 5 is also clear
(`common/pmutil.c:120-138`).  It likewise contains no legacy-USB or
Intel-USB2 SMI status.  Those facts concern wake/legacy delivery only, not the
PCI interrupt pins of the UHCI/EHCI controllers.

### GPIOBASE-relative I/O

Coreboot defines a set bit in `GPIO_USE_SEL` as GPIO mode, a set bit in
`GP_IO_SEL` as input, and zero as output (`common/gpio.h:10-21`).  Bank 2 bit
zero is GPIO32 (`common/gpio.h:61-94`).  `GP_LVL` is the sampled/current level;
the implementation indexes it by GPIO number (`common/gpio.c:106-146`).

| Register | Observed | Set-bit decode |
|---|---:|---|
| GPIO_USE_SEL | `197e75ff` | GPIO mode: 0--8, 10, 12--14, 17--22, 24, 27, 28; all other bank-1 bits native |
| GP_IO_SEL | `e0ea6fff` | input bits: 0--11, 13, 14, 17, 19, 21--23, 29--31 |
| GP_LVL | `02fceeff` | high bits: 0--7, 9--11, 13--15, 18--23, 25 |
| GPO_BLINK | `00040000` | blink bit 18 set |
| GPI_INV | `00000000` | no bank-1 inversion bit set |
| GPIO_USE_SEL2 | `030300ff` | GPIO mode: GPIO32--39, 48, 49, 56, 57 |
| GP_IO_SEL2 | `0f55fff0` | input: GPIO36--48, 50, 52, 54, 56--59 |
| GP_LVL2 | `15ff00d3` | high: GPIO32, 33, 36, 38, 39, 48--56, 58, 60 |

Direction and level fields have electrical meaning only where the pad is in
GPIO mode.  The census does not identify board nets, safe polarities, or who
else may drive them.  It therefore authorizes no GPIO write.  In particular,
GPIO56 remains input/high and must not be folded into ACPI or USB IRQ work.

## Exact safe next sequence

The following are separate milestones.  A later milestone must not be used to
justify adding its writes to an earlier one.

### 1. ACPI table-only image: no PM/SMI/GPIO write

After the existing memory/resource/IOAPIC-mask gates pass, generate tables
from runtime-decoded state:

- FADT `SCI_INT=9`; PM1 event block `0500`, length 4; PM1 control `0504`,
  length 2; PM timer `0508`, length 4; and, for structural validation only,
  GPE0 block `0520`, length 16.  These offsets and lengths are the exact ICH10
  coreboot layout (`i82801jx/fadt.c:7-29`).
- Keep `SMI_CMD`, `ACPI_ENABLE`, and `ACPI_DISABLE` zero.  The x86 FADT layer
  fills them only when a permanent SMI handler exists
  (`coreboot/src/arch/x86/acpi.c:42-46`), while this board selects `NO_SMM`
  (`src/mainboard/msi/x58_pro_e/Kconfig:5-13`).
- Omit PM2 for now: the common map labels it mobile-only
  (`common/pmutil.h:61-68`).  Do not copy the historical ICH10 FADT flags
  wholesale: it advertises 8042, S4 RTC wake, sleep-button and C2 properties
  that this census does not prove (`i82801jx/fadt.c:31-34`).
- Emit the MADT SCI override as ISA IRQ9 -> GSI9, level/high.  That is the
  exact local ICH PM contract (`common/pmbase.c:108-115`) and produces flags
  `000d` (`src/include/acpi/acpi.h:780-787`).
- Keep all IOAPIC redirection entries and legacy PIC IRQ9 masked and do not
  boot an OS from this inspection image.  With no SMI command, an OS-facing
  table contract requires firmware to have established ACPI mode; the current
  `SCI_EN=0` has not.

### 2. Remove the SCI-storm prerequisite, still with SCI masked

Use a one-shot transaction with the exact LPC/base/decode identity above.
Require `PM1_EN=0`, both `GPE0_EN` halves zero, `SMI_EN=0`,
`ALT_GP_SMI_EN=0`, `GPIO_ROUT=0`, and `PM1_CNT[15:0]=0000`.  Status registers
are asynchronous evidence and should be logged, not used as a durable whole-
register identity tuple.  First census the 8259 masks and ELCR: B06VP requires
the legacy PIC and configures LAPIC LINT0 for ExtINT
(`src/mainboard/msi/x58_pro_e/b06vn_pci.c:3536-3644`), but this PM census did
not read PIC state.  Require slave-PIC IRQ9 masked as well as IOAPIC entry 9.

Then perform exactly one non-reversible W1C write:

```text
outw(PMBASE + PM1_STS, PRBTNOR_STS)   # 0500 <- 0800
```

Read back and require bit 11 clear.  Require every enable register unchanged
and both possible SCI delivery paths still masked.  Do not call
`reset_pm1_status()` or `dump_all_status()`: the former echoes all set PM1
bits, and the latter clears PM1, SMI, GPE, alternate-GPI and TCO history
(`common/pmutil.c:206-212`).  A new asynchronous status bit is a reason to log
and stop, not to broaden the W1C mask.

### 3. Establish ACPI mode with no event source enabled

Only after milestone 2 succeeds, accept exactly low-word `PM1_CNT` `0000`
(new transition) or `0001` (retained target).  With every event enable still
zero, GSI9 masked, and PIC IRQ9 masked, use a 16-bit RMW to set only `SCI_EN`
bit 0 and require readback `0001`; also verify the captured reserved upper word
remains zero.  Do not set `SMI_EN.BIOS_RLS`.

This creates a quiet, retained ACPI-mode state suitable for an OS-ready FADT.
It does not prove interrupt delivery.

### 4. Prove PM timer and one SCI, separately

First repeat a read-only, bounded movement test over the 24-bit PM timer and
require at least one change plus a nonzero wrap-aware delta.  The single
`00d3fbe9` sample in this transcript is address-decode evidence only.  The
existing bounded implementation shows the required mask, sample count and
wrap arithmetic (`src/mainboard/msi/x58_pro_e/b06vn_pci.c:1492-1525`).

For a later SCI-delivery image, program only IOAPIC entry 9, initially masked,
for a dedicated installed handler, BSP destination, fixed delivery, and the
proved level/high electrical contract.  Keep slave-PIC IRQ9 masked so the
same asserted SCI cannot also reach LAPIC LINT0 through B06VP's ExtINT path.
With `TMROF_EN=0`, W1C only `TMROF_STS`; then set only `TMROF_EN`, unmask only
entry 9, and wait with a bounded timeout.  The handler must acknowledge
`TMROF_STS` before LAPIC EOI, count exactly one interrupt, clear `TMROF_EN`,
and remask entry 9.  Retain `SCI_EN=1` only after exact post-state verification.
Do not use GPIO, power-button, periodic-SMI, or TCO sources for the first
delivery proof.

The TCO block at `PMBASE+60h` was not captured even though `SMI_STS.TCO=1`.
A read-only census of `TCO1_STS`, `TCO2_STS`, `TCO1_CNT`, and `TCO2_CNT` is a
prerequisite to any watchdog, TCO-SCI, or TCO-SMI decision.  Keep
`SMI_EN.TCO_EN=0` and `GPE0_EN.TCOSCI_EN=0` meanwhile.

### 5. Keep SMI fail-closed

Do not call `global_smi_enable()`.  It clears all status families and enables
TCO, APMC, sleep SMI, EOS and global SMI
(`common/smi.c:35-75`).  With no installed/relocated permanent SMM handler,
the complete target remains `SMI_EN=00000000`, including USB bits 18, 17 and
3.  Leave `SMI_LOCK` clear.  SMM enablement, SMRAM protection, handler entry,
source-specific acknowledgement, and final locking require a separate
milestone.

### 6. Route USB runtime IRQs through PCI/PIRQ, not PM/SMI

Keep USB wake GPEs and all legacy-USB SMI enables zero.  UHCI/EHCI runtime
interrupts use PCI interrupt pins and ICH DxxIR/PIRQ routing.  Coreboot maps a
device's `PCI_INTERRUPT_PIN` through its DxxIR nibble and emits APIC GSI
`16 + PIRQ index` (`common/rcba_pirq.c:21-45`, `:68-93`); PCI INTx is
level/low (`src/arch/x86/mpspec.c:252-271`).

The earlier immutable B06VP route census (SHA-256
`d07c2b6d3a8f73458578cbf3e1b7d38a38449d8912f3139c3278a07c45bd4429`)
found all legacy `PIRQ[A-H]_ROUT=80` and `D29IR=D26IR=3210`
(`captures/2026-09-06-b06vp-car-ich10-routing-readonly.raw:3-34`).  Thus the
legacy PIC PIRQ paths are disabled, while the RCBA route nibbles map INTA/B/C/D
linearly to PIRQA/B/C/D.  Leave the legacy PIRQ bytes at `80`; do not use the
historical driver's all-IRQ11 shortcut (`i82801jx/lpc.c:51-112`).

Before emitting an exact USB `_PRT`, take a read-only `PCI_INTERRUPT_PIN`
byte for each of D26:F0/F1/F2/F7 and D29:F0/F1/F2/F7.  The present PM census
does not contain those bytes, and the exact ICH10 source in this tree defines
the D26/D29 route-register offsets but not every D26IP/D29IP field.  Therefore
an exact per-controller GSI is not yet an admitted fact.  After that census,
publish only the derived direct GSIs and leave their IOAPIC entries masked for
the OS.  A firmware IRQ test additionally requires controller status/enable
census, a working handler, source acknowledgement before EOI, and unmasking
only the one shared PIRQ GSI under test.

Do not start a USB IRQ test while the separately documented active
over-current condition remains unresolved
(`research/msi/ich10-usb-overcurrent-vbus-audit-2026-09-06.md`).  Clearing a
sticky controller status cannot remove an active electrical input, and the PM
status here does not supersede that observation.

## Explicit uncertainties

- The origin and age of every pending W1C bit are unknown.  The repeated trace
  replays recorded observations; it is not a second hardware-time sample.
- `GEN_PMCON_1[9]`, `GEN_PMCON_2[2,0]`, and all undefined `GPE_CNTL` bits lack
  exact local ICH10 semantics/access definitions.  They are preserve-only.
- `RTC_POWER_FAILED=1` is decoded, but no native RTC-state-machine recovery is
  yet specified or tested.
- PM timer movement and interrupt delivery are not proved by this capture.
- TCO/watchdog control/status and USB controller interrupt status/enables were
  outside the census.
- 8259 mask/ELCR state was outside the census even though B06VP retains the
  PIC-to-LAPIC ExtINT path; both SCI delivery paths must be gated explicitly.
- GPIO register syntax is decoded, but board-net identity, polarity and safe
  output values are not.
- SCI level/high is the current coreboot ICH contract; it still requires a
  bounded target delivery test before an OS boot is authorized.

## Source integrity

Primary decoded source files used above:

```text
2dcfd65b1cf72cd69ee50e789d347e1051679b48e8bc78a764e0d1fb7e743ce6  coreboot/src/southbridge/intel/common/pmutil.h
1fab6d75cf904b5f732366911aaffc3b060f34e75ea3d56823820a8554b76b8d  coreboot/src/southbridge/intel/common/pmutil.c
78c1c96ea50b21b8db53a91fdf4c21c060da17b3d6d2bb85173090542d8f9fc5  coreboot/src/southbridge/intel/common/pmbase.c
01c81494a642ca20435f6b070835be174f4f1347ebd6e179511807aef80d034b  coreboot/src/southbridge/intel/common/gpio.h
df5ccea742198e7c6e965fa8e39be5a06b74dc1fe3620f1fb607ea4acacf7fca  coreboot/src/southbridge/intel/common/gpio.c
4cd86d9277f57a50e3df23526449f330963d9261895bea52bbfb19e41f46fae5  coreboot/src/southbridge/intel/i82801jx/i82801jx.h
94d55ef8e954b1c08d7a76101085c9219e3444107b6507965c732428c086cbf6  coreboot/src/southbridge/intel/i82801jx/lpc.c
98dcbe9569799a73d6f48bc59e643643f02fd65f94612107c8f4cd26509fd58e  coreboot/src/southbridge/intel/i82801jx/fadt.c
b3e855fba5265592a73d9372ae1bb6dfe66d179523b1f630390c7223cbdcd6f9  coreboot/src/arch/x86/acpi.c
1666059f164ccc74f4936263df25b49d409e2b8442a29d8c80ec7097777ddd0d  coreboot/src/southbridge/intel/common/rcba_pirq.c
c46f02fa32882336c7bf17ee4c157332fba0f406f562f129bc316d2e8aba7c34  coreboot/util/inteltool/powermgt.c
3585c96760dd8b372775d108b8d04cf489c7e35c329685e4b66728c1016f2447  coreboot/src/southbridge/intel/i82801jx/early_core.c
f8a5557596334413ffd252e38252daf306950e1dcdfa8e5b5f18c4284ff96a45  coreboot/src/southbridge/intel/i82801jx/early_init.c
b7a75ff19612d02622e4fb45ee58f381923cd024e07024f19d5b3f0493b0e609  coreboot/src/southbridge/intel/i82801jx/acpi/ich10.asl
4007afe76ceeb15eee74568c1d4659c0e6599f67d761ba049c46fef491bd087b  coreboot/src/mainboard/msi/x58_pro_e/Kconfig
b8a3ef574979a90116feb5bbc2d49a01775f9c2b9bf8280db62ec0cd7899880c  coreboot/src/include/acpi/acpi.h
b85bf3f2ce888622284f3dbf1479c0f865d531b373700b2560416bce81bf5c00  coreboot/src/mainboard/msi/x58_pro_e/b06vn_pci.c
7661e4a3b260231b57c88f3fbba7c37b2cee61f285ebd482c81a2caf33fc5bc3  coreboot/src/southbridge/intel/common/smi.c
6aab865f73b9d12b519f711df91b5c66771185722c0e000dca4f0334d7319ad4  coreboot/src/arch/x86/mpspec.c
```
