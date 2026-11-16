/* SPDX-License-Identifier: GPL-2.0-only */
/* Future Root-selected early-native trial: exact same-board Original inputs.
 * Direct SMU messages2a/29; no DXIO service9 opcode or DMA input buffer.
 * All49 tuples/order are byte-joined to the closed Enabled05 UART capture.
 */
#include "b565d4_cpu.h"
#include <commonlib/bsd/compiler.h>
#include "native_usb_boot.h"
#include "native84_profile.h"
#include <amdblocks/smn.h>
#include <amdblocks/smu.h>
#include <arch/cpu.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/uart.h>
#include <string.h>
#include <timer.h>

#define USB_SKIP_REGISTER 0x03810a84U
#define USB_SKIP_CONTROLLER (1U << 9)
#define USB_SKIP_PHY (1U << 10)
#define USB_WHOLE_BUDGET_MS 120000U

_Static_assert(CONFIG_SOC_AMD_COMMON_BLOCK_SMU && !CONFIG_COOP_MULTITASKING &&
	SMU_NUM_ARGS == 6 && SMN_SMU_MESG_ID == 0x03b10528U &&
	SMN_SMU_MESG_RESP == 0x03b10564U &&
	SMN_SMU_MESG_ARGS_BASE == 0x03b10998U,
	"Selected Cezanne genuine scalar SMU API");

struct usb_boot_request {
	uint32_t message_id;
	uint32_t args[6];
};

/* Address-looking words here are SMU arguments, not host SMN stores.
 * Basic12, OEM32, subsequence4, then final29 with six zero arguments.
 */
static const struct usb_boot_request usb_boot_requests[] = {
	{ 0x2a, { 0x00180128, 0x0003000f, 0x00000000, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180128, 0x0003000f, 0x00000000, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180120, 0x00ffffff, 0x00000000, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180120, 0x00ffffff, 0x00000000, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180130, 0x00000100, 0x00000100, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180130, 0x00000100, 0x00000100, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180114, 0x00030003, 0x00000000, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180114, 0x00030003, 0x00000000, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180130, 0x00030000, 0x00030000, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180130, 0x00030000, 0x00030000, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0000c12c, 0x00020000, 0x00020000, 0x01000003, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0000c12c, 0x00020000, 0x00020000, 0x01100003, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c000, 0xf3bf7007, 0x32133001, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c004, 0x0000000f, 0x0000000b, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c400, 0xf3bf7007, 0x32133001, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c404, 0x0000000f, 0x0000000b, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c800, 0xf3bf7007, 0x32133001, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c804, 0x0000000f, 0x0000000b, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011cc00, 0xf3bf7007, 0x32133001, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011cc04, 0x0000000f, 0x0000000b, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c000, 0xf3bf7007, 0x32133001, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c004, 0x0000000f, 0x0000000b, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c400, 0xf3bf7007, 0x32133001, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c404, 0x0000000f, 0x0000000b, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c800, 0xf3bf7007, 0x32133001, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011c804, 0x0000000f, 0x0000000b, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011cc00, 0xf3bf7007, 0x32133001, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0011cc04, 0x0000000f, 0x0000000b, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00168028, 0x00000707, 0x00000202, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00168428, 0x00000707, 0x00000202, 0x01000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180130, 0x0000f000, 0x00000000, 0x01000002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00120088, 0x000000f0, 0x00000050, 0x01000002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180130, 0x0000f000, 0x00001000, 0x01000002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00120088, 0x000000f0, 0x00000050, 0x01000002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00168028, 0x00000707, 0x00000202, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00168428, 0x00000707, 0x00000202, 0x01100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180130, 0x0000f000, 0x00000000, 0x01100002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00120088, 0x000000f0, 0x00000050, 0x01100002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00180130, 0x0000f000, 0x00001000, 0x01100002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00120088, 0x000000f0, 0x00000050, 0x01100002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0000d024, 0x10000000, 0x00000000, 0x01000003, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0000d0a4, 0x10000000, 0x00000000, 0x01000003, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0000d024, 0x10000000, 0x00000000, 0x01100003, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x0000d0a4, 0x10000000, 0x00000000, 0x01100003, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00000000, 0x00000000, 0x00000000, 0xc0000001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00000000, 0x00000000, 0x00000000, 0xc0000002, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00000000, 0x00000000, 0x00000000, 0xc0100001, 0x00000000, 0x00000000 } },
	{ 0x2a, { 0x00000000, 0x00000000, 0x00000000, 0xc0100002, 0x00000000, 0x00000000 } },
	{ 0x29, { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 } },
};
_Static_assert(ARRAY_SIZE(usb_boot_requests) == 49, "Exact Original USB call count");
static bool attempted;

static void usb_boot_stop(const char *why, size_t sequence) __noreturn;
static void usb_boot_stop(const char *why, size_t sequence)
{
	printk(BIOS_ERR, "NATIVE USB49 REFUSAL seq=%u reason=%s; no retry\n",
	       (unsigned int)sequence, why);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

static void usb_boot_failure_snapshot(size_t sequence, uint32_t message_id)
{
	uint32_t raw[6];
	const uint32_t response = smn_read32(SMN_SMU_MESG_RESP);

	/* One final observation only: no response/argument/message stores,
	 * no command replay. Response0 is in-flight, not valid returned data.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(raw); i++)
		raw[i] = smn_read32(SMN_SMU_MESG_ARG(i));
	printk(BIOS_ERR, "NATIVE USB49 FAILURE SNAPSHOT seq=%u id=%02x response=%08x "
	       "raw=%08x/%08x/%08x/%08x/%08x/%08x no_reissue=1\n",
	       (unsigned int)sequence, message_id, response,
	       raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

void native_usb_boot_prepare(void)
{
	struct stopwatch whole, call;
	struct smu_payload payload;
	const struct cpuid_result cpu = cpuid(1);
	uint32_t skip;

	if (native84_profile_policy.entry_path != 2)
		usb_boot_stop("early-native entry2 required", 0);
	native84_stage_check();
	if (attempted)
		usb_boot_stop("second attempt", 0);
	attempted = true;
	if (!b565d4_is_cezanne(cpu.eax))
		usb_boot_stop("Cezanne USB firmware required", 0);
	printk(BIOS_INFO, "NATIVE USB49 PREP BEGIN CPUID=%08x/%08x "
	       "skip_smn=%08x before_DXIO=1 requests=49 scalar_args_only=1\n",
	       cpu.eax, cpu.ebx, USB_SKIP_REGISTER);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	skip = smn_read32(USB_SKIP_REGISTER);
	printk(BIOS_INFO, "NATIVE USB49 SKIP raw=%08x controller=%u phy=%u\n", skip,
	       !!(skip & USB_SKIP_CONTROLLER), !!(skip & USB_SKIP_PHY));
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (skip & (USB_SKIP_CONTROLLER | USB_SKIP_PHY))
		usb_boot_stop("fresh skip bits9/10 must both be zero", 0);
	stopwatch_init_msecs_expire(&whole, USB_WHOLE_BUDGET_MS);

	for (size_t sequence = 0; sequence < ARRAY_SIZE(usb_boot_requests); sequence++) {
		const struct usb_boot_request *request = &usb_boot_requests[sequence];
		enum cb_err result;

		memcpy(payload.msg, request->args, sizeof(payload.msg));
		printk(BIOS_INFO, "NATIVE USB49 BEGIN seq=%u id=%02x "
		       "args=%08x/%08x/%08x/%08x/%08x/%08x\n",
		       (unsigned int)sequence, request->message_id,
		       payload.msg[0], payload.msg[1], payload.msg[2],
		       payload.msg[3], payload.msg[4], payload.msg[5]);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		/* Check immediately before each call, including prior UART time.
		 * One call retains the genuine API's own10s ready +10s completion.
		 */
		if (stopwatch_expired(&whole))
			usb_boot_stop("whole120s budget expired before call", sequence);
		stopwatch_init(&call);
		result = send_smu_message((enum smu_message_id)request->message_id, &payload);
		stopwatch_tick(&call);
		stopwatch_tick(&whole);
		if (result != CB_SUCCESS) {
			printk(BIOS_ERR, "NATIVE USB49 END seq=%u id=%02x result=%d "
			       "elapsed_us=%lld total_us=%lld returned_valid=0\n",
			       (unsigned int)sequence, request->message_id, result,
			       stopwatch_duration_usecs(&call), stopwatch_duration_usecs(&whole));
			usb_boot_failure_snapshot(sequence, request->message_id);
			usb_boot_stop("genuine scalar SMU request failed", sequence);
		}
		printk(BIOS_INFO, "NATIVE USB49 END seq=%u id=%02x result=%d "
		       "elapsed_us=%lld total_us=%lld returned=%08x/%08x/%08x/%08x/%08x/%08x\n",
		       (unsigned int)sequence, request->message_id, result,
		       stopwatch_duration_usecs(&call), stopwatch_duration_usecs(&whole),
		       payload.msg[0], payload.msg[1], payload.msg[2],
		       payload.msg[3], payload.msg[4], payload.msg[5]);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	}
	printk(BIOS_INFO, "NATIVE USB49 SCALAR REQUESTS COMPLETE49; genuine DXIO follows\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}
