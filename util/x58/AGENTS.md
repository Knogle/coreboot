> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../Documentation/mainboard/msi/x58_pro_e.md).

# AGENTS.md

## Project

**MSI X58 Pro-E coreboot port with UEFI-compatible payload**

The goal of this repository is to develop a reproducible coreboot port for the
MSI X58 Pro-E, covering the Intel Bloomfield/Gulftown CPU families, the
integrated triple-channel DDR3 memory controller, the Intel X58 IOH, and the
ICH10R southbridge.

The intended firmware architecture is:

```text
reset vector
  -> coreboot bootblock
  -> cache-as-RAM / early CPU initialization
  -> DDR3 + QPI initialization
  -> X58 IOH + ICH10R initialization
  -> coreboot ramstage
  -> EDK2/Tianocore UEFI payload
  -> UEFI operating-system boot
```

Do not describe the end product as merely a “UEFI BIOS replacement.” The hard
part is the platform initialization performed before the UEFI payload starts.
EDK2 supplies UEFI boot and runtime services; it does not initialize X58, QPI,
or the Bloomfield/Gulftown memory controller for coreboot.

---

## Known project facts

- Target mainboard: **MSI X58 Pro-E**
- Socket: **LGA1366**
- Chipset: **Intel X58 IOH + ICH10R**
- Firmware family: expected to be **AMI BIOS 8 / legacy AMIBIOS**
- The flash device is socketed, which makes recovery and rapid chip swapping
  practical.
- The board has six DDR3 DIMM slots and uses the CPU-integrated triple-channel
  memory controller.
- The desired final boot environment is UEFI-compatible, preferably through
  an EDK2 payload.
- Intel DX58SO, DX58SO2, and DX58OG firmware images are useful comparative
  material because they contain early Intel X58 UEFI implementations.
- The historical coreboot code once named `intel/nehalem` is **not** a usable
  Bloomfield/X58 implementation. It was actually for Arrandale/Ironlake and was
  renamed accordingly.
- Existing X58 references in coreboot utilities, such as DMIBAR support in
  `inteltool`, do not constitute chipset initialization support.

Treat every board-specific electrical or firmware detail not listed above as
unverified until it is measured or documented.

---

## Primary engineering objective

Produce a minimal, reviewable, upstream-quality coreboot platform port that can
eventually:

1. execute reliably from reset;
2. provide early POST-code and serial diagnostics;
3. establish cache-as-RAM;
4. initialize one supported CPU configuration;
5. initialize one conservative DDR3 configuration;
6. train QPI and bring up the X58 IOH;
7. initialize ICH10R and enumerate PCI;
8. expose correct memory and ACPI information;
9. launch an EDK2 UEFI payload;
10. boot a 64-bit operating system from GPT media.

Initial success does **not** require overclocking, every CPU stepping, every
DIMM topology, S3 resume, RAID, or all onboard peripherals.

---

## Non-goals for the first bring-up

Do not spend early project time on:

- graphical firmware setup;
- Secure Boot;
- overclocking controls;
- XMP;
- all six DIMM slots;
- mixed DIMM populations;
- maximum memory frequency;
- S3 suspend/resume;
- Intel RAID firmware compatibility;
- legacy option-ROM execution;
- perfect ACPI power management;
- every Bloomfield and Gulftown stepping;
- distributing proprietary Intel, AMI, or MSI binary modules.

The first hardware target should be deliberately narrow:

```text
one known-good CPU
one DIMM
JEDEC-safe frequency and timings
one GPU
one SATA boot device or USB device
all overclocking disabled
```

Expand the matrix only after a stable cold boot exists.

---

## Central technical risk

The main research problem is the initialization of:

- the Bloomfield/Gulftown integrated memory controller;
- DDR3 training;
- CPU-to-X58 QPI;
- X58 IOH state required before normal PCI enumeration;
- the interaction between CPU-only resets, warm resets, and memory training.

The firmware may contain Intel reference code or an MRC-like component, but do
not assume it is:

- a standalone binary;
- relocatable;
- callable through a documented ABI;
- equivalent to Intel FSP;
- portable between Intel and MSI boards;
- independent of PEI services or AMIBIOS global state.

A recognizable firmware module is evidence, not automatically a reusable blob.

---

## Firmware-source strategy

Use three complementary sources.

### 1. MSI X58 Pro-E vendor firmware

This is the authoritative source for board-specific behavior:

- DIMM-slot and SPD-address mapping;
- GPIO configuration;
- Super I/O configuration;
- clock generator programming;
- voltage-controller interaction;
- X58 and ICH10R straps;
- PCIe topology;
- board-specific reset sequencing;
- ACPI and interrupt routing.

Although AMIBIOS8 is not PI/UEFI, it is not necessarily a single unstructured
binary. Expect compressed modules, a bootblock, BIOS core, option ROMs,
microcode, ACPI data, setup data, and vendor-specific regions.

The difficult code may still be statically linked into a large POST module.

### 2. Intel DX58SO/DX58SO2/DX58OG UEFI firmware

Use Intel firmware as a structural and semantic reference:

- Firmware Volumes and FFS files;
- SEC/PEI/DXE boundaries;
- PE32 or TE images;
- GUIDs, PPIs, and HOBs;
- possible memory-init or QPI-related PEIMs;
- Intel-generic register sequences;
- strings, POST codes, and error paths.

Do not assume an Intel memory-init PEIM can be transplanted directly into
coreboot. It may depend on Intel-board-specific policy and a substantial PEI
environment.

### 3. A running board using the original firmware

Collect the actual post-initialization state from real hardware. This is often
more useful than static disassembly alone.

Capture at least:

```bash
sudo flashrom -p internal -r vendor-live.bin
sudo inteltool -a > inteltool-full.txt
sudo lspci -nnxxxx > lspci-config.txt
sudo superiotool -adeV > superio.txt
sudo acpidump -o acpi.dat
sudo dmidecode > dmidecode.txt
sudo cpuid -1 -r > cpuid.txt
```

Record exact versions, CPU model and stepping, DIMM part number, slot, BIOS
settings, and whether the capture followed a cold boot or warm reset.

Where safe and useful, add repeatable MSR, MMIO, PCI configuration, SMBus, and
POST-code captures.

---

## Proprietary-blob policy

Never commit vendor firmware or extracted proprietary modules to the public
repository.

Allowed repository contents include:

- cryptographic hashes;
- extraction scripts;
- offsets and metadata;
- module GUIDs;
- locally applied patch descriptions;
- reverse-engineered interfaces;
- clean-room reimplementations;
- scripts that operate on a user-supplied firmware image;
- documentation describing how to obtain and verify an image independently.

Use a local layout similar to:

```text
blobs-local/
  msi-x58-pro-e/
  intel-dx58so2/
```

Add the directory to `.gitignore`.

Any experimental wrapper around a vendor module must remain optional and must
not become an unexplained permanent dependency. Clearly distinguish:

- native open initialization;
- locally extracted vendor-assisted initialization;
- experimental reverse-engineering scaffolding.

Do not claim legal redistributability merely because a module can be extracted.

---

## Reverse-engineering methodology

### Static analysis

Preferred tools:

- UEFITool and UEFIExtract for Intel UEFI images;
- AMIBIOS8-compatible extraction tools for the MSI image;
- Ghidra for 16-bit and 32-bit x86 analysis;
- `cbfstool`, `ifdtool`, `inteltool`, `superiotool`, and `msrtool` where
  applicable;
- `binwalk`, `strings`, `objdump`, and custom Python parsers.

Search for code patterns involving:

- PCI configuration ports `0xcf8` and `0xcfc`;
- SMBus accesses to SPD EEPROM addresses;
- `RDMSR` and `WRMSR`;
- DMIBAR, MCHBAR-like, PCIEXBAR, and other base-register setup;
- polling loops around training/status registers;
- POST writes to port `0x80`;
- large register/value tables;
- CPU reset requests during memory initialization;
- memory-map and TOLUD/TOUUD construction;
- QPI link-width, speed, and training state;
- repeated sequences shared by MSI and Intel firmware.

When a sequence appears in both firmware families, document it as a likely
Intel-generic primitive. When it appears only in the MSI image, treat it as
potentially board-specific.

### Dynamic analysis

Prefer controlled observation over blind copying.

Useful methods include:

- serial logging from vendor firmware, when available;
- POST-code tracing;
- comparing cold boot and warm reset;
- comparing one-DIMM and multi-DIMM populations;
- dumping PCI/MSR/MMIO state after successful vendor initialization;
- temporarily instrumenting an emulator only for code-understanding purposes;
- tracing extracted modules in a synthetic PEI environment if feasible.

Do not write undocumented MMIO or MSR values to live hardware without a
recovery path and a clear hypothesis.

### Interface reconstruction

For any candidate vendor memory-init component, identify:

1. entry point;
2. execution mode;
3. stack and temporary-memory requirements;
4. fixed-address assumptions;
5. parameter block layout;
6. required PEI services or callbacks;
7. input SPD representation;
8. output memory map or HOB data;
9. reset behavior;
10. side effects on CPU, IOH, and southbridge state.

Do not add a call to a binary function until these properties are documented.

---

## Bring-up phases

### Phase 0: Recovery and hardware inventory

Before flashing experimental firmware:

- identify the exact flash-chip model, capacity, package, and voltage;
- obtain at least two compatible spare chips;
- verify a complete external read/write/verify cycle;
- store multiple verified vendor backups;
- identify a usable POST-code path;
- identify the Super I/O and UART routing;
- document board revision and all relevant jumper positions.

Acceptance criteria:

- a bad image can be recovered without soldering;
- vendor firmware can be restored and verified;
- each test image has a unique build identifier.

### Phase 1: Firmware archaeology

Create reproducible inventory reports for both MSI and Intel images.

Expected outputs:

```text
research/
  msi/
    image-hashes.md
    module-map.csv
    strings/
    post-codes.md
  intel/
    image-hashes.md
    fv-map.csv
    guid-map.md
    pei-candidates.md
  comparisons/
    shared-sequences.md
    candidate-mrc-interface.md
```

Acceptance criteria:

- module boundaries are reproducible;
- candidate early-init components are named and hashed;
- relevant code regions can be loaded into Ghidra with documented bases.

### Phase 2: Minimal coreboot target

Add the mainboard skeleton and enough chipset scaffolding to build an image.

First hardware image should do only:

1. execute from reset;
2. emit distinct POST codes;
3. initialize the UART if possible;
4. print a build ID;
5. halt safely.

No RAM dependency is permitted at this point.

Acceptance criteria:

- repeated cold boots produce the same trace;
- failure points are distinguishable;
- no unintended write to the vendor chip occurs.

### Phase 3: CPU and cache-as-RAM

Implement or adapt:

- reset-vector transition;
- microcode loading;
- minimal CPU setup;
- cache-as-RAM;
- early C environment;
- watchdog and reset handling.

Acceptance criteria:

- stable C execution before DRAM;
- serial console remains functional;
- CPU identification and stepping are logged.

### Phase 4: SMBus and SPD access

Bring up only the minimum ICH10R path needed to read SPD.

Acceptance criteria:

- the selected DIMM can be detected repeatedly;
- raw SPD can be dumped;
- bad/missing SPD is reported without hanging;
- no training has started yet.

### Phase 5: DDR3 and QPI research implementation

Start with the narrowest supported configuration.

Required logging:

- selected memory frequency;
- SPD-derived timings;
- channel/rank detection;
- each training phase;
- QPI training state;
- every reset request;
- final memory-map registers.

Acceptance criteria:

- deterministic training on one DIMM;
- at least 256 MiB can be tested reliably;
- repeated cold boots pass;
- failures return a useful code instead of silently resetting forever.

### Phase 6: X58 IOH and ICH10R

Implement enough platform code for:

- QPI-complete IOH access;
- DMIBAR and PCIe configuration;
- LPC and SPI;
- SATA in a conservative mode;
- USB sufficient for basic testing;
- PCI enumeration;
- onboard-device enable/disable policy.

Acceptance criteria:

- stable PCI tree;
- GPU and boot storage enumerate;
- no overlapping resources;
- vendor and coreboot captures can be meaningfully compared.

### Phase 7: Ramstage, tables, and payload

Add:

- memory resource reporting;
- SMBIOS;
- ACPI;
- interrupt routing;
- MP tables only where required;
- CBMEM;
- EDK2 UEFI payload integration.

Acceptance criteria:

- EDK2 starts consistently;
- UEFI shell runs;
- memory map is sane;
- a GPT-formatted device can be enumerated;
- a 64-bit operating-system loader starts.

### Phase 8: Stabilization

Only after the minimal target is reliable, add:

- additional DIMM slots and channels;
- multi-rank support;
- Gulftown/Bloomfield expansion;
- additional memory speeds;
- ACPI power management;
- S3 resume;
- optional NVMe DXE support;
- additional onboard devices.

---

## Suggested repository layout

```text
.
├── AGENTS.md
├── README.md
├── Documentation/
│   ├── board-inventory.md
│   ├── bringup-log.md
│   ├── flash-recovery.md
│   ├── memory-init.md
│   ├── qpi-init.md
│   └── test-matrix.md
├── research/
│   ├── msi/
│   ├── intel/
│   └── comparisons/
├── scripts/
│   ├── extract-msi.py
│   ├── extract-intel.py
│   ├── compare-reg-sequences.py
│   └── collect-vendor-state.sh
├── patches/
├── configs/
└── blobs-local/          # ignored; never commit
```

If this work is performed directly inside a coreboot tree, keep board code and
silicon code separated:

```text
src/mainboard/msi/x58_pro_e/
src/northbridge/intel/x58/
src/southbridge/intel/ich10/
src/cpu/intel/model_1067x/   # verify exact placement and CPUID coverage
```

Do not force this exact layout if current coreboot conventions require a better
one. Follow the contemporary tree structure and avoid duplicating existing
common Intel code.

---

## Coding rules

- Prefer small, bisectable commits.
- One hardware hypothesis per commit whenever possible.
- Every register write needs one of:
  - a public datasheet reference;
  - a clearly documented vendor-firmware observation;
  - an experimentally justified explanation.
- Use named masks and fields, not unexplained hexadecimal constants.
- Put board policy in mainboard code and reusable silicon behavior in chipset
  code.
- Avoid arbitrary delays. Poll status with bounded timeouts.
- Every loop that waits on hardware must have a timeout and diagnostic output.
- Do not silently retry resets indefinitely.
- Preserve early console output for as long as practical.
- Keep cold-boot and resume paths explicitly separate.
- Default to conservative JEDEC parameters.
- Reject unsupported DIMM configurations clearly.
- Never enable overclocking during bring-up.
- Keep binary extraction and disassembly tooling deterministic.
- Record tool versions for all generated research artifacts.

Example register style:

```c
#define X58_FOO_CTRL_ENABLE       BIT(3)
#define X58_FOO_CTRL_MODE_MASK    GENMASK(6, 4)

static void x58_enable_foo(uintptr_t base)
{
        uint32_t value = read32p(base + X58_FOO_CTRL);

        value &= ~X58_FOO_CTRL_MODE_MASK;
        value |= X58_FOO_CTRL_ENABLE;
        write32p(base + X58_FOO_CTRL, value);
}
```

Do not use a symbolic name unless its meaning is reasonably established. For
unknown fields, use neutral names such as `FIELD_6_4` and document the observed
behavior.

---

## Logging requirements

Each hardware test record must include:

```text
build commit:
ROM hash:
flash chip:
board revision:
CPU:
CPU stepping:
microcode revision:
DIMM model:
DIMM slot:
GPU:
PSU:
boot type: cold / warm / reset
POST trace:
serial log:
result:
recovery required:
notes:
```

Keep logs immutable. Add new observations rather than rewriting failed-test
history.

Use clear phase prefixes:

```text
[BOOTBLOCK]
[CAR]
[SPD]
[RAMINIT]
[QPI]
[X58]
[ICH10]
[RAMSTAGE]
[PAYLOAD]
```

---

## Validation policy

A single successful boot is not proof.

Minimum validation for a milestone:

- ten consecutive cold boots;
- ten warm resets where applicable;
- memory test appropriate to the initialized range;
- no unexplained variation in training results;
- no hidden fallback to vendor code unless explicitly documented;
- vendor firmware remains recoverable.

For RAM initialization, validate at least:

- data-bus integrity;
- address-line integrity;
- walking-bit patterns;
- cache-disabled access where useful;
- a longer external memory test after payload boot.

Never expand hardware support while the current minimal configuration is
intermittent.

---

## Decision rules for vendor code

Use the following preference order:

1. public documentation and existing open coreboot code;
2. clean-room reimplementation from observed behavior;
3. temporary local vendor-assisted shim for research;
4. direct vendor-module execution only as an explicitly experimental fallback.

Before using an extracted module, answer:

- Is it legally redistributable? Usually assume no.
- Is the entry point known?
- Is the calling convention known?
- Are its dependencies known?
- Does it expect PEI services?
- Does it use fixed physical addresses?
- Does it issue CPU-only or full resets?
- Are board-policy inputs separable?
- Can its output be translated into coreboot resource structures?
- Can the project eventually replace it?

If several answers are unknown, continue reverse engineering instead of adding
the blob to the normal boot path.

---

## Agent operating instructions

When working in this repository:

1. Read `AGENTS.md`, `README.md`, the current bring-up log, and the test matrix
   before making changes.
2. Inspect the current tree and follow existing coreboot conventions.
3. Do not assume that names containing “Nehalem” refer to desktop Nehalem.
4. Do not claim X58 support based on utility-only PCI-ID recognition.
5. Do not invent register meanings.
6. Do not modify or redistribute proprietary firmware images.
7. Do not produce a flashable image that removes the current diagnostic path
   without explicitly noting it.
8. Keep experimental and production paths separate through Kconfig options or
   clearly isolated branches.
9. For hardware-affecting changes, include:
   - the hypothesis;
   - expected POST/serial result;
   - failure mode;
   - recovery procedure;
   - exact test configuration.
10. Prefer completing one measurable bring-up milestone over broad speculative
    scaffolding.
11. Never state that hardware has been tested unless a real test log exists.
12. Mark inferred behavior as inference.
13. Preserve a known-good build after every new milestone.
14. Do not optimize boot time before correctness.
15. Avoid unrelated refactors during hardware bring-up.

---

## Immediate work queue

Unless repository evidence changes the priority, proceed in this order:

1. document flash recovery and exact flash-chip details;
2. acquire and hash the MSI vendor ROM;
3. acquire and hash a suitable Intel DX58SO2/DX58OG UEFI ROM;
4. produce module maps for both images;
5. identify Super I/O and serial routing;
6. capture vendor-initialized hardware state;
7. create the coreboot mainboard skeleton;
8. reach reset-vector POST output;
9. reach serial output before DRAM;
10. establish cache-as-RAM;
11. read SPD through ICH10R;
12. map candidate memory/QPI initialization code;
13. implement the smallest possible one-DIMM training path;
14. initialize X58 sufficiently for PCI enumeration;
15. launch an EDK2 payload.

---

## Definition of initial project success

The first major release is successful when the MSI X58 Pro-E can cold boot from
a fully documented coreboot image, using a defined CPU and one-DIMM
configuration, initialize DDR3 and QPI without an undisclosed vendor boot path,
enumerate essential PCI devices, launch EDK2, and enter a UEFI shell
reproducibly.

A later release may broaden hardware support and reduce or eliminate any
temporary locally extracted vendor-assisted components.
