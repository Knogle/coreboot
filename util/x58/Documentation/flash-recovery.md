> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Flash recovery prerequisites

Status: **partially established by a user-reported vendor boot from the spare;
the complete programmer/read-back/bad-image recovery record is still open**.

The user reports that the top-aligned 16 MiB factory image successfully booted
from the Winbond W25Q128.V..M spare.  Together with the earlier complete-EEPROM
comparison, this establishes the required top placement and a working vendor
boot path on that spare.  It does not yet record programmer voltage/software,
read-back SHA-256, or recovery from a deliberately bad image; keep the original
known-good chip unchanged during custom-firmware tests.

## Recovery acceptance checklist

- [ ] Record exact board model and PCB revision.
- [ ] Photograph/read the socketed chip marking and orientation.
- [ ] Verify capacity, voltage, erase geometry, and programmer support from the
      chip datasheet.
- [ ] Obtain at least two compatible spare chips.
- [ ] Use voltage-safe external hardware and a known-good adapter.
- [ ] Record how the full-chip comparison was obtained and preserve the raw
      reads separately from the supplied MSI-branded update archive.
- [ ] Read the installed chip at least twice after power is fully removed.
- [ ] Confirm the independent dumps and MSI image all have whole-file SHA-256
      `ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8`.
- [ ] Store immutable originals in two locations.
- [ ] Erase/write/read-back/compare a spare chip with the vendor dump.
- [ ] Boot the verified spare and restore the vendor boot path.
- [ ] Confirm a deliberately bad spare can be recovered without soldering.
- [ ] Identify a POST-code path and verify UART electrical levels.
- [ ] Give every test ROM a unique visible build ID and record its hash.

## Recovery record template

```text
date/operator:
board model/revision:
programmer and software version:
adapter and voltage:
chip manufacturer/model/package:
installed-chip dump 1 SHA-256:
installed-chip dump 2 SHA-256:
spare-chip model:
erase result:
write result:
read-back SHA-256:
vendor boot result:
bad-image recovery result:
POST path:
UART path and voltage:
photos/datasheets/log locations:
```

Do not assume an internal `flashrom` write is safe merely because an internal
read works.  Initial experimental images should be programmed externally to a
spare chip while the known-good original stays untouched.
