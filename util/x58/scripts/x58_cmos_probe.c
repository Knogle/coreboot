/* SPDX-License-Identifier: GPL-2.0-only */

#define _GNU_SOURCE
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <unistd.h>

#define ICH10_LPC_CONFIG "/sys/bus/pci/devices/0000:00:1f.0/config"
#define ICH10_RCBA_REGISTER 0xf0
#define ICH10_RC_OFFSET 0x3400
#define ICH10_RC_U128E (1U << 2)

static unsigned char read_extended_cmos(unsigned char address)
{
	/* Port 0x72 selects bits 6:0 inside the enabled upper 128-byte bank. */
	outb(address & 0x7f, 0x72);
	return inb(0x73);
}

static int read_u32_at(const char *path, off_t offset, uint32_t *value)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	ssize_t count;

	if (fd < 0)
		return -1;
	count = pread(fd, value, sizeof(*value), offset);
	if (close(fd) != 0 || count != sizeof(*value))
		return -1;
	return 0;
}

static unsigned char read_standard_cmos(unsigned char selector)
{
	/* Bit 7 keeps NMI disabled exactly as the MSI early wrapper does. */
	outb(selector | 0x80, 0x70);
	(void)inb(0x61);
	return inb(0x71);
}

int main(void)
{
	static const unsigned char addresses[] = {
		0x81, 0x82, 0x88, 0x89, 0x8e, 0xca, 0xf1, 0xf5,
	};
	unsigned char saved_index_70;
	unsigned char saved_index_72;
	unsigned char alt_gp_smi_en_low;
	uint32_t rcba;
	uint32_t rtc_configuration;
	size_t i;

	if (read_u32_at(ICH10_LPC_CONFIG, ICH10_RCBA_REGISTER, &rcba) != 0) {
		fprintf(stderr, "ICH10 RCBA read: %s\n", strerror(errno));
		return 1;
	}
	if (!(rcba & 1) ||
	    read_u32_at("/dev/mem", (rcba & 0xffffc000U) + ICH10_RC_OFFSET,
		&rtc_configuration) != 0) {
		fprintf(stderr, "ICH10 RTC configuration read failed (RCBA=%08x): %s\n",
		       rcba, strerror(errno));
		return 1;
	}
	if (!(rtc_configuration & ICH10_RC_U128E)) {
		fprintf(stderr,
			"REFUSED: RCBA=%08x RTC_RC=%08x; upper-128 RTC decode is disabled and 72/73 would alias 70/71\n",
			rcba, rtc_configuration);
		return 1;
	}

	if (ioperm(0x70, 4, 1) != 0 || ioperm(0x538, 1, 1) != 0) {
		fprintf(stderr, "ioperm: %s\n", strerror(errno));
		return 1;
	}

	saved_index_70 = inb(0x70);
	saved_index_72 = inb(0x72);
	/* Match the original wrapper's byte read; only bits 6:4 are consumed. */
	alt_gp_smi_en_low = inb(0x538);
	printf("RCBA=%08x RTC_RC=%08x U128E=1 INDEX70=%02x INDEX72=%02x ALT_GP_SMI_EN_LOW=%02x FIELD_6_4=%u\n",
	       rcba, rtc_configuration, saved_index_70, saved_index_72,
	       alt_gp_smi_en_low,
	       (alt_gp_smi_en_low >> 4) & 7);
	printf("CMOS_DIAG_0E=%02x\n", read_standard_cmos(0x0e));
	for (i = 0; i < sizeof(addresses); i++)
		printf("CMOS_%02X=%02x\n", addresses[i],
		       read_extended_cmos(addresses[i]));

	/* Restore both selectors. No CMOS data register is ever written. */
	outb(saved_index_72, 0x72);
	outb(saved_index_70, 0x70);
	return 0;
}
