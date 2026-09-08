# Public evidence, privacy and omitted artifacts

This public hand-off retains all 126 historical Markdown research documents
from the private project workspace, with English operator-quote translations,
privacy redactions and repaired links. It is a **derived source/documentation
export**, not an unchanged publication of the private hardware evidence and
not a new firmware build, flash operation or hardware test.

## Interpretation of redacted values

Private hosts, login details, local user paths, network addresses and unique
hardware identities are omitted or replaced by non-operational examples.
Documentation-range addresses and locally administered example MAC addresses
are examples, not descriptions of the tested network. Synthetic DIMM serials
in prose or SPD hex rows are not measured module identities. They retain the
location/length of the serial field solely to explain the compatibility work.
Hashes quoted as historical observations refer to the original private data;
do not recompute them from a redacted SPD excerpt and call the difference a
hardware fault. In particular, a raw SPD fingerprint is distinct from the
serial-neutral profile fingerprint.

CPU models/CPUIDs, PCI vendor/device IDs, firmware-module GUIDs, register
addresses/values and artifact checksums are technical references, not private
device serials. They are retained where needed for reproducible engineering.
Colon-separated workspace-marker tuples must not be mistaken for NIC MAC
addresses. Historical claims that a source capture is immutable apply to the
private original; this public derivative is intentionally not byte-identical.

## Omitted classes

The original documentation scope contained 664 files / 58,280,943 bytes.
Its private capture directory alone contained 453 files / 56,364,742 bytes:
286 raw serial files, 133 JSON metadata files, 17 JSONL files, 11 text files,
four local capture scripts and two logs. These are **not published**. Many
serial captures contain non-UTF-8/control bytes, identity-bearing SPD data,
network/storage identifiers, terminal replays or private operational context.
A text extension or absence of a ROM suffix is not proof of safe publication.

Also omitted are local firmware inputs/extracted modules, composed ROMs,
flash-ready images, private build directories and their generated reports,
photographs, private authentication material, machine-specific source paths
and other historical artifacts not included in this public source export.
No right to redistribute a vendor executable is inferred from extraction.

Two small vendor byte runs formerly quoted in the notes (the 16-byte reset
vector record and an eight-byte adjustment table) are represented by their
offset, length, SHA-256 and independently described meaning instead. Raw
opcode bytes were removed from the quoted caller-state instruction lines;
the addresses and recovered policy semantics remain. These reductions do
not remove the corresponding hardware hypothesis or source provenance.

References below record the original *relative artifact names* mentioned by
the retained Markdown documents. The links in those documents now lead here
instead of pretending that a missing private file is publicly downloadable.
An entry means **omitted**, not verified, regenerated or disproved. Source
links that could be mapped to the public coreboot tree were repaired instead.
The index counts Markdown links; plain-text historical path mentions may also
refer to private originals and are governed by the same policy.

## Public reproduction and new observations

Use the [current mainboard guide](../../../Documentation/mainboard/msi/x58_pro_e.md)
for public build and host-test commands. Old notes may show paths from the
former two-repository lab layout; their presence does not imply that an old
private release can be reconstructed unchanged with the current source.
The archived, hardware-tested B06WK and the subsequent source-only SPD-change
state must remain distinguishable.

New public evidence should include a source commit, effective configuration,
image identity/hash, non-identifying hardware configuration, boot provenance,
expected/observed stage markers and a clear outcome/limitation statement.
Keep originals privately; publish reviewed text excerpts or normalized
register tables with an explicit redaction manifest. Do not attach a vendor
ROM, credential, whole SPD identity dump, raw DMI/UUID/serial list or unreviewed
network-bearing console log to a public issue.

## Omitted artifact reference index

| Historical relative reference | Omitted class | References |
| --- | --- | ---: |
| `builds/experimental/msi-x58-pro-e-b05-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06a-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06b-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06c-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06h-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06i-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06j-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06n-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v0-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v1-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v2-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v3-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v4-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v5-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06v6-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v7-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v8-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06v9-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06va-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vb-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06vc-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vd-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06ve-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06vf-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vg-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vh-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06vi-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vj-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vk-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06vl-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06vm-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vn-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06vo-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06vp-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06vq-ichbase1-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vq-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vq-platro1-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vq-usbtrace1-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vr-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vs-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vt-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06vy-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06vz-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06wa-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06wb-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06wc-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06wd-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06we-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06wf-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06wg-manifest.md` | Private/generated release artifact | 1 |
| `builds/experimental/msi-x58-pro-e-b06wh-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06wi-manifest.md` | Private/generated release artifact | 2 |
| `builds/experimental/msi-x58-pro-e-b06wj-manifest.md` | Private/generated release artifact | 3 |
| `builds/experimental/msi-x58-pro-e-b06wk-manifest.md` | Private/generated release artifact | 4 |
| `coreboot/payloads/external/SeaBIOS/seabios/src/hw/ahci.c` | Historical artifact not included in this source export | 1 |
| `coreboot/payloads/external/SeaBIOS/seabios/src/malloc.c` | Historical artifact not included in this source export | 1 |
| `research/msi/captures/2026-08-04-e5645-running-uncore-02.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-04-e5645-running-uncore-03.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-04-e5645-running-uncore-4k.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-04-e5645-running-uncore.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-04-runtime-invasive-probes.json` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/2026-08-04-spd-smbus-decoded.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-04-x58-hidden-qpi-probe.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-31-e5645-ratio6-qpi-high-01.txt` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-31-e5645-ratio6-qpi-high-02.txt` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-31-e5645-ratio6-qpi-high-03.txt` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-31-e5645-ratio6-qpi-high-ioh.txt` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-31-e5645-ratio6-qpi-slow-01.txt` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-31-e5645-ratio6-qpi-slow-02.txt` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-08-31-e5645-ratio6-qpi-slow-03.txt` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-ahci-hba-reset-trace.metadata.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-ahci-pi3f-trace.metadata.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-ahci-port5-spinup-trace.metadata.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-ehci1-reset-port-census.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-ehci2-reset-port-census.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-hpet-counter-runstop-trace.metadata.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-hpet-decode-census-trace.metadata.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-hpet-ioapic20-delivery.metadata.json` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/2026-09-06-b06vp-car-pic-elcr-tco.metadata.json` | Private hardware capture or capture metadata | 3 |
| `research/msi/captures/2026-09-06-b06vp-car-pic-pit.metadata.json` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/2026-09-06-b06vp-car-uhci-d26-reset-port-census.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-car-uhci-d29-reset-port-census.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-fintek-kbc-readonly.metadata.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-fintek-kbc-readonly.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-gpio56-high-ehci2-short-discard.raw` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/2026-09-06-b06vp-gpio56-high-ehci2-short-probe.metadata.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-gpio56-high-ehci2-short-rollback.raw` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/2026-09-06-b06vp-gpio56-high-ehci2-short-run.raw` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/2026-09-06-b06vp-hw-03-full-to-netboot-menu.raw` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/2026-09-06-b06vp-hw-cold-02-gpio-usb-power-census.raw` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/2026-09-06-b06vp-pm-smi-gpio-census-run.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-06-b06vp-pm-smi-gpio-census.metadata.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-07-b06wd-usb-car-gpio57-uhci4-port1-reset-load-v2.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-07-b06wd-usb-car-gpio57-uhci4-port1-reset-run-v2.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-07-b06wd-usb-car-gpio57-uhci6-port2-reset-short-run.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-07-b06wd-usb-car-recovery-before-uhci4-reset-v3.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-07-b06wf-authorized.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-07-b06wf-repeat.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-07-b06wg-local-usb-hirens.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/2026-09-07-b06wh-hw-02-usb-winpe-a5.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wi-hw01-20260907T2136.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wi-hw01-20260907T2136.raw.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wi_select_jetflash.py` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-acpi-lab-boot-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-acpi-republish-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-census-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-chain-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-fadt-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-fadt-first-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-final-tables-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-rootdump-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-ssdt-writecheck-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-ssdt-writeflag-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-grub-tables-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-hw01-20260908T0030-boot-snapshot.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-hw01-20260908T0030.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-hw01-20260908T0030.raw.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-jetflash-acpi-fixed-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-jetflash-prekey-20260908-01.raw.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj-jetflash-space-20260908-01.raw.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wj_select_jetflash_20260908.py` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wk-intenso-boot-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wk-intenso-boot-20260908-01.raw.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wk-intenso-initial-20260908-01.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wk-intenso-memtest-20260908-08.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wk-intenso-memtest-20260908-08.raw.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wk-intenso-memtest-20260908-09.raw` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wk-intenso-memtest-20260908-09.raw.json` | Private hardware capture or capture metadata | 1 |
| `research/msi/captures/b06wk-linux-user-serial-20260908-07.raw` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/b06wk-linux-user-serial-20260908-07.raw.json` | Private hardware capture or capture metadata | 2 |
| `research/msi/captures/b06wk_select_intenso_20260908.py` | Private hardware capture or capture metadata | 1 |
