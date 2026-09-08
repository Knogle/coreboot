> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WJ recurring Windows A5: vendor comparison and RAM diagnostic lab

## Scope and current conclusion

The operator confirms that B06WJ still encounters the previously identified
Windows `ACPI_BIOS_ERROR (0xA5)`. Four bugcheck arguments are unavailable.
The task is extensive reference/vendor analysis and experiments on the active
image, with RAM/QPI and working USB accepted as the baseline. No firmware
source, flash chip or boot-medium contents were changed in this session.

Two concrete defects are now established: omitted IOH interrupt routes in
the native root `_PRT`, and a reserved flag bit in the CTBL SSDT resource
descriptor. Both were corrected in target RAM, fully read back, and retained
through a JetFlash MBR handoff. The operator subsequently reports A5 again.
Thus this attempt has no successful Windows outcome; no kernel capture yet
proves which published root/tables Windows selected or the exact bugcheck
subtype. The chronological entries below preserve
the earlier observations and unsuccessful diagnostic entry paths.

## Configuration and immutable evidence

```text
test family: B06WJ-ACPI-RAMLAB
build commit: a2eb375438c85cd7908140636cbee8407bee8c0d plus archived WJ delta
ROM SHA256: 9ad2108df0a88b66384d6833f3c50bf66f2509f2a739baf0c1b05509b476bd06
flash chip: socketed W25Q128.V..M, 16 MiB; no fresh programmer readback
board: MSI X58 Pro-E / MS-7522; PCB revision not restamped
CPU: E5645 / CPUID 000206c2 / stepping 2 / associated microcode 0000001f
DIMM: sole SPD54, BLS4G3D1609DS1S00, 4 GiB, 2Rx8; physical slot not restamped
GPU: HD5450 1002:68f9, physical option ROM
PSU: unchanged; exact model not restamped
serial: operator@serial-gateway.example.invalid /dev/ttyUSB1, COM1 3f8, 115200 8N1
reference: root@192.0.2.203, vendor V8.14B8 11/09/2012, Linux 6.8.0-31-generic
reference differences: 12 GiB/12 threads and additional PCI devices; not the target's population
recovery: target-only Shelly 192.0.2.215 switch 0, one-second OFF/timed ON
```

The local full-chip ROM hash was rechecked unchanged. A running banner and
SMBIOS identify WJ; that is not an independent external flash readback.
ConsolePi was never reset. The proprietary MSI/Intel bytes and vendor ACPI
captures stay in ignored `blobs-local/` directories.

## Fresh reference captures

Two read-only collections are retained as
`blobs-local/msi-x58-pro-e/x58-acpi-a5-reference-20260908-01/` and `-02/`.
They include the live ACPI tables, PCI configuration, RCBA, GPIO, PM state,
interrupts, memory/I/O maps and platform identification. The static ICH route
words and GPIO policy agree with the earlier vendor collections; PM timer
values are runtime observations, not configuration to clone.

- D31IR/D29IR/D28IR/D27IR/D26IR: `0232/0237/3201/3216/3250`.
- D26IP: `30000421`; ICH IOAPIC decode: `03`.
- Vendor PMBASE/GPIOBASE: `800/500`; WJ deliberately uses `500/580`.
- GPIO USE/IO words: `19fdff01/e0ff2cc3`, bank 2 `030300ff/0d54fffe`.

These observations do not justify blanket writes to GPIO levels, SMI enables,
GPE state or OS-programmed interrupt-controller entries.

The additional read-only [NVS collector](../../scripts/capture_x58_vendor_acpi_nvs.py)
checks MSI DSDT identity/checksums, the exact AML region encoding, the FACS
relationship and the kernel's NVS allocation before reading 255 bytes.
It establishes a genuine runtime patch:

```text
MSI DSDT BIOS OperationRegion: BF78E064, length FF
FACS: BF78E000; region starts at FACS + 64 hex
live DSDT SHA256: 99f49927294b2217ec1e6de06df3c8b16c6be7508b45277a7771cca71b380e8f
255-byte NVS SHA256: d9bc49365e3029e1fe08304c6aacd6e7ce657ff7d6f349010716e88fc0c6d28d
```

The raw NVS evidence is in
`blobs-local/msi-x58-pro-e/x58-acpi-nvs-reference-20260908-01/`. This proves
that the ROM's `FFFFFF00` placeholder is not the live RAM base; it does not
reconstruct every original firmware patcher or SMM service.

The separate [Intel DX58SO/SO2/OG comparison](../intel/acpi-x58-wj-comparison-2026-09-08.md)
records deterministic extraction of actual EFI ACPI modules. Fixed-table
templates have unfilled pointers/checksums and the DSDTs contain FIX0..FIX4
policy/address markers. Their semantics are references, not loadable tables
or a justification to execute the Intel ACPI DXE driver under SeaBIOS.

## First target RAM diagnostic execution

After a target-only one-second AC cycle, the early iPXE configuration shell
was entered. DHCP succeeded. A native standalone GRUB 2.06 BIOS-PXE image was
loaded from an allowlisted, temporary HTTP server on the reference host:

```text
artifact: builds/experimental/x58-acpi-ramlab-build-20260908-03/x58-acpi-ramlab-01.pxe
size: 277686 bytes
SHA256: c2e95c5e245c385ffc9419eaf97901d13fbc6838a12d33345bb149233891117c
automatic actions: serial console, lsacpi, lsmmap, BIOS disk inventory
automatic table overrides/register experiments/OS boots: none
```

This is a network-loaded diagnostic, not a new firmware release. Optional
native candidate AMLs are embedded as inert files. The original WJ DSDT is
hash-pinned. [Builder and manual procedure](acpi-ramlab/README.md) describe
the real EBDA/E820 side effects of an eventual manual ACPI replacement.

Actual hardware captures, each with an accompanying `.raw.json`, are:

- [Fresh WJ to early iPXE](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
- [HTTP/GRUB execution](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
- [SCI, PM timer and D31:F6 census](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
- [Root-table bytes and all PCI interrupt pins](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
- [FADT/SSDT bytes and PCI identities](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
- [DSDT/FACS/MCFG/MADT/SPCR/HPET/RSDP bytes](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

Long iPXE commands initially lost characters when sent as a burst. Pacing at
30 ms per byte and prompt-gating produced complete echoed commands. The
failed burst attempt is not interpreted as a chipset or NIC failure.

## Actual tables and registers, before any override

GRUB finds the valid revision-2 RSDP, RSDT `01676030`, XSDT `016760e0`, and
valid checksums for FADT (276 bytes/revision 6), SSDT, MCFG, MADT, SPCR and
HPET. Its `No RSDPv1` output means no separate revision-0 root pointer was
found; the revision-2 RSDP contains a nonzero RSDT pointer and a valid legacy
checksum. It is not evidence that BIOS ACPI is absent.

- FADT points to FACS `01676240` and DSDT `01676280` through both legacy and
  extended fields; legacy/extended PM addresses describe WJ's `500` base.
- MADT describes the one advertised BSP, APIC ID 0/UID 0, IOAPIC ID 1 at
  `fec00000`, IRQ0→GSI2 and high/level IRQ9→GSI9.
- PM1_CNT read at `504`: `0001`, so SCI_EN is set.
- PM timer samples at `508`: `0055cf77` then `006645a7`; the counter advances.
- These checks do not prove SCI interrupt delivery, all AML evaluations,
  the exact timer frequency, or Windows compatibility.

### Interrupt-route mismatch

| Target PCI functions | Actual interrupt pin | Released WJ `_PRT` | Vendor direct-GSI contract |
|---|---|---|---|
| `00:00.0` and `00:01.0..00:0a.0` | A on every observed function | Only D3 covered | D0..D10, A/B/C/D → GSI16/17/18/19 |
| `00:16.0..00:16.7` | A/B/C/D/A/B/C/D | D22 absent | D22, A/B/C/D → GSI16/17/18/19 |
| `00:1f.6`, ID `8086:3a32` | C | D31/C → GSI18 present | No missing D31/D route demonstrated |
| `00:0d.7`, ID `8086:341b`, host-bridge class | A | No entry | Vendor APIC package also omits D13; do not invent a GSI |

The first two rows reveal omitted published routes, not a need to repeat
memory training. MSI's route package supplies a concrete reference for a
clean-room DSDT-only extension. The Microsoft
[A5 subtype documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0xa5--acpi-bios-error)
includes missing interrupt-routing mappings among possible causes. The
operator's stop-code-only observation cannot establish that subtype.

Separately, the native PCI-root `_CRS` omits the legacy producer ranges
`A0000..BFFFF` and `C0000..DFFFF` that the MSI runtime describes. Native
isolated candidates for those ranges and the missing S0/S5 state packages
have been built and checked offline. They must remain separate hypotheses
from the IRQ-table test; no successful Windows result is implied.

## Early-entry limitation and next controlled attempt

The first GRUB run was entered from iPXE's **option-ROM configuration**
prompt, before SeaBIOS `bcv_prepboot` assigns BIOS disks. Consequently only
`memdisk` was usable and BIOS disk 80 probes failed. This is an entry-phase
limitation, not proof of a USB regression. GRUB's BIOS `exit` uses INT18 with
a reset fallback; it does not safely return to the interrupted SeaBIOS POST.

A second target-only one-second cycle was therefore initiated with a fresh
bounded capture. It ignores the early configuration prompt and sends Ctrl-B
only after `Booting from ROM...`, iPXE execution, and the later command-line
invitation. The purpose is to load the same diagnostic after BIOS disk setup,
identify JetFlash by observed partitions/contents and test an isolated RAM
ACPI change. Its outcome will be appended, not inferred from the first run.

Current recovery boundary: the ROM and original AML are unchanged; a normal
firmware boot reconstructs the original table set. No permanent correction
or completed Windows boot has yet been established by this report.

## Additional concrete SSDT finding and revised experimental scope

The [byte-level reconstruction](b06wj-runtime-acpi-reconstruction-2026-09-08.md)
recovered all eleven structures and verified the DSDT's exact release hash.
It also found a separate encoding defect in the 113-byte SSDT's `CTBL._CRS`:
the DWORD-address descriptor has general flags `1c`, including reserved bit
4. The common coreboot constant `ADDR_SPACE_GENERAL_FLAG_CONSUMER=0x10`
produces this value. It is not a board-specific register inference or a bad
memory read; the source and captured bytes agree. The ACPI specification
requires that reserved bit to be zero.

For the captured original SSDT (SHA256
`ddf9c8e8c8938f0ff49abb0613fe633ea9a0a27ff389b6fb2a460f782df6dd90`), the
byte-minimal historical consumer correction is offset `59: 1c→0d` with
checksum offset `09: aa→b9`. No address/range/AML length changes are needed.
The exact table and address must be rechecked in the current boot before any
RAM write. This finding is independent of the missing IOH routes.

The next intended Windows attempt now groups **two demonstrated table
defects**: the native IOH `_PRT` extension and that SSDT descriptor correction.
It also uses GRUB's ACPI republishing/EBDA mechanism. This is an intentionally
grouped forward-progress experiment, not a one-variable A/B result. Even if
Windows progresses, isolated follow-ups would be needed to attribute the
previous A5 to one correction rather than the other or to table relocation.
The legacy VGA and S0/S5 hypotheses remain unapplied.

## Second boot capture limitation, appended

The second logger correctly ignored the early configuration prompt but its
strict three-dot match missed interleaved SeaBIOS text:
`Booting from ROM..Booting from cf00:0386`. The actual iPXE execution and late
invitation then followed. This is a capture-gate defect, not a target boot
failure. SATA was mapped to BIOS disk 0, JetFlash to disk 1; netboot.xyz ran.

At +505.132 seconds the exact logger process was intentionally interrupted
with SIGINT to retain metadata and release the UART. No reset followed.
Ctrl-C in netboot's menu attempted its local-disk default (reported failure),
then reloaded the HTTPS menu. An individually observed sequence of down-arrow
steps selected the explicit iPXE shell; Enter reached its prompt and
`show product` reconfirmed B06WJ. The diagnostic is therefore now being
entered through the completed BIOS boot phase, not the early option-ROM hook.
The new helper revision for future runs accepts the observed ROM prefix;
it does not rewrite the deployed helper or this raw history.

## RAM corrections actually applied, 08:34 UTC boot attempt

After SeaBIOS disk setup, GRUB identified the 120818688-KiB JetFlash as
`hd1`, with NTFS partition label `CCCOMA_X64FRE_EN-GB_DV9` (UUID
`66EE442EEE43F539`) and a second FAT `RUFUS_BOOT` partition. Its observed
files include `bootmgr`, `setup.exe` and `sources`. This is a Windows setup
medium; its identity must not be assumed to be the earlier Hiren's image.
The SATA Crucial drive was `hd0`. No installation or disk-write action was
issued.

The active diagnostic was the late-entry standalone image in
`builds/experimental/x58-acpi-ramlab-build-20260908-04/`, 277886 bytes,
SHA256 `8a624928116fcdf23f5dc7069234cc628af33b05c2552c4dfa43e8318ac15c24`.
It uses the existing SeaBIOS disk services, not a new USB or AHCI driver.
Its temporary HTTP server on the reference was terminated after transfer;
no persistent network service or DHCP change was made.

### 1. Byte-minimal CTBL SSDT correction

Before either write, a complete read of the current 113-byte SSDT at
`016767e0` reconfirmed the exact original hash listed above. The only writes
to this original table were:

```text
01676839: 1c -> 0d  (descriptor general flags, table offset 59)
016767e9: aa -> b9  (whole-table checksum, offset 09)
```

The complete after-write readback has SHA256
`08522e71ad8df4fe7c301e60989e64c515a9437f0a248dcdeec5be0107a58033`
and a valid checksum. Its addresses, sizes and AML length are unchanged.
The [flag-write capture](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
and [checksum/full-readback capture](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
retain the actual transmitted commands and observed bytes.

### 2. Native IOH `_PRT` extension

The native candidate adds 44 vendor-referenced routing tuples (20 to 64),
covering D0, D1, D2, D4..D10 and D22, pins A..D to GSI16..19. Existing
D3/ICH routes remain unchanged. The exact 1618-byte candidate is
`builds/experimental/acpi-ramlab/b06wj-ioh-prt-20260908/ioh-prt-only/dsdt.aml`,
SHA256 `ebb760c9558dca096b8b1bd21820416c1b04020375afd7512da6d3b421ea8bbf`.
It was published with:

```text
acpi --exclude=DSDT (memdisk)/boot/grub/dsdt-ioh-prt.aml
```

GRUB copied the other live tables, including the already repaired SSDT,
and rewrote both FADT DSDT pointers. The full actual DSDT readback matches
the candidate hash exactly. No vendor AML was transplanted. No new GPIO,
SMI, IOAPIC redirection or PCI routing-register write was made in this step.
The [republication capture](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
records the command and resulting EBDA change.

### 3. Publication checks and GRUB side effects

GRUB moved the EBDA from `9e800` to `96400`; BDA `40e` became `9640`, and
the new revision-2 RSDP is at `96500`. The new DSDT starts at `159a1d0`.
GRUB prepends copied tables to a list, reversing the original root order.
To preserve FADT as the first entry, the first and last entries were swapped
in both RSDT and XSDT; no table was added or removed. Since the entry-byte
multiset is unchanged, both checksums remain valid. Final order is
FADT, SPCR, APIC, MCFG, SSDT, HPET. All XSDT upper DWORDs were checked as zero.

The exact four DWORD writes, valid only for this captured allocation, were:

```text
159aaeb <- 159a9b3   RSDT first entry: FADT
159aaff <- 159a822   RSDT last entry: HPET
159ab27 <- 159a9b3   XSDT first entry, low DWORD: FADT
159ab4f <- 159a822   XSDT last entry, low DWORD: HPET
```

No subsequent `acpi` command was issued. In GRUB 2.06 table generation is
immediate, not a repeated action at `boot`; the later hook supplies the
modified memory map instead. [GRUB ACPI implementation](https://github.com/rhboot/grub2/blob/grub-2.06/grub-core/commands/acpi.c#L716-L755)

The strict reconstruction in
`builds/experimental/b06wj-acpi-final-for-jetflash-20260908/manifest.json`
contains all eleven final structures, with no incomplete bytes, valid
checksums wherever defined, matching RSDT/XSDT entries, and matching legacy/
extended FADT DSDT and FACS pointers. FACS stays at `1676240`; the other
tables are copied, not borrowed from the reference's different CPU topology.
Evidence: [root and DSDT readback](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
and [remaining tables, roots and MBR load](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

### JetFlash handoff and present outcome

`set root=(hd1)` and `chainloader +1` completed without a GRUB error. At
08:34:59 UTC `boot` was issued with `debug=mmap`, followed by a bounded
180-second passive capture. The hook reports:

```text
installing preboot handlers
mmap chunk 96400-97000:2
mmap chunk 159a1d0-159ab8f:3
hooktarget = 0x96c00
Press any key to boot from USB.....
```

This proves that the ACPI RAM region was passed as type 3 (ACPI reclaimable)
and the EBDA/hook region as reserved in GRUB's installed E820 path. It is
stronger evidence than `lsmmap`, which shows only the unmodified firmware map.
It does not prove that Windows subsequently consumed the tables.

The full [boot capture](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) is
1695 bytes, SHA256
`af7161b94affffa9edfd8b5840d09d5e13d06e916a4f234a27d001b5c0300d9c`.
No additional key was sent after `boot`; the capture then contains two
SeaBIOS `invalid handle_legacy_disk:729` messages for AH15/DL00 and DL01.
These are disk-type queries for absent BIOS floppy mappings, not evidence
of a USB data-transfer failure or a confirmed Windows launch. The operator
has been asked to report the current display and accept the USB invitation
if still present. Serial silence is not classified as a successful OS boot.

The corrections are RAM-only and are lost on a firmware restart. The latest
flash artifact remains B06WJ with its original hash. Legacy VGA windows,
S0/S5 and the FADT power-button contract remain separate, unapplied
hypotheses. This grouped two-defect correction plus GRUB relocation cannot
isolate one cause even if the next screen shows Windows progress. If A5
persists, the four bugcheck arguments remain the most direct discriminator.

## Host validation and handoff

The diagnostic builder contracts pass 19/19 host tests; the strict binary
reconstructor passes 9/9. These tests exercise file handling, hash pins,
optional candidate isolation, complete-byte reconstruction and ACPI checksum
rules; they do not count as hardware or Windows passes. No full firmware
rebuild or repeated RAM/QPI qualification was performed for these checks.

The matching SeaBIOS source at commit
`5497f43189374647b3b0f282aa497e71c891f3c5`, `src/disk.c:709..729`, confirms
that the two AH15 messages take the missing-floppy-mapping path. The dump is
printed before the error return (AH01/carry set), so its input register
values must not be interpreted as the returned status. Without identifying
the loaded code at CS:IP `8e40:7d82`, these messages cannot distinguish a
USB invitation timeout from subsequent loader execution.

The UART capture has finished and released the port. There is no background
reset loop and no further automated input. The board is left in its current
post-handoff state for the operator's screen report. A confirmed graphical
result is needed before deciding whether to preserve this RAM experiment or
restart into another deliberately separated candidate.

## Operator-requested USB confirmation key, 08:49 UTC

The operator explicitly requested one keypress to accept the JetFlash USB
boot with the current RAM corrections. A three-second passive precheck
received no bytes. At `2026-09-08T08:49:37.866373+00:00`, exactly one ASCII
space (`20`) was transmitted through COM1; the next 60 seconds also produced
no serial bytes. Both helpers exited normally and released the port.

Evidence: [passive precheck metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
and [single-key metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
with their immutable, empty RX captures alongside. This confirms the serial
transmission, not that the USB loader accepted it: its earlier key window
may already have expired. No further key, reset, ACPI mutation or register
write was issued. The hardware configuration and intended ROM are unchanged
from B06WJ-ACPI-RAMLAB-01; the graphic/Windows result still requires the
operator's screen report.

## Operator confirms A5 again after the RAM experiment

The next operator report is `A5 ACPI BIOS error again` (translated from German). Record this as an
OS failure for the attempted corrected-table JetFlash path, not as a pending
screen outcome or a successful correction. The source is the user's screen
observation; the serial log itself contains no Windows bugcheck packet.

No reset or additional hardware action was performed in response. The two
structural findings remain valid and their pre-handoff RAM corrections are
fully evidenced. However, the repeated top-level code does not prove that
Windows stopped on the same ACPI object, that both original defects were
irrelevant, or that Windows actually consumed GRUB's replacement RSDP rather
than another still-discoverable root. Those require a kernel-level capture.
The late space transmission also cannot prove acceptance of the earlier
USB invitation. No new RAM/QPI stability conclusion follows.

The current FADT still sets `POWER_BUTTON` (a control-method device instead
of the fixed button), while the DSDT provides no `PNP0C0C`; this is a remaining
contract inconsistency, not a demonstrated fatal subtype. The PCI root also
still lacks the vendor/open-Intel legacy VGA producer windows. Neither
candidate was applied in the reported experiment. Changing GPIOs wholesale
or transplanting the MSI SMM-dependent namespace would not resolve this
uncertainty.

The next discriminating capture is Windows kernel debugging, preferably
without changing the medium: on a separately authorized next boot, accept
the USB invitation promptly, then try F8 -> debugging in Windows Boot
Manager with an actual KD/WinDbg endpoint already attached. Microsoft
documents the WinPE F8 alternative and default serial COM1/115200 settings.
Whether this particular setup image exposes that menu and retains those
BCD defaults has not been checked. [WinPE kernel debugging](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/winpe-debug-apps?view=windows-11)

A normal serial text logger is not that debugger. The kernel debug transport
must be enabled before boot, and the debugger must own the UART connection
without concurrent ROMMON/picocom/capture input. At the stop, obtain
`.bugcheck` and `!analyze -v`, then inspect the root/tables the kernel used.
Parameter 1 distinguishes descriptor parsing, routing, namespace, ACPI mode
and other failures; the other three parameters refine that distinction.
[Microsoft A5 parameter reference](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0xa5--acpi-bios-error)

If F8 is unavailable, preparing a separate debug BCD/media copy needs a
Windows tooling path and an explicit target. No original BCD, boot medium,
registry hive or firmware image was modified here. The operator has been
asked whether a Windows PC/VM with WinDbg is available. This does not assume
an already-working target Windows session or a saved crash dump.

## Next user-selected direction: Linux live medium

The operator offers to prepare a Linux image. The immediate next experiment
therefore becomes a BIOS/SeaBIOS-compatible x86-64 live USB boot, not a
Windows debugger setup and not replacing the motherboard's flash with an
OS image. A separate expendable USB preserves the Windows medium if available.
No restart or media write has been performed by the agent on this offer.

First collect the normal ACPI-enabled kernel boot log, the actual tables in
`/sys/firmware/acpi/tables`, `/proc/interrupts`, `/proc/iomem`, and PCI
enumeration/resources. Do not start with `acpi=off`, `pci=noacpi` or
`noapic`, which would bypass the interfaces under investigation. A Linux
boot would be valuable platform evidence but would not by itself explain
Windows's A5 or prove complete ACPI correctness.

The reset boundary must be explicit: a firmware restart reconstructs stock
B06WJ tables and removes the RAM-only changes. Label that Linux run as a
stock-WJ baseline unless the corrections are reapplied and read back. If
overrides are used again, verify the kernel's selected DSDT and SSDT hashes
against the candidate hashes rather than assuming the handoff selected them.
COM1 remains `3f8`, 115200/8N1; capture should be armed before the agreed boot.

Windows KD remains a fallback, not a current blocker. Locally installed
radare2 5.9.8 has a known-build profile limit (latest profile 18362), while
Rizin 0.7.4 can report an unknown build but requires a matching kernel/PDB
for its complete attach path. No serial bridge or KD handshake was started.
