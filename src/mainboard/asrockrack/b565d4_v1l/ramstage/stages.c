/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include "native84_parameters.h"
#include <cbmem.h>
#include <console/console.h>
#include <console/uart.h>
#include <imd.h>

#if ENV_HAS_CBMEM
uintptr_t cbmem_top_chipset(void)
{
	return native84_descriptor()->cbmem_top;
}

uintptr_t cbmem_bottom_chipset(void)
{
	const struct native84_handoff *d = native84_descriptor();

#if ENV_RAMSTAGE
	/* c_start must still receive top, separately from parameters56. */
	if (_cbmem_top_ptr != d->cbmem_top)
		native84_terminal();
#endif
	return d->cbmem_bottom;
}

struct cbmem_imd_handle_storage cbmem_imd_handle_storage_chipset(void)
{
	const struct native84_handoff *d = native84_descriptor();
	struct cbmem_imd_handle_storage storage;

	/* No CBMEM calls here. The exact immutable epoch/bank is rechecked on
	 * every public operation and nested hook. Each stage gets its own slot.
	 */
	storage.bank_base = d->handle_bank_base;
	storage.bank_bytes = d->handle_bank_bytes;
	storage.handle = (void *)(uintptr_t)(d->handle_bank_base +
		(ENV_RAMSTAGE ? sizeof(struct imd) : 0));
	return storage;
}
#endif

#if ENV_RAMSTAGE
static unsigned int recovery_attempted;
void __real_main(void);
void __wrap_main(void);
void __wrap_main(void)
{
	native84_stage_admit(&native84_handoff_parameters);
	if (_cbmem_top_ptr != native84_handoff_parameters.cbmem_top)
		native84_terminal();
	/* Genuine hardwaremain performs timer/console/post80/exception prefix.
	 * Checked CBMEM recovery returns; retained soc_init is the next terminal.
	 */
	__real_main();
	native84_terminal();
}

int __real_cbmem_initialize(void);
int __wrap_cbmem_initialize(void);
int __wrap_cbmem_initialize(void)
{
	native84_stage_check();
	if (recovery_attempted)
		native84_terminal();
	recovery_attempted = 1;
	if (__real_cbmem_initialize() || !cbmem_online())
		native84_terminal();
	native84_stage_check();
	printk(BIOS_INFO, "NATIVE85 STOCK C_START BOUNDED RECOVER RETURN\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	/* Permit PRE_DEVICE callbacks, then stop in retained soc_init before silicon. */
	return 0;
}
#endif
