/* SPDX-License-Identifier: GPL-2.0-only */
#include <stddef.h>
#include <stdint.h>
#include "native84_parameters.h"

#if !ENV_RAMSTAGE || !ENV_X86 || !CONFIG(ARCH_RAMSTAGE_X86_32) || \
	!CONFIG(SOC_AMD_CEZANNE_NATIVE) || !CONFIG(RELOCATABLE_MODULES)
#error "Only the separately selected retained IA32 RAMstage parameter storage"
#endif
#if ENV_SEPARATE_DATA_AND_BSS
#error "This private linker fragment requires normal combined program data/BSS"
#endif

_Static_assert(sizeof(uintptr_t) == 4 && sizeof(size_t) == 4,
	"Genuine IA32 parameter storage");
_Static_assert(sizeof(struct native84_handoff) == 56 &&
	_Alignof(struct native84_handoff) == 4, "Descriptor56 with alignment4");

/* No pointers/relocations or nonzero placeholder permission are serialized.
 * Explicit custom initialized section; actual ELF PROGBITS/file bytes and
 * rmodule symbols/header/relocations still require the selected build check.
 */
struct native84_handoff native84_handoff_parameters
	__attribute__((used, aligned(4), section(".module_parameters"))) = { 0 };
