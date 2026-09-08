> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Serial-independent SPD admission — pending next image

Status, 2026-09-08: **source/configuration change only, no firmware image built
or flashed**. The installed and archived B06WK image still checks the original
module's complete SPD fingerprint. Do not reuse the WK release identity for
an image containing this change; assign the next build its own identity and
manifest when the operator requests a build.

## Evidence and hypothesis

Two explicit reads of the replacement BLS4G3D1609DS1S00. module match the
historical HW07 SPD in 255 of 256 bytes. Only byte 125 differs, the last byte of
the DDR3 module serial (both private identities are redacted). Both describe the same
4-GiB2Rx8 profile and5-5-5-12 DDR3-800 timing candidate. The user reports the
replacement as known-good for a comparative Memtest run. This does not
constitute successful training or a memory-test pass under the modified port.
See the [immutable live evidence](../research/msi/b06wk-new-dimm-menu-hw-2026-09-08.md).

The narrow hypothesis is that changing only the module serial must not reject
an otherwise identical supported SPD profile. It does not extend one-rank
support or alter CPU, IMC, QPI, memory-map, cache, IRQ, USB or ACPI programming.

## Implemented contract

`CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT` is default-off and requires the
existing B06VL-derived broad handoff path. It is explicitly enabled in the
working `configs/x58-pro-e-b06wk.config` for the pending successor. Archived
release configurations and images are not modified.

- The input SPD and EEPROM remain untouched.
- Hash all256bytes with FNV-1a, substituting zero only at offsets122–125.
- Require profile FNV `5194e521` in every automatic admission and post-MINIT
  check, and again in the post-memory/ramstage handoff validator.
- Keep full raw FNV separately: original `fb66b530`, replacement `3f5e3f88`.
  Raw diagnostics and any manual `vprep SPD_FNV` confirmation keep their
  original meaning. No manual vendor-call lock is removed.
- All252 other bytes remain covered, including manufacturer/date, part
  number, CRC bytes, timings, geometry and upper/XMP data. Existing complete
  read, CRC, decoded-policy and sole-SPD54 topology gates are unchanged.
- Clear both cached hashes when starting another SPD read or invalidating
  vendor state after a generic write; failed reads cannot reuse an old match.
- With the option enabled, the CBMEM contract becomes version10/164bytes
  with raw and profile fields covered by its integrity digest. Old versions
  are rejected by the new consumer. With the option disabled, versions1–9,
  their layouts and raw-hash admission remain unchanged.

This FNV fingerprint is a compatibility check, not a cryptographic guarantee
or a substitute for memory testing. The intermediate1Rx8 module remains
outside the supported profile. Only serial variation is intentionally
admitted; other manufacturing metadata is still strict.

## Verification and next hardware test

Validation completed: `python3 -B -m unittest discover -s tests` passes all
567 tests, including 9 focused SPD compatibility tests. The focused C tests
accept 1,024 individual serial-byte value variants and reject 2,016 single-bit
changes outside the serial bytes. `git diff --check` passes. Host tests
compile only temporary native C test programs, not firmware or payload
images. They exercise the production normalization/admission and
handoff functions, input preservation, malformed sizes, serial changes,
nonserial mutations, rejected one-rank data and old/new contracts.

Existing release artifacts were hashed before and after this work and are
unchanged: WK16-MiB image `bfabde239a45489c00afa2f739d4dafa6e3b3891c758ea89ba9c0c8f554b3ca0`,
base4-MiB ROM `204fd3dfa490fb163a2e9a5c8f2e3a0087cb9c9d930baa85a25a924faa1e07a7`,
archived effective configuration `2d5edb1e3e2b268b575ca5df821084eff281938a0df56f30b6ff3ca62d06eb30`.
No hardware session or firmware integration build was performed.

When a new image is explicitly requested:

1. Preserve WK and vendor recovery chips/images; assign a fresh build ID.
2. Carry the new option into the successor's effective configuration.
3. Build and verify the firmware/CBFS/CAR-size integration, not performed now.
4. On the E5649/CPUID206c2/ucode1f board, use one replacement4-GiB2Rx8 DIMM
   at SPD54, existing HD5450 and boot media; record actual PCB/PSU/slot details.
5. Expect unchanged POST sequencing plus `FULL256_FNV1A=3f5e3f88`,
   `PROFILE_FNV1A=5194e521 SERIAL_122_125=IGNORED COMPAT_GATE=PASS`, and
   version10/size164 at handoff if all other initialization checks pass.
6. Boot Memtest and record its actual memory-map source, version, failing
   addresses and complete passes. The earlier low-RAM reservation conflict
   and high-RAM cache gap are separate open issues, not fixed here.

A profile mismatch still falls back to CAR ROMMON. Training failures retain
the existing bounded diagnostics and reset-loop guards. Recover through the
established cold-start/socketed-chip procedure; do not work around failures
by editing SPD EEPROM identity or inventing successful handoff values.
