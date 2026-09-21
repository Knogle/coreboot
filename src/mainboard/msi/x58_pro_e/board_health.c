/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <console/console.h>
#include <device/pci_def.h>
#include <southbridge/intel/common/pmutil.h>
#include <southbridge/intel/i82801jx/i82801jx.h>
#include <stdbool.h>
#include <stdint.h>
#include "board_health.h"
#include "platform_acpi.h"

#define HEALTH_LPC PCI_DEV(0, 0x1f, 0)
#define HEALTH_LPC_ID 0x3a168086u
#define HEALTH_GPIO_ENABLE 0x10
#define HEALTH_SIO_INDEX 0x4e
#define HEALTH_SIO_DATA 0x4f
#define HEALTH_SIO_LDN 0x07
#define HEALTH_SIO_DEVICE_ID 0x4105
#define HEALTH_SIO_VENDOR_ID 0x1934

static bool fintek_selector_failed;

static uint8_t sio_read(uint8_t index)
{
	outb(index, HEALTH_SIO_INDEX);
	return inb(HEALTH_SIO_DATA);
}

static void sio_select(uint8_t ldn)
{
	outb(HEALTH_SIO_LDN, HEALTH_SIO_INDEX);
	outb(ldn, HEALTH_SIO_DATA);
}

static void report_fintek(const char *phase)
{
	/* The existing bootblock/X58_LEGACY_INPUT paths and superiotool's 0x4105 entry
	 * establish 4e/4f, 87/87 entry, aa exit, ID and these LDN registers.
	 * This is NOT another Fintek model's init routine. No global, enable,
	 * base, IRQ, GPIO or hardware-monitor data register is written.
	 */
	static const uint8_t ldns[] = { 1, 4, 5 };
	static const uint8_t gpio_groups[] = { 0xe0, 0xd0, 0xc0, 0xb0, 0xf0 };
	uint8_t config[3][5], gpio[5][4];
	uint8_t old_ldn, restored_ldn, power_down, uart_share;
	uint16_t device_id, vendor_id;

	if (fintek_selector_failed) {
		printk(BIOS_WARNING, "[BOARD-HEALTH] %s FINTEK=SKIP_PRIOR_SELECTOR_FAILURE\n",
			phase);
		return;
	}
	outb(0x87, HEALTH_SIO_INDEX);
	outb(0x87, HEALTH_SIO_INDEX);
	device_id = sio_read(0x20);
	device_id |= (uint16_t)sio_read(0x21) << 8;
	vendor_id = (uint16_t)sio_read(0x23) << 8;
	vendor_id |= sio_read(0x24);
	if (device_id != HEALTH_SIO_DEVICE_ID || vendor_id != HEALTH_SIO_VENDOR_ID) {
		outb(0xaa, HEALTH_SIO_INDEX);
		printk(BIOS_WARNING, "[BOARD-HEALTH] %s FINTEK=SKIP_ID DID=%04x VID=%04x\n",
			phase, device_id, vendor_id);
		return;
	}
	old_ldn = sio_read(HEALTH_SIO_LDN);
	if (old_ldn > 0x0a) {
		outb(0xaa, HEALTH_SIO_INDEX);
		printk(BIOS_WARNING, "[BOARD-HEALTH] %s FINTEK=SKIP_LDN LDN=%02x\n",
			phase, old_ldn);
		return;
	}
	/* F71882 V0.28P 8.1.7/8.1.8: software power-down and UART sharing.
	 * Global25 is not a silicon revision identifier.
	 */
	power_down = sio_read(0x25);
	uart_share = sio_read(0x26);
	for (unsigned int i = 0; i < sizeof(ldns); i++) {
		sio_select(ldns[i]);
		config[i][0] = sio_read(0x30);
		config[i][1] = sio_read(0x60);
		config[i][2] = sio_read(0x61);
		config[i][3] = sio_read(0x70);
		config[i][4] = ldns[i] == 5 ? sio_read(0x72) : 0;
	}
	/* F71882 GPIO configuration groups from superiotool/fintek.c. Raw
	 * values only: board net names/attached fans cannot be inferred here.
	 */
	sio_select(6);
	for (unsigned int i = 0; i < sizeof(gpio_groups); i++)
		for (unsigned int j = 0; j < sizeof(gpio[i]); j++)
			gpio[i][j] = sio_read(gpio_groups[i] + j);
	sio_select(old_ldn);
	restored_ldn = sio_read(HEALTH_SIO_LDN);
	outb(0xaa, HEALTH_SIO_INDEX);
	fintek_selector_failed = old_ldn != restored_ldn;

	printk(BIOS_DEBUG, "[BOARD-HEALTH] %s FINTEK_DID=%04x VID=%04x "
		"POWER_DOWN=%02x UART_SHARE=%02x LDN_RESTORE=%02x/%02x FUNCTION_CONFIG_WRITES=0\n",
		phase, device_id, vendor_id, power_down, uart_share, old_ldn, restored_ldn);
	if (old_ldn != restored_ldn)
		printk(BIOS_WARNING, "[BOARD-HEALTH] %s FINTEK_LDN_RESTORE_FAILED NO_RETRY\n",
			phase);
	for (unsigned int i = 0; i < sizeof(ldns); i++)
		printk(BIOS_DEBUG, "[BOARD-HEALTH] %s FINTEK_LDN=%02x "
			"EN=%02x BASE=%02x%02x IRQ=%02x AUX_IRQ=%02x\n",
			phase, ldns[i], config[i][0], config[i][1], config[i][2],
			config[i][3], config[i][4]);
	for (unsigned int i = 0; i < sizeof(gpio_groups); i++)
		printk(BIOS_DEBUG, "[BOARD-HEALTH] %s FINTEK_GPIO_REG=%02x "
			"RAW=%02x/%02x/%02x/%02x\n", phase, gpio_groups[i],
			gpio[i][0], gpio[i][1], gpio[i][2], gpio[i][3]);
	printk(BIOS_DEBUG, "[BOARD-HEALTH] %s HWM_RUNTIME=NOT_ACCESSED "
		"FAN_POLICY=UNCHANGED PACKAGE=F71882_OR_F71883\n", phase);
}

void x58_board_health_report(const char *phase)
{
	const uint32_t saved_cf8 = inl(PCI_IO_CONFIG_INDEX);
	const uint32_t id = pci_io_read_config32(HEALTH_LPC, PCI_VENDOR_ID);
	uint32_t gpio_raw, route;
	uint16_t lpc_en, pmcon3;
	uint8_t gpio_ctl;
	const uint16_t gpio_base = x58_platform_gpiobase();

	if (id != HEALTH_LPC_ID) {
		outl(saved_cf8, PCI_IO_CONFIG_INDEX);
		printk(BIOS_WARNING, "[BOARD-HEALTH] %s SKIP_LPC_ID=%08x\n", phase, id);
		return;
	}
	lpc_en = pci_io_read_config16(HEALTH_LPC, LPC_EN);
	gpio_raw = pci_io_read_config32(HEALTH_LPC, GPIOBASE);
	gpio_ctl = pci_io_read_config8(HEALTH_LPC, D31F0_GPIO_CNTL);
	route = pci_io_read_config32(HEALTH_LPC, D31F0_GPIO_ROUT);
	pmcon3 = pci_io_read_config16(HEALTH_LPC, D31F0_GEN_PMCON_3);
	outl(saved_cf8, PCI_IO_CONFIG_INDEX);

	/* ICH10 319973-003 13.6.2.4: VRT is hardwired1, D[5:0] is a date
	 * alarm, and a Status C read acknowledges events. None are sampled.
	 * GEN_PMCON3 RTC_PWR_STS is sticky provenance, NOT battery voltage or
	 * proof of an ongoing fault. The existing LPC RTC policy owns clearing.
	 */
	printk(BIOS_DEBUG, "[BOARD-HEALTH] %s GEN_PMCON3=%04x RTC_PWR_STS=%u "
		"RTC_DATA=NOT_ACCESSED RTC_POLICY=STANDARD_PRESERVE_NVRAM "
		"VRT_IS_NOT_BATTERY_HEALTH=1\n", phase, pmcon3,
		!!(pmcon3 & RTC_BATTERY_DEAD));
	printk(BIOS_DEBUG, "[BOARD-HEALTH] %s GPIOBASE=%08x GPIO_CNTL=%02x "
		"GPIO_ROUT=%08x GPIO_WRITES=0\n", phase, gpio_raw, gpio_ctl, route);
	/* Read only the admitted native GPIO aperture. Unknown
	 * decode is skipped, never repaired/probed at arbitrary port addresses.
	 */
	if ((gpio_base == 0x500 || gpio_base == 0x580) &&
	    (gpio_raw & ~1u) == gpio_base && gpio_ctl == HEALTH_GPIO_ENABLE) {
		const uint32_t use1 = inl(gpio_base + GP_IO_USE_SEL);
		const uint32_t dir1 = inl(gpio_base + GP_IO_SEL);
		const uint32_t level1 = inl(gpio_base + GP_LVL);
		const uint32_t use2 = inl(gpio_base + GP_IO_USE_SEL2);
		const uint32_t dir2 = inl(gpio_base + GP_IO_SEL2);
		const uint32_t level2 = inl(gpio_base + GP_LVL2);
		printk(BIOS_DEBUG, "[BOARD-HEALTH] %s GPIO1 USE=%08x DIR=%08x LVL=%08x "
			"GPIO2 USE=%08x DIR=%08x LVL=%08x NATIVE_LEVELS_NOT_LATCH_PROOF=1\n",
			phase, use1, dir1, level1, use2, dir2, level2);
	} else {
		printk(BIOS_WARNING, "[BOARD-HEALTH] %s GPIO=SKIP_UNADMITTED_DECODE\n", phase);
	}
	if (lpc_en & CNF2_LPC_EN)
		report_fintek(phase);
	else
		printk(BIOS_WARNING, "[BOARD-HEALTH] %s FINTEK=SKIP_DISABLED_DECODE\n", phase);
}
