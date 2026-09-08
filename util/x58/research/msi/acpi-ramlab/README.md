> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../../Documentation/mainboard/msi/x58_pro_e.md).

# X58 ACPI RAM lab 01

An optional, network-loaded **GRUB BIOS diagnostic**, not a coreboot/SeaBIOS
replacement or flash image. This is for the active B06WJ configuration and
Windows `ACPI_BIOS_ERROR (0xA5)` research. RAM/QPI and existing USB operation
remain the accepted foundation; this adds no qualification tests.

## Build and entry

Use one matching GRUB tool/module installation. The vendor-reference Live
Linux was reported to contain GRUB 2.06 tools with BIOS modules under
`/usr/lib64/grub/i386-pc`; recheck availability when building. The local
workstation has GRUB 2.12 tools but no installed BIOS module directory.
Do not mix their modules. From a copy of the repository files:

```sh
bash scripts/build_x58_acpi_ramlab.sh \
  --mkstandalone /usr/bin/grub-mkstandalone \
  --modules-dir /usr/lib64/grub/i386-pc \
  --output-dir /tmp/x58-acpi-ramlab-build01
```

For a minimal transferred set, also supply `--config /path/grub.cfg` and
`--dsdt /path/dsdt-from-rom.aml`. The DSDT is the native, unchanged release
artifact from `builds/experimental/b06wj-release-20260907/`, size 1076 bytes,
SHA256 `63ba8a0d497cc6998bfffd35ea816d7bba5d85f811337ce0bfb21f7c854d7d03`.
It contains no extracted vendor AML. The wrapper rejects other DSDT bytes.
It limits installed/preloaded modules and records input/output hashes,
tool version, command and log. Two independently generated matching outputs
are required before describing a given GRUB/tool pair as byte-reproducible.

Optionally append
`--candidates-dir builds/experimental/acpi-ramlab/b06wj-20260908` to embed
exactly the three native AML experiments named in that directory's manifest.
Each has a concrete SHA256 pin in the wrapper; any missing/mutated candidate
aborts before output creation. Other files in that directory are ignored.
The optional destinations are:

- `(memdisk)/boot/grub/dsdt-vga-only.aml`
- `(memdisk)/boot/grub/dsdt-s0-s5-only.aml`
- `(memdisk)/boot/grub/dsdt-s0-only.aml`

The default build contains only the unchanged baseline DSDT. Adding the
candidate files does **not** change `grub.cfg`, select a candidate, apply an
ACPI override or start a disk. Their source deltas, hashes and offline IASL
checks are documented in the candidate manifest/README; no hardware result
is inferred from embedding them.

The separately generated `legacy-windows` experiment is an independent
option, not a change to those three candidates:

```text
--legacy-windows builds/experimental/acpi-ramlab/b06wj-legacy-windows-20260908/legacy-windows/dsdt.aml
```

It is hash-pinned to
`d0808b3049696dcf329282f54eb1d4f438bb0d207c3df5377d53ff4db53890d2`
and appears at `(memdisk)/boot/grub/dsdt-legacy-windows.aml`. This 1128-byte
native DSDT adds both PCI0 resource producers `A0000–BFFFF` and
`C0000–DFFFF`, with no sleep-object changes. These are declarations, not new
PAM/register programming. It may be embedded alone or alongside the original
three candidates; without its own flag it is absent. No default behavior or
firmware changes follow from embedding it.

Similarly, `--ioh-prt FILE` optionally embeds the independently generated
`builds/experimental/acpi-ramlab/b06wj-ioh-prt-20260908/ioh-prt-only/dsdt.aml`
at `(memdisk)/boot/grub/dsdt-ioh-prt.aml`. The concrete SHA256 pin is
`ebb760c9558dca096b8b1bd21820416c1b04020375afd7512da6d3b421ea8bbf`.
This 1618-byte native candidate changes only the root interrupt-routing
package from 20 to 64 tuples, adding D0/D1/D2/D4–D10/D22, pins 0–3 mapped
to GSI16–19. Existing D3 and ICH tuples are preserved and D13 is excluded.
It adds no VGA windows or sleep objects. The routing expansion is an
experimental description, not a claim that IRQ delivery or Windows boot
has been validated. Embedding several files does not merge their changes:
select at most one whole DSDT per fresh experimental boot.

Serve the resulting `.pxe` from an authorized temporary HTTP/TFTP directory
reachable by the target, without changing its normal DHCP boot policy.
For a disk-chainload experiment, enter the **post-POST/BBS BIOS iPXE** shell:
let the early `Press Ctrl-B to configure iPXE` invitation pass, wait for
SeaBIOS `Booting from ROM...` and iPXE `starting execution...`, then use
`Press Ctrl-B for the iPXE command line...`. SeaBIOS maps BIOS disks in
`bcv_prepboot()` only after option-ROM setup and the interactive boot menu.
GRUB entered from the early configuration prompt can therefore see only its
memdisk even while USB initialization is otherwise working. Then run:

```text
chain http://<authorized-server>:<port>/x58-acpi-ramlab-01.pxe
```

Expected: `X58-ACPI-RAMLAB-01`, live ACPI inventory, the firmware memory map,
BIOS disks/partitions, then a serial-only GRUB command line at COM1
`0x3f8`, 115200 8N1. There is **no automatic disk boot, table replacement,
reset, exit or peripheral-register mutation**. UART console programming and
ordinary BIOS disk reads are required. Commands capable of manual memory,
I/O or ACPI writes are present for later explicitly chosen experiments; this
is not a privilege-enforced read-only monitor.

## First read-only observations

Keep the serial log. `lsacpi` prints RSDP/RSDT/XSDT metadata, checksums and
MADT entries, but not a complete DSDT disassembly or raw table export.
It also does not prove Windows' AML interpreter accepts the namespace.

```text
lsacpi
lsmmap
ls -l
read_word 0x40e
inw 0x504
inl 0x508
inl 0x508
```

`0x40e` is the BDA EBDA segment pointer; multiply it by 16 outside GRUB to
derive the EBDA address. PM1_CNT at `0x504` and PM_TMR at `0x508` follow
the **current B06WJ** `PMBASE=0x500` from `acpi_tables.c` and its hardware
readback, not the vendor's different PMBASE. PM1_CNT bit 0 is SCI_EN;
two timer values only show sampled counter progress, not interrupt delivery.
Do not read unrelated device register ranges speculatively.

`lspci` and `setpci` are also preloaded for a manual PCI interrupt-pin census.
For GRUB 2.06, `grub-core/commands/setpci.c` confirms the following read-only
example (do not assume this BDF is present without checking the census):

```text
setpci -s 00:1f.6 00.l
setpci -s 00:1f.6 3d.b
setpci -s 00:1f.6 08.l
setpci -s 00:1f.6 0e.b
```

These read identity, interrupt pin, class/ProgIF/revision and header type.
`INTERRUPT_PIN` and `HEADER_TYPE` are also accepted symbolic register names.
Use one register operand per command. An absent/not-enumerated BDF may
produce no output rather than an explicit error. Commands without an
assignment read; an `=VALUE` operand writes and is not part of the initial
read-only procedure. An advertised interrupt pin does not alone
establish a board-specific `_PRT` route; correlate it with the actual IRQ
route registers, bridge swizzling and the vendor-live namespace.

Use the actual `lsacpi` RSDT/XSDT address, then its table pointers and lengths:

```text
hexdump --skip=0x<actual-RSDT-address> --length=0x<actual-length> (mem)
hexdump --skip=0x<actual-FACP-address> --length=0x<actual-length> (mem)
hexdump --skip=0x<actual-DSDT-address> --length=0x<actual-length> (mem)
```

FADT contains its 32-bit DSDT pointer at offset 40 and, for a long enough
modern FADT, X_DSDT at offset 140. Decode and check lengths/pointers offline
before following them. Bound initial header dumps to 36 bytes, and full
table dumps to their independently checked size. UART hex output permits
offline reconstruction and IASL validation without writing a target disk.
`hexdump` does not validate arbitrary physical addresses for the operator.

## Later manual control experiment — not automatic

First record original tables and establish that GRUB itself can chainload
the **identified JetFlash** without overriding ACPI. `ls -l` and partition
contents/UUID must distinguish the 128-GB Transcend from the SATA SSD; neither
SeaBIOS menu index 2 nor `hd1` is a permanent identity. Set `root` only after
identification and use the actual partition's existing boot sector. Do not
add an automatic fallback to another drive on failure.

Only after that baseline, a separately chosen attempt can republish the
unchanged release DSDT:

```text
set debug=acpi,mmap
acpi --exclude=DSDT (memdisk)/boot/grub/dsdt-wj.aml
lsacpi
lsmmap
```

This is a **real mutation of RAM ACPI and the EBDA**, even though the AML
content is unchanged. It rebuilds root tables and FADT pointers, preserving
the original FACS address; it is not a no-op. Do not use `--no-ebda` for a
Windows BIOS-chainload test: that restricts visibility to GRUB-aware loaders.
Then manually chainload only the previously identified JetFlash. A successful
table command is not a successful Windows boot. Repack controls separate
GRUB's relocation effects from a future one-hypothesis AML/FADT correction.
No override or chainboot command is embedded in the default configuration.

Do not treat BIOS GRUB `exit` as a return to an interrupted iPXE caller:
GRUB 2.06 `kern/i386/pc/startup.S` calls INT18 and has a firmware-reset
fallback. The pinned iPXE PXE-NBP loader does not hook INT18; SeaBIOS's
INT18 advances the boot sequence rather than completing interrupted POST.
An accidental early entry therefore requires the established controlled
restart, not an invented jump back into the firmware's initialization code.

## Why Windows can see an override, and limits

The [GRUB acpi manual](https://www.gnu.org/software/grub/manual/grub/html_node/acpi.html)
states that the default operation updates the EBDA RSDP. Inspection of the
GRUB source additionally establishes:

- `grub-core/commands/acpi.c`: copies tables into `GRUB_MEMORY_ACPI`, rebuilds
  RSDT/XSDT/RSDP, fixes FADT DSDT/FACS pointers and checksums, and updates
  the BDA EBDA segment pointer at `0x40e`.
- `grub-core/mmap/i386/pc/mmap.c`: reserves the memory and installs INT12/
  INT15 preboot handlers exposing its adjusted BIOS E820 map.
- `grub-core/loader/i386/pc/chainloader.c`: transitions to the boot sector
  without a platform reset or undoing the ACPI operation.

The EBDA-copy strategy is explicitly heuristic upstream. SeaBIOS runtime
compatibility on this board is an experiment, not proven by those sources.
Use one override attempt per boot; do not stack repeated repacks, issue
resets hoping modified tables persist, or publish stock vendor AML with its
unimplemented BIOS data/SMM dependencies. A normal firmware boot reconstructs
the original B06WJ tables. If the lab hangs, recover through the established
target-only one-second AC cycle/guard procedure; never power-cycle ConsolePi.

No firmware was flashed and no hardware success is claimed by these files.
GRUB cannot infer the four missing Windows bugcheck arguments from ACPI
tables. Their direct capture still requires a Windows kernel debugger or a
usable crash dump; a plain UART text logger is not a KDCOM debugger.
