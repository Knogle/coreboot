> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# SeaBIOS deferred USB trace: QEMU validation

Date: 2026-09-06

Scope: instrumentation/decoder validation only. The machine model was QEMU
i440fx with an explicit EHCI controller and a USB keyboard attached to its root
bus. It does not emulate the MSI X58 Pro-E, X58 IOH, ICH10 electrical routing,
or the board's GPIO/overcurrent wiring.

The tested source tree was SeaBIOS base
`b52ca86e094d19b58e2304417787e96b940e39c6` plus the subsequently frozen patch
whose SHA-256 is
`f222796ea8029444106ca009045b1528b9a325fa5b84706ca252739d353c232f`.
The release builder reproduces that tree as clean local commit
`5497f43189374647b3b0f282aa497e71c891f3c5`.

The serial capture contained `USB keyboard initialized` and this complete
journal:

```text
[USBTRACE] BEGIN count=24 dropped=0
[USBTRACE] 0 EVT=01 TYPE=0 BDF=ff:1f.7 PORT=255 RET=0 A=00000064 B=00000000
[USBTRACE] 1 EVT=10 TYPE=3 BDF=00:04.0 PORT=255 RET=32 A=00000006 B=00006880
[USBTRACE] 2 EVT=11 TYPE=3 BDF=00:04.0 PORT=255 RET=0 A=00080000 B=00001004
[USBTRACE] 3 EVT=12 TYPE=3 BDF=00:04.0 PORT=255 RET=0 A=00080000 B=00001004
[USBTRACE] 4 EVT=13 TYPE=3 BDF=00:04.0 PORT=255 RET=0 A=00080031 B=00008004
[USBTRACE] 5 EVT=14 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00001003 B=00000001
[USBTRACE] 6 EVT=14 TYPE=3 BDF=00:04.0 PORT=1 RET=0 A=00001000 B=00000001
[USBTRACE] 7 EVT=14 TYPE=3 BDF=00:04.0 PORT=2 RET=0 A=00001000 B=00000001
[USBTRACE] 8 EVT=14 TYPE=3 BDF=00:04.0 PORT=3 RET=0 A=00001000 B=00000001
[USBTRACE] 9 EVT=14 TYPE=3 BDF=00:04.0 PORT=4 RET=0 A=00001000 B=00000001
[USBTRACE] 10 EVT=14 TYPE=3 BDF=00:04.0 PORT=5 RET=0 A=00001000 B=00000001
[USBTRACE] 11 EVT=15 TYPE=3 BDF=00:04.0 PORT=0 RET=1 A=00001003 B=00001103
[USBTRACE] 12 EVT=30 TYPE=3 BDF=00:04.0 PORT=0 RET=1 A=00000000 B=00000000
[USBTRACE] 13 EVT=16 TYPE=3 BDF=00:04.0 PORT=0 RET=2 A=00001005 B=00000000
[USBTRACE] 14 EVT=31 TYPE=3 BDF=00:04.0 PORT=0 RET=2 A=00000000 B=00000000
[USBTRACE] 15 EVT=32 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00000001 B=00000000
[USBTRACE] 16 EVT=33 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=02000112 B=40000000
[USBTRACE] 17 EVT=34 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00220209 B=a0080101
[USBTRACE] 18 EVT=35 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00000022 B=00000022
[USBTRACE] 19 EVT=36 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00000001 B=00000000
[USBTRACE] 20 EVT=38 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00000000 B=00000000
[USBTRACE] 21 EVT=39 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00000008 B=00000007
[USBTRACE] 22 EVT=37 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00030101 B=00000000
[USBTRACE] 23 EVT=02 TYPE=0 BDF=ff:1f.7 PORT=255 RET=0 A=00000064 B=00000000
[USBTRACE] END
```


`scripts/decode_seabios_usbtrace.py --strict` accepted it as one valid block:
24 parsed/reported entries, contiguous indices, complete terminator and zero
drops. Five decoder unit tests cover valid, malformed, incomplete, dropped and
multiple-block inputs. The sentinel `ff:1f.7`/port 255 decodes as a global
event rather than a physical PCI function.
