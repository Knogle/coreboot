# Matisse and Vermeer desktop backends

These are explicit build-time backends, sharing the DRAM-first board flow.
The adopted source111 desktop capacity remains16 CPU slots.

See [the board documentation](../../../../../Documentation/mainboard/asrockrack/b565d4_v1l.md)
for current qualified CPUs, firmware inputs, native CPU/NBIO/DXIO/USB behavior,
payloads, observed stability and remaining limits.
See [the build instructions](../../../../../util/b565d4/README.md) for pinned
sources, the private binary input package and normal make commands.

The PSP/ABL owns DRAM training. This backend does not invoke x86 FSP or
establish cache-as-RAM. It retains the selected AMD CPU initialization and
uses the native resource allocator and normal coreboot device initialization.
The accepted policy hides the CCP PCI function. Additional CPU models and
physical multi-CCD32-thread startup are not qualified by the5600X results.
