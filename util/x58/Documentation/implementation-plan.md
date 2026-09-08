> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Concrete coreboot implementation plan

This plan deliberately separates silicon initialization from payload choice.
No phase below should be called hardware-tested until an immutable real-board
log exists.

## Target architecture

```text
reset vector
  -> CPU: protected mode and MSI-derived non-evict cache-as-RAM
  -> bootblock: POST code, minimal ICH10R LPC, Fintek UART, watchdog stop
  -> ICH10R minimal LPC/SMBus setup, read one SPD
  -> Nehalem Uncore and X58 CSI/QPI physical/link initialization
  -> one-channel/one-rank JEDEC-safe DDR3 training
  -> leave CAR, construct memory map and CBMEM
  -> X58 DMI/PCIe/resource setup
  -> reusable ICH10R device initialization
  -> ramstage tables and discrete GPU/storage enumeration
  -> SeaBIOS first, then EDK2 Universal Payload
```

The static MSI call order makes QPI-before-main-MINIT the initial hypothesis.
Each transition must have a bounded timeout, phase POST code, serial message
when available, and an explicit error halt/reset policy.

The exact fail-closed B0 design, source tree, Kconfig policy, POST-code map,
and patch ordering are in
[first-build-architecture.md](first-build-architecture.md).

## Proposed source separation

Follow contemporary upstream conventions when they differ, but keep these
ownership boundaries:

```text
src/mainboard/msi/x58_pro_e/       board GPIO/SPD/clock/slot/policy/tables
src/northbridge/intel/x58/         IOH QPI/DMI/PCIe/resources
src/cpu/intel/model_206cx/         first E5645/Westmere-EP CPU and IMC/Uncore target
src/cpu/intel/model_106ax/         Bloomfield after the E5645 path is stable
src/southbridge/intel/i82801jx/    reuse/audit existing ICH10 support
src/superio/fintek/f71882fg/       only code specific to the verified SIO
```

If current coreboot convention places the integrated memory controller under a
different CPU/SoC-like hierarchy, follow that convention rather than forcing
the illustrative paths.  Board-specific SPD addresses, voltage/clock policy,
and topology must not leak into reusable X58/CPU code.

## Phase A: make experimentation recoverable

Deliverables:

1. preserve the user-verified full 4 MiB image and record how its bytewise
   EEPROM comparison was performed;
2. record board revision and socketed flash marking;
3. identify capacity, package, and voltage from its datasheet;
4. acquire two compatible spare chips;
5. complete external read, erase, write, read-back, and hash verification;
6. preserve at least two independently read vendor images;
7. validate POST and 3.3 V/RS-232/TTL nature of JCOM1 before connecting tools.

Gate: no coreboot image is written until the original firmware can be restored
without soldering.  See [flash-recovery.md](flash-recovery.md).

## Phase B: capture board policy from the vendor boot

For one fixed CPU, one DIMM in the verified first slot, one GPU, and AHCI:

```bash
sudo flashrom -p internal -r vendor-live.bin
sudo inteltool -a > inteltool-full.txt
sudo lspci -nnxxxx > lspci-config.txt
sudo superiotool -adeV > superio.txt
sudo acpidump -o acpi.dat
sudo dmidecode > dmidecode.txt
sudo cpuid -1 -r > cpuid.txt
```

Add targeted scripts for all CPU Uncore PCI functions, relevant MSRs, X58 QPI
and DMI state, ICH10 GPIO/LPC/SMBus, and SPD bytes.  Repeat after a true cold
boot and a warm reset.  Record exact BIOS settings and component part numbers.

Gate: live Super I/O ID, UART route/level, SPD address/slot, reset behavior,
and diagnostic path are known.  Configuration port `0x4e` is already
verified-static in MSI `RUN_CSEG`.

## Phase C: reset-vector diagnostic ROM

Create the smallest mainboard/CPU skeleton that:

- contains a unique build ID/hash;
- emits distinguishable port-`0x80` codes from the reset path;
- uses coreboot's Intel non-evict CAR at the MSI-observed base `0xfff80000`
  and size 64 KiB;
- reuses ICH10 BAR/SPI setup but narrows early LPC decode to `CNF2` at
  `0x4e/0x4f` and COMA at `0x3f8` (no `0x2e`, KBC, FDD, LPT, COM2, or game
  decode in this milestone);
- verifies the Fintek identity at the statically established port `0x4e` and
  enables COM1 logical device 1 at `0x3f8`;
- emits a pre-RAM serial banner if the electrical route works;
- stops the TCO watchdog and halts without RAM use, QPI/PCIEXBAR programming,
  romstage entry, or writes to flash.

Gate: ten identical cold-boot traces using a spare chip.

## Phase D: expand CPU handling and prove the romstage loader

Build C already needs the generic CAR path to execute C.  In this phase, start
with one exact CPUID and microcode revision, add reset classification and CPU
driver policy, measure CAR stack use, and prove that romstage can be loaded
from CBFS and halt safely while still in CAR.  Do not reuse coreboot
`model_1067x`; that is Core 2/Penryn code, not Bloomfield.  The historical
`intel/nehalem` northbridge was Arrandale/Ironlake and is also not reusable
silicon code.

Gate: stable pre-DRAM C, CPUID/stepping/microcode logged for ten cold boots and
ten warm resets.

## Phase E: minimal ICH10R SMBus and SPD

Reuse and audit only the necessary `i82801jx` early SMBus/LPC code.  The
progression is deliberately split:

1. B04 programs and reads back the ICH10R/SMBus PCI configuration only; its
   terminal `dd` has been user-reported once.
2. B05 performs one byte-data read from logical SPD address `0x50`, offset
   `0x02`, and accepts only DDR3 type `0x0b`.  It has one START, an iteration
   bound, explicit error codes, and no guessed timeout recovery.  Its first
   reported hardware run ended at exact `DEV_ERR` code `fa`; DIMM placement
   and pre-DRAM pin state were not recorded.
3. B06A is the isolated diagnostic response to `fa`: record and release
   SMBCLK through documented `PIN_CTL`, require CLK/DATA high, and issue one
   bounded type-byte command at each standard SPD address.  It is not a full
   SPD dump and an exact `DEV_ERR` means only no usable response for that
   command.  Two fresh ccache-disabled builds produced the same 4-MiB image,
   SHA-256
   `83100b39b15997576f0e7a599a0e8ab5c1e78fdf1cff6f204926620e206e930e`;
   the first target run reached terminal `0x45`, identifying exactly one DDR3
   type response at logical address `0x54`.
4. B06B reads fixed address `0x54`, offsets `0x00..0x7f`, once each and
   validates the revision-selected DDR3 base-section CRC.  It deliberately
   leaves upper SPD bytes, slot correlation, QPI, and the IMC untouched.
5. B06C accepts the observed 176-byte-used declaration, reads bytes
   `0x80..0xaf` twice, requires equality, invokes coreboot's guarded DDR3
   decoder, and derives but does not program a DDR3-800 JEDEC-cycle candidate.
6. B06D correlates one physical slot at a time without changing firmware
   policy.
7. Only after that should the candidate be translated into documented X58
   register fields and conservative JEDEC/controller minimums
   and reject every untested topology.  Do not start training in this phase.

The B05 success gate remains repeatable terminal `dc` from a true AC-off start
with the complete trace and a DIMM population known to answer at `0x50`.  At
`f9`, remove AC power because the controller is intentionally not recovered.

Phase gate: repeatable full SPD dump and physical slot/address mapping;
missing or malformed SPD fails with a diagnostic rather than a hang.

## Phase F: QPI/CSI

Implement only the single-socket CPU-to-X58 link:

1. program documented CPU Uncore/X58 QPI policy for a conservative link;
2. establish required routing/base registers in the documented order;
3. request physical initialization;
4. poll link/physical-init status with bounded timeouts;
5. log speed, width, training/failure fields, and reset requests;
6. distinguish cold, warm, and CPU-only reset paths and cap retry count.

Use neutral names for fields known only from binary observation.  Validate
every register and device/function against the exact CPU stepping and X58
datasheets.

Gate: deterministic IOH/QPI accessibility without DDR training, if the
hardware permits that diagnostic point, or an unambiguous next-stage trace.

## Phase G: one-DIMM native DDR3 initialization

Implement the narrowest sequence derived from public Nehalem registers and
correlated vendor traces:

1. choose one low JEDEC frequency and voltage;
2. program channel/rank geometry and address decode;
3. program rank, bank, refresh, CKE, ZQ, and MRS values;
4. issue documented DDR3 initialization/MRS commands;
5. drive the IMC physical-init FSM per rank;
6. log/pass-check read DQ/DQS, receive-enable, write-leveling, and write
   DQ/DQS phases;
7. enforce per-step and global timeouts;
8. construct TOLUD/TOUUD and reserved ranges only after training succeeds;
9. perform data-bus, address-line, walking-bit, and uncached tests before
   trusting larger memory.

Do not add XMP, multi-DIMM, all three channels, high frequency, or mixed ranks
until the one-DIMM cold-boot result is stable.

Gate: ten cold boots, ten warm resets, deterministic training logs, and at
least 256 MiB tested.

## Phase H: X58, ICH10R, and ramstage

Implement X58 system-address routing, DMIBAR/DMI, PCIEXBAR, root ports, and
resource reporting.  Reuse ICH10R SATA AHCI, USB, LPC, and PCI code after
auditing assumptions inherited from x4x boards.  Initially disable RAID,
JMicron storage, PXE, unused USB, and nonessential devices.

Add SMBIOS, ACPI, interrupt routing, CBMEM, and a stable PCI tree.  Compare
every capture with the vendor baseline and explain intentional differences.

Gate: discrete GPU and one SATA/USB boot device enumerate without resource
overlap.

## Payload sequence

### SeaBIOS first

SeaBIOS is the faster diagnostic payload because its coreboot integration is
mature and it adds less UEFI-specific surface.  X58 has no integrated GPU, so
visible legacy VGA normally needs the installed GPU's option ROM or a
coreboot-provided framebuffer.  The GPU ROM is conditional and separate from
X58 platform initialization.  Serial SeaBIOS output can be used before VGA is
ready.

Success: SeaBIOS starts, sees the coreboot memory map/PCI tree, and boots a
simple disk or diagnostic payload.

### EDK2 second

Use coreboot's current Universal Payload integration unless upstream convention
changes.  Keep the EDK2 CPU timer path that depends on CPUID leaf `0x15`
disabled for Bloomfield unless measured support says otherwise.  Enable serial
debug and CBMEM logging.

EDK2 must consume already-correct memory and hardware state.  It should not
contain hidden Intel desktop-X58 PEIMs or serve as the X58 raminit implementation.

Success: UEFI shell is stable, memory map is sane, GPT media enumerates, and a
64-bit OS loader starts reproducibly.

## Optional vendor-assisted laboratory path

Only after the private interfaces are documented, an isolated Kconfig option
may load locally extracted MSI modules for comparison.  Requirements:

- user-supplied image and verified module hash;
- no redistribution and no default enablement;
- explicit CAR/stack/address/callback contract;
- capped reset count and visible POST/error translation;
- output converted and validated before ramstage;
- build/log states unmistakably that vendor assistance was used;
- native path remains independently buildable and is the release goal.

The Intel monolithic PEIM is a worse rehosting candidate than the MSI DLLs
because it additionally requires a PEI ecosystem and Intel-board variable
policy.

## Definition of the next implementable milestone

B06B reached terminal `0x64` from the target, proving all 128 fixed-`0x54`
base reads, DDR3 type, and the selected base CRC gate.  The immediate hardware
gate is now the reproducible B06C image from a true AC-off state with CPU and
DIMM population unchanged.  Its continuation after `62` is
`68,69,6a..6c,6d,6e..70,71,72,73,74,75`; `75` is the intentional halt after
the used-176 double-read, guarded decode, and non-programmed JEDEC-cycle
candidate.  Timeout `50` still requires complete AC removal.

B06C is not RAM initialization and its cycle tuple is not an X58 register
encoding.  B06D retains one-at-a-time physical-slot correlation.  QPI/IMC
writes and DDR training remain outside these milestones.
