# MSI X58 Pro-E: experimental X58 bring-up

This downstream research port brings up the MSI X58 Pro-E (MS-7522), its
LGA1366 processor, Intel X58 IOH and ICH10R southbridge. It is not a supported
production BIOS, and a usable recovery programmer is required before testing.

The current path uses **locally supplied vendor-assisted DDR3/QPI
initialization**, followed by coreboot ramstage and a SeaBIOS payload. It does
not provide native open-source memory initialization. EDK2/UEFI remains a
separate development direction, not the tested payload in this release.
Neither payload replaces the CPU, memory-controller or chipset initialization.

## Current status

The last hardware-tested private image was **B06WK**. This public source tree
also contains a subsequent, **source-only serial-independent SPD admission
change**. No new firmware build or hardware result is implied by publishing
the sources. Do not identify a new build as the unchanged historical B06WK.

The narrow measured target used a Xeon E5649, CPUID `0x206c2`, microcode
revision `0x1f`, one 4-GiB 2Rx8 DDR3 DIMM responding at SPD address `0x54`,
and a Radeon HD 5450 with its own legacy video option ROM. The vendor-reference
machine used an E5645; it must not be confused with the measured target.
The public records omit individual module, storage and network identities.

Recorded milestones include:

- Reset, cache-as-RAM, serial diagnostics and an interactive ROMMON.
- Vendor-assisted memory initialization and a High-QPI endpoint, followed by
  coreboot ramstage and SeaBIOS execution.
- PCI device enumeration, Radeon display output, USB keyboard/storage,
  SATA detection/basic reads, and iPXE network transfers.
- Linux kernel and initramfs execution through `/init`, device discovery and
  filesystem reads. A completed live desktop/session is not established.

Important open problems are not fixed by the SPD change:

- Windows PE stops with `ACPI_BIOS_ERROR (0xA5)`; the detailed bugcheck
  parameters and exact failing ACPI object remain unknown.
- Linux runs unusually slowly. Firmware source exposes a possible cache-policy
  gap for remapped high memory; its causal role is not established by a final
  live-MSR capture.
- A Memtest 5.01 photo reports failures in the low-memory tail that SeaBIOS
  reserves. A disagreement between the coreboot and BIOS memory maps is a
  concrete hypothesis, not proof of a defective DIMM or a passing memory test.
- Short power interruptions can leave USB in a state rejected by the existing
  guards. Cold-start/recovery behavior is not validated across all conditions.
- Repeated cold/warm boot, full-memory stability, complete ACPI power
  management, additional DIMM topologies and general OS compatibility are open.

## Source and test layout

Clone the downstream research branch explicitly:

```sh
git clone --branch coreboot-x58 https://github.com/Knogle/coreboot.git
cd coreboot
```

Its base is the previously exercised upstream commit
`fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2`, not newly rebased upstream `main`.
Preserving that base does not turn the public export or its source-only SPD
change into a new hardware-validated release.

Board code is in `src/mainboard/msi/x58_pro_e/`. The research tools,
configurations, optional payload patches, host tests and English historical
documentation are kept together under [`util/x58`](../../../util/x58/README.md).

The [SPD contract](../../../util/x58/Documentation/spd-compatibility.md)
ignores only DDR3 serial bytes 122 through 125 in its compatibility digest.
Complete reads, CRC, geometry, rank count, timing, topology and all other SPD
bytes remain gated. Raw identity is kept separate from compatibility, and
the new producer/consumer handoff is version 10. This is not support for
arbitrary modules, one-rank DIMMs or additional memory channels.

Run commands from the **coreboot checkout root**, not from `util/x58`.
The host needs Python 3, Bash, Git, GNU Make, a native C compiler and the
normal coreboot development dependencies. Follow the upstream
[build prerequisites](../../../README.md#build-requirements-and-building-coreboot)
for the host platform. Firmware building additionally requires the pinned
`i386-elf` cross-toolchain, ACPICA `iasl` and the initialized Intel microcode
submodule. The `crossgcc-i386` target also builds `iasl`; the development
helper checks for both before compiling firmware.

Host tests do not require target hardware and do not build firmware:

```sh
python3 -B -m unittest discover -s util/x58/tests -v
PYTHONPATH=util/x58/scripts python3 -B -m unittest discover -s util/x58/scripts -p 'test_*.py' -v
bash util/x58/scripts/test_x58_host_c.sh
python3 util/x58/scripts/audit_public_tree.py
```

The public preparation run collected 582 host tests, with 22 private-fixture
tests explicitly skipped; the remaining tests passed. The script-local suite
collected 39 tests, with five private-input tests explicitly skipped and the
rest passing. These skips are not hardware successes or evidence that the
omitted inputs were re-tested publicly. The actual public-source bootstrap
also reproduced the pinned patched SeaBIOS revision `5497f4318937` and checked
iPXE `7c39c04a537c`, without compiling a payload or firmware image.
The three standalone C harnesses also pass (ROMMON script engine, RAM loader
and IRQ probe). Their backends are simulated; the IRQ test explicitly excludes
native privileged hardware operations with `X58_IRQPROBE_HOST_TEST`.
On an unprepared clone, optional payload-source checks are also skipped until
the explicit public-source preparation step below has completed.

An optional [GitHub Actions template](../../../util/x58/ci/x58-host-tests.yml.example)
contains the same source-only checks. It is intentionally not installed as an
active workflow: the publishing credential did not grant workflow-management
permission. Local test results above are verified; no hosted CI run is claimed.

For a future, explicitly requested local firmware build, the development
workflow is:

```sh
make crossgcc-i386 CPUS=8
git submodule update --init --checkout 3rdparty/intel-microcode
bash util/x58/scripts/prepare_x58_payloads.sh
bash util/x58/scripts/prepare_x58_payloads.sh --check
X58_VENDOR_ROM=/path/to/A7522IMS.8F0 bash util/x58/scripts/build_x58_development.sh
```

The preparation step retrieves the pinned open-source payload inputs and
applies the local patches; it is not a firmware build. The build requires
your own matching MSI firmware input. Its fresh identity is
`X58PROE-DEVELOPMENT-SPD10`, not historical B06WK. By default, local results
and `build-manifest.json` are placed under the ignored
`util/x58/blobs-local/msi-x58-pro-e/development-spd10/` directory; an explicit
`X58_OUTPUT_DIR` may select another private output directory. Use absolute
paths for both `X58_VENDOR_ROM` and `X58_OUTPUT_DIR`. The workflow checks
two clean outputs for equality, but that workflow has not been run
as part of this public source-only preparation.

The development configuration is
`util/x58/configs/x58-pro-e-development.config`. Successful local builds would
retain the four ROM artifacts, effective coreboot/SeaBIOS configurations and
the generated manifest in that private output directory. Differing existing
artifacts are not silently overwritten. The `--check` preparation mode is a
read-only local preflight; it does not fetch sources or touch target hardware.

No MSI/Intel/AMI image, extracted
proprietary executable module, composed flash image, private key or private
hardware capture is supplied here. A recognisable memory-init module is not
a documented, interchangeable MRC/FSP binary. Review the
[vendor-assisted interface](../../../util/x58/Documentation/vendor-assisted-memory-init.md)
and [recovery instructions](../../../util/x58/Documentation/flash-recovery.md)
before attempting a build or flash operation.

## Recovery and first testing

1. Identify the installed flash chip, capacity, package and operating voltage;
   do not infer these solely from a programmer's generic chip-family name.
2. Preserve independently verified, complete vendor backups and a known-good
   recovery chip. Test the external programmer's read/write/verify path first.
3. The historical target used a 16-MiB W25Q128-family flash. A coreboot build
   component is not automatically the complete, correctly aligned chip image.
   Check the generated manifest, image size and layout before programming.
4. Record the source commit, effective configuration, complete-image hash,
   build identity, non-identifying hardware configuration and boot provenance.
5. Keep serial/POST diagnostics and reset-loop guards enabled. Never replace
   an unexpected gate result with a fabricated success value.
6. After a failed image, restore the known-good chip/image with the validated
   recovery procedure. Do not assume a short AC interruption is a full reset.

No flash or remote-power command is a required part of the public build
workflow. Adapt any lab-only tool to an explicitly authorized local target.

## Research and collaboration

Start with the [research index](../../../util/x58/Documentation/PUBLIC_RESEARCH_INDEX.md),
[bring-up history](../../../util/x58/Documentation/bringup-log.md),
[test matrix](../../../util/x58/Documentation/test-matrix.md),
[ROMMON scripting contract](../../../util/x58/Documentation/b06v1-register-scripting.md)
and [privacy/evidence policy](../../../util/x58/Documentation/PUBLIC_EVIDENCE.md).
Historical notes retain their original experimental chronology; a statement
that a build was not yet tested is not silently rewritten into a later result.

Contributions should state one hardware hypothesis, the source of each
register write, expected diagnostics, bounded failure behavior and recovery
method. Keep native initialization, optional vendor-assisted experiments and
payload changes distinguishable. Submit reproducible host tests and sanitized
observations, not proprietary firmware or private lab access details. Preserve
CPU/PCI IDs, register values and firmware module GUIDs as technical evidence;
omit personal paths, credentials, MAC addresses and individual device serials.

Submit downstream GitHub pull requests to **Knogle/coreboot:coreboot-x58**.
Upstream coreboot's Gerrit process is separate; the generic upstream README
does not prohibit pull requests to this research fork.
