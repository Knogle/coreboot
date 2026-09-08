> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Vendor-firmware state capture

Status: tooling implemented, host-tested with synthetic PCI sysfs fixtures,
and run successfully on the MSI X58 Pro-E on 2026-08-04.  Three 256-byte
captures were raw-register identical; the explicit 4-KiB pass returned full
extended configuration for the two IOH functions and 256 bytes for the CPU
Uncore functions.  See
[the live analysis](../research/msi/live-uncore-2026-08-04.md).

## Purpose and safety boundary

`scripts/dump_nehalem_uncore.py` captures the enumerated Bloomfield/Gulftown
CPU-Uncore PCI configuration that ordinary `lspci` output is easy to miss.  It
uses the device/function tables and named offsets from the upstream Linux
[`i7core_edac.c`](https://github.com/torvalds/linux/blob/master/drivers/edac/i7core_edac.c)
driver and the Intel 320835/321322/323370 datasheet terminology.  The named view now
covers the published common IMC, SAD/TAD, CPU-side QPI, channel control,
address/rank, and thermal layouts.  A few `MC_TEST_OBSERVED_*` names are
neutral offsets recovered from MSI code, not Intel-published field names.

The tool:

- opens only existing `/sys/bus/pci/devices/*/config` files for reading;
- performs no PCI, MMIO, MSR, I/O-port, or sysfs writes;
- does not enable devices or force scans of hidden buses;
- reads 256 bytes per recognized function by default, covering every named
  register in its current map;
- records raw bytes, SHA-256, BDF, role, expected device/function position,
  and a named-register view as JSON.

Completeness is evaluated separately for each PCI domain/bus (the Linux
driver's per-socket grouping).  Diffs identify a device by BDF plus
vendor/device ID and report four-byte access width explicitly; an ID change at
one BDF is therefore a removed and added function, not a register delta.

Even reads can expose broken firmware or hardware behavior.  Run the first
capture with a recovery path, local console, and no irreplaceable workload.
The 4 KiB extended-configuration mode is deliberately opt-in and should not be
the first test.

## Recognized CPU generations

| Profile | Linux PCI-ID families | Expected functions |
|---|---|---|
| `bloomfield_gainestown` | `2c01`, `2c10/2c11`, optional `2c14/2c15`, `2c18..2c1c`, `2c20..2c33`, noncore `2c40/2c41` | SAD, CPU QPI, device 3 common, devices 4–6 channel functions, device 0 noncore |
| `gulftown_westmere_ep` | `2d81`, `2d90..2d95`, `2d98..2d9c`, `2da0..2db3`, noncore `2c70` | SAD, both CPU QPI/mirror-port pairs, and the same three-channel IMC layout |

Only the explicit Linux IDs are accepted; the apparent numeric gaps are not
blindly scanned.  X58 IDs `8086:3405` and `8086:342e` are captured as IOH
context but do not by themselves prove that a supported CPU Uncore profile is
visible.

## First capture

Create a new immutable test directory and fill in the hardware-test metadata
from [bringup-log.md](bringup-log.md).  Then, under the vendor firmware:

```bash
sudo python3 scripts/dump_nehalem_uncore.py --require-target \
  > vendor-cold-01-uncore.json

sha256sum vendor-cold-01-uncore.json
```

`--require-target` returns status 2 when neither supported CPU profile is
visible.  That is a useful failure, not permission to probe random BDFs.  The
Linux driver notes that some Xeon 55xx firmware omits high noncore buses from
ACPI and has an optional fixup scan for buses 255/254.  Enabling that kernel
path changes enumeration state and belongs in a separate, explicitly approved
experiment; the snapshot tool never does it.

If the 256-byte capture is stable and extended configuration is actually
needed, repeat separately with:

```bash
sudo python3 scripts/dump_nehalem_uncore.py \
  --require-target --config-bytes 4096 > vendor-cold-01-uncore-4k.json
```

Do not overwrite the 256-byte baseline.

The successful target artifacts are under `research/msi/captures/`.  Their
post-boot state is a baseline, not a write-order trace.  In particular,
`MC_CHANNEL_DIMM_INIT_STATUS` describes only the most recent training command,
and write-only `MC_CONTROL.INIT_DONE` cannot be recovered from a readback.

## Compare one variable at a time

Examples of useful controlled pairs are cold versus warm reset, one DIMM slot
versus another, or one vendor memory setting at a time.  Compare all captured
dwords:

```bash
python3 scripts/compare_uncore_dumps.py \
  vendor-cold-01-uncore.json vendor-warm-01-uncore.json
```

Or reduce the report to the published and explicitly vendor-observed named
offsets:

```bash
python3 scripts/compare_uncore_dumps.py --only-known \
  vendor-cold-01-uncore.json vendor-warm-01-uncore.json
```

The complete raw capture should still be retained.  The published fields
describe DIMM geometry, training status, channel/rank presence, address
mapping, QPI, RAS, and counters, but do not expose all PHY state.  The
comparator also reports transitions in read errors or capture length so an
unreadable function cannot masquerade as an unchanged one.

Static correlation with MSI `MINITDLL`, including the high-value unpublished
`03.4` offsets to compare, is recorded in
[uncore-register-correlation.md](../research/comparisons/uncore-register-correlation.md).

## Complementary captures

Run these separately so a failure identifies the risky reader:

```bash
sudo lspci -Dnnxxxx > lspci-config.txt
sudo dmidecode > dmidecode.txt
sudo cpuid -1 -r > cpuid.txt
sudo decode-dimms > spd.txt
sudo acpidump -o acpi.dat
```

`inteltool` remains useful for X58 DMIBAR and ICH10 state, but current upstream
does **not** implement X58 CPU-Uncore timing export through `-S`; its memory
timing path targets other northbridges.  Do not use an assumed
`inteltool -S x58-training.bin` result as evidence.  Run `inteltool` categories
individually and preserve each partial output because broad register dumps can
hang some systems.

An S3 boot script may capture restore programming but is not equivalent to a
cold-boot training trace.  CHIPSEC or any kernel driver needed to expose it
should therefore be treated as an optional later capture, with its exact
version and load state recorded.

## Interpretation rules

- A changed dword is an observation, not an initialization write to copy.
- Counters/status fields may be volatile; require repeated captures.
- A stable post-boot value may have been computed from earlier hidden PHY
  state rather than programmed directly.
- Never write a captured MMIO/MSR/PCI value on live hardware without a named
  source, a bounded hypothesis, and external flash recovery.
- Ten reproducible cold boots remain the milestone criterion; one snapshot is
  only a baseline.
