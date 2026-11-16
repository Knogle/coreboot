# Experimental Matisse and Vermeer desktop backends

Select `CONFIG_B565D4_MATISSE` or `CONFIG_B565D4_VERMEER` using a normal profile:

* `configs/config.asrockrack_b565d4_v1l_seabios_matisse`
* `configs/config.asrockrack_b565d4_v1l_edk2_matisse`
* `configs/config.asrockrack_b565d4_v1l_seabios_vermeer`
* `configs/config.asrockrack_b565d4_v1l_edk2_vermeer`

These select the shared desktop register interface and device graph through
`CONFIG_B565D4_DESKTOP_NATIVE`, with separate CPU admission. They are
experimental desktop paths. Physical milestones include the Matisse 3700X
and Vermeer 5600X B2. On 2026-10-31, canonical Vermeer SeaBIOS61 and EDK2/CSM62
profiles boot the SATA Proxmox installation with all twelve threads on
microcode 0a201213. Both use the complete original F20 Vermeer ABL described
below. The single 32-GiB ECC population has bounded EDAC/MCA, ACPI, RTC and
120-second KVM guest verification evidence under both payloads. Longer
tests on older images do not qualify this new firmware cohort. Native NVMe
UEFI and legacy CorebootOS boots were separately observed on earlier images.
These are ALPHA milestones, not production qualification. See
the current acceptance matrix (`../../../../../porting/asrock_am4/PRODUCTION_READINESS_20261030.md`; external historical evidence) and
the physical Matisse report (`../../../../../porting/asrock_am4/MATISSE_PHYSICAL_20261028.md`; external historical evidence).
They do not automatically detect and switch between Cezanne, Matisse and
Vermeer at runtime. The default Cezanne profiles keep their original CPU admission.

Matisse admits Family 17h model 70h/71h and Vermeer admits Family 19h model
20h/21h before the common pre-C code's first MSR access. Matisse uses its
observed version-23/108 APOB and two four-core complexes; Vermeer uses
version24/f8 and one eight-core complex. Later code uses the selected APOB
topology, `30081018` launch state, SMU11 `530/57c/9c4` mailbox, desktop NBIO/DXIO
and USB programs, and the desktop PCI device layout. An admitted CPUID alone
does not establish that those later operations execute successfully.

The retained PSP/ABL/APCB firmware owns DRAM initialization. The canonical
Vermeer profiles replace only its active Vermeer ABL with the complete
original F20 unit; the remaining PSP code, keys, APCB and fuse policy retain
their qualified bytes. Matisse and Cezanne keep their existing selection.
Bootblock,
ROMstage, checked memory ownership, rmodule load, RAMstage and payload handoff
remain on the normal board build path. There is no new x86 AGESA/FSP call or
cache-as-RAM path. The shared Cezanne build umbrella and configuration type are
still used to reuse common DRAM-first and board code; splitting those common
parts into a proper desktop SoC is remaining architectural work.

## Vermeer firmware inputs

The canonical Vermeer profiles select `B565D4_VERMEER_ABL_F20`, which selects
the paired original A21213 boot microcode and the large payload layout for
both SeaBIOS and EDK2. The microcode directory is
`3rdparty/blobs/mainboard/asrockrack/b565d4_v1l/desktop-microcode-vermeer`.
The ABL file is `vermeer-f20-abl.bin` in that board blob directory, with
511680 bytes and SHA256
`f67f88a0bcf1949dae9c1f74cb9a889e4c645f83d9eb4ebe7bbd36b741862f5a`.
The normal image packer validates the complete unit, its twenty stages,
four directory updates and two RAW FFS storage files. It rejects an
unpaired boot-microcode or ABL input. The surrounding firmware volumes must
remain structurally valid; merely placing the unit in erased bytes is
insufficient. These checks do not independently verify its cryptographic
signature or qualify another processor stepping.

With the old ABL, the first x86 observation reported microcode zero and the
original13 loader stopped before returning. With the new complete ABL,
the first x86 observation reports 0a201212 and the ordinary coreboot AMD
loader completes the update to 0a201213 on all twelve OS threads. This
whole-unit comparison establishes a working initialization combination;
the precise internal ABL failure mechanism was not traced. The experimental
vendor pointer-handoff helper is disabled and absent from these linked
images. No PSP authentication, key or provisioning policy was changed.

## Source derivation

The native `matisse_*` components were first developed and compared with the
retained board firmware under `porting/asrock_am4/matisse`. The original board
CPM selector chooses this desktop policy for Matisse and Vermeer. Their names
retain that research provenance. The research copies remain unchanged.

* `platform.c` composes the admitted NBIO, USB and two-phase DXIO programs with
  actual coreboot SMN/SMU access, a bounded CBMEM DMA allocation and normal PCI
  enumeration.
* `fabric.c` programs only the known low MMIO window 0, IO window 0 and PCI
  config map 0. It uses desktop destination ID 4, preserves other windows and
  supplies the ordinary resource allocator. It neither assumes the total MMIO
  window count nor enables an unqualified high-MMIO window.
* `iohc.c` derives PSP/internal-NBIF/IOAPIC setup from the ASRock
  AmdSocAm4MtsPei and AmdNbioBaseSspPei paths. Existing valid PSP/NBIF mappings
  and a nonzero matching PSP address MSR are preserved. The internal NBIF
  aperture is separate from the ordinary PCI IOMMU MMIO allocation. The CCP
  function is hidden using desktop strap `10834200` at payload entry.
* `promontory.c` uses the existing board firmware upload with desktop root
  identity and routing. C51C bit0 follows the original extended-family and
  CPUID-low-byte selection, including stepping: actual 3700X 00870f10 sets it
  while Vermeer clears it. Linked native code is compared with the original
  CPUID helper and branch under explicit CPU/indirect-read models.
* `cpu.c` retains architectural AMD CPU initialization and checks every CPU's
  fresh SMM exclusions against the published RAM resources. It does not change
  SMM ownership or unlock SMM.
* `cpu_power.c` enables the original Vermeer CPU-clock and protection-feature
  subset `0x13f`, once, after CPU/PCI initialization and before ACPI tables.
  Its SMU version and CPU guards retain boot on unsupported combinations;
  they skip the power policy and its P-state advertisement. A live5600X B2
  SMU00384c00 trial changed measured loaded frequencies from a fixed2.2GHz to
  the requested2.2/2.8GHz and stock boost (about4.31GHz across twelve threads,
  4.565GHz in the single-thread sample). This live diagnostic is separate from
  qualification of the firmware callback. No OS module belongs in a release.
  Normal26 subsequently passed a genuine cold start without an OS module:
  RAW-clock counters measured about4.63GHz single-thread and4.40GHz on twelve
  loaded threads. Its sustained combined load reached95.6C and was stopped;
  this is not endurance qualification. A separate original-SMU384c BIOS3e
  runtime request85C passed CPU12/locked24GiB verify120 with the measured
  temperature recorded in the production controls. The next native callback
  requests the configurable60..90C ceiling (default85C) after enabling the
  feature subset. The earlier native28 request before activation was accepted
  but ineffective under cold-boot load; the93C guard stopped that test after
  about15seconds. A failed ceiling request skips P-state advertisement.
  Normal30 subsequently cold-qualified the corrected order and passed a
  one-hour host / half-hour guest overlap test. New firmware cohorts still
  require their own endurance tests; the earlier runtime120s result alone
  does not qualify them.
  The existing feature/thermal requests do not add manual voltage, clock,
  PPT/TDC/EDC, fan, fabric or deeper idle overrides.
  An ALPHA experiment initializes the original SMU384c CPPC service
  through feature22 after the temperature request, then reads its information
  and nominal/lowest-frequency replies. Every failure leaves boot and the
  already prepared hardware P-states available; requests are not retried.
  The information word is a firmware capability constant, not proof of a
  working OS interface. PCC allocation/cacheability, CPU indexing, table
  registration and ACPI publication remain separate work. This experiment
  does not expose `_CPC` or change the OS scaling driver. Physical execution
  and its effects require evidence from each actual firmware cohort.
* Hardware P-state tables reuse the common AMD ACPI generator. APIC-sorted
  domain indices are checked against actual package/core/thread topology,
  independently of MADT UID order. The SMU acknowledgement is not a feature
  bitmap or proof of operating frequency; physical OS measurements qualify it.
* The common reset observer in `ramstage/native132_core_launch.c` selects the
  desktop launch register, mailbox and APOB decoder. It excludes BSP bit 0
  from the desktop AP release request. Working references have Matisse `fffe`
  with sixteen threads and Vermeer `ffe` with twelve threads online.

The Matisse direct-memory adapter preserves SME enable when the actual CPU
mode is unpaged IA32 with PAE clear, a supported SME CPUID leaf and a C-bit
above the 32-bit address range. It does not enable encryption or convert
encrypted mappings. Cezanne/Vermeer retain their previous SME restriction.

See `porting/asrock_am4/VERMEER_STAGE_INTEGRATION_20261027.md` for original
module addresses, the reference captures, build hashes and test scope.
See `porting/asrock_am4/MATISSE_STAGE_INTEGRATION_20261028.md` for the Matisse
integration, current shared source names, tests and candidate images.

## Initial scope

The initial topology adapter accepts one desktop CCD and up to sixteen logical
threads. Multi-CCD Matisse/Vermeer, older AM4 CPUs, S3/deep idle, CPPC,
full SMU power policy, high-MMIO allocation and extended CMOS/CFR desktop
setup remain open. The ordinary profiles default to x16 and expose the existing
bounded CMOS x16/four-x4 policy; per-CPU physical qualification is separate.
The Cezanne x8/x4/x4 and allocation-stop probe are not offered for this backend.
The normal Vermeer EDK2 profile uses the board's bounded FCH_SPI variable
provider; cold retention, reclamation and owned-probe deletion were tested.

Preserve each CPU family's qualified return images and the current target's
actual NVAR. Follow all current bench gates in `.site-local/AGENTS.md`.
The native PCC probe follows the original CcxZen3Dxe allocation and cache
contract: a private128-KiB CBMEM owner contains a64-KiB aligned shared interval.
It checks all CPU cache snapshots before using one common unused variable
MTRR, flushes initialization and verifies the UC mapping on every CPU. No
PSP, ABL or SMM reservation is repurposed. Failed setup attempts restore only
the newly introduced MTRR. The private CBMEM ID must be absent before allocation,
and the actual new entry must match the owner pointer and full requested size;
an existing ID is not accepted merely because `cbmem_add` returns a pointer.
An allocation is retained after an ambiguous SMU
reply. Original SMU384c message16 registers the address directly, followed by
one finite PCC read. Registration-header and per-CPU capability failures log
and return without blocking boot. This probe publishes neither PCCT nor
`_CPC`; ordinary hardware P-states remain available. The new memory/cache/
doorbell path needs its own physical qualification before OS publication.



The next PowerPlay ALPHA candidate uses a native772-byte producer matching
SmuV11DxeVMR38BCh under the board's compiled scalar PCD defaults. It retains
zero stock automatic voltage/clock/PPT/TDC/EDC overrides, the exact compiled
platform-TDP field1000 and CPU/SoC telemetry calibration140/75, and the
original unconditional0101h bytes. It fills the count7Ch and256-word APIC
array80h from enabled initialized CPU devices in ascending APIC order, rather
than MADT UID order. These are board-specific values, not generic AM4 defaults.
The original runtime Setup and protocol-hook6424h overrides are not imported.

A fresh exact4-KiB CBMEM owner retains the staging table for its entire SMU
lifetime, including failures. The original BIOS message5 receives its low/high
address in arguments0/1; after the original cache flush, message10h imports
304h bytes. These are the Vermeer BIOS-queue opcodes, not the Cezanne split
address5/6 convention. Both requests use the common bounded coreboot SMU
transport once. The import precedes feature13Fh, the configurable temperature
request and CPPC service22. An import failure skips the optional CPPC probe;
ordinary hardware P-state policy is still attempted, with no diagnostic halt.

Normal95 physically established an owned all-CPU-UC PCC interval and a
completed header transaction, but every CPU capability word stayed zero.
The missing PowerPlay CPU/APIC table is a hypothesis for that incomplete data,
not a proven cause. The producer and modeled native import pass HOST checks;
the new table transfer remains an unqualified ALPHA experiment until actual
cold-boot, CPU data, MCA/EDAC, temperature and host/guest tests are captured.
It publishes neither PCCT nor `_CPC`, and does not change the OS scaling driver.


The next ACPI ALPHA candidate publishes the validated shared-memory CPPC
contract through ordinary coreboot PCCT and per-CPU `_CPC` generators. The
common PCCT generator runs before the CPU SSDT; its weak default emits no
table on platforms without a provider. This board fills one qualified type0
subspace only after the exact fresh PCC probe and a matching enabled CPU
inventory. CPU packages are emitted only after that PCCT provider succeeds.
They reuse `acpigen_write_CPPC_package`, with a local GCPC and local-return
`_CPC` method in each real CPU device. Its PCC offset uses APIC-sorted rank,
independently of MADT UID order. The generic absoluteCPU0 method is unsuitable
for these per-CPU shared-memory offsets and is not called.

Normal96 physically imported the original compiled-default PowerPlay table,
obtained valid166/132/62/20 capabilities for all twelve threads and passed its
host600/guest300 regression. This does not yet qualify an OS CPPC driver. The
new provider retains common hardware P-state fallback and withholds both PCCT
and `_CPC` for a nonzero user P-state limit. Unsupported topology, failed PCC
registration/read/UC setup or unavailable table space also leaves the legacy
interface. No manual voltage/clock override or additional SMU request is added.
HOST C fault/namespace/APIC-mapping and actual unmodified acpigen AML/ACPICA
checks pass; target driver requests and thermal/host/KVM behavior of the new
OS interface remain unqualified until their own physical candidate is tested.


Normal97 exposed a CPU identity error in the board SSDT: generic MADT sorts
enabled CPUs by hardware-thread ID and then APIC ID, while the CPU SSDT used
the nondeterministic MP device-list order for its sequential UIDs. Actual
MADT/SSDT joining found ten wrong CPPC targets in twelve CPUs. Global limits
worked because all targets received the same request; individual limits did
not address the intended CPU. Syntax-only ASL validation did not detect this.

The next candidate sorts only its local CPU-pointer array in the generic
MADT order before assigning SSDT UIDs. CPPC continues to use the independently
validated APIC-sorted PCC slot for that actual CPU, and hardware P-state
domains retain their actual core/thread rank. It does not change the global
device list, CPU initialization, SMU requests, PCC layout or CSM behavior.
Qualification must join the real generic MADT with the complete CPU writer
over shuffled inventories and repeat that join on the actual target before
testing each CPU's standard driver requests. The new correction is HOST-only
until its own image, physical boot and request tests are captured.

RTC wake transition draft: the actual98 S0 alarm fails with PM74[29] set,
succeeds with only that bit clear, and fails again after restoration. Keep
that bit clear at firmware S0 handoff; publish the configured S5 permission
as RWA5. Standard _PTS(5) applies it, while _WAK and _TTS(0) clear it. No
other sleep state is advertised. This draft still needs HOST and physical
qualification, including both S0 alarms and actual S5 wake.

The transition AML and its RWA5 producer are guarded by the union of board
and desktop setup, matching setup_enabled(); normal Vermeer uses the latter.
