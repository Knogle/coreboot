/* SPDX-License-Identifier: GPL-2.0-only */
/* Root-selected invasive Original-AM4/CZN trial. No vendor PEIM execution.
 * Fixed policy is explicit below; it is not a claim to have recovered a live
 * 420-byte vendor source descriptor. Firmware-returned engine bytes are distinct.
 */
#include "b565d4_cpu.h"
#include <commonlib/bsd/compiler.h>
#include "czn_dxio.h"
#include "czn_pcie_topology.h"
#include "board_config.h"
#include "setup_options.h"
#include "native84_profile.h"
#include "native_apob_resources.h"
#include "../normal_uart2.h"
#include <arch/cpu.h>
#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/x86/cache.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/tsc.h>
#include <delay.h>
#include <device/pci_def.h>
#include <stdint.h>
#include <string.h>

uint32_t native197_uart_ff_retries(void);
uint32_t native198_uart_dropped_bytes(void);
uint32_t native198_uart_dropped_flushes(void);
uint32_t native198_uart_full_wait_caps(void);
uint32_t native198_uart_recoveries(void);
uint32_t native198_uart_degraded(void);
void native208_uart_mute(bool mute);

_Static_assert(sizeof(uintptr_t) == 4, "Selected flat IA32 direct-PA trial");
#define DXIO_CBMEM_ID 0x4358494fU /* CXIO, unique private persistent allocation */
#define DXIO_BYTES 16384U
#define DXIO_RESPONSE 0x03b10564U
#define DXIO_MESSAGE 0x03b10528U
#define DXIO_ARGS 0x03b10998U
#define DXIO_MAX_EVENTS 256U
#define DXIO_MAX_REQUESTS 1024U
#define DXIO_SIMPLE_US 1000000U
#define DXIO_POINTER_US 10000000U
/* Service 0x23 selects the engine buffer with 3 and ancillary with 6.
 * The engine count belongs in buffer+12, not this command argument.
 */
#define DXIO_ENGINE_BUFFER 3U

struct dxio_journal {
	uint32_t opcode, outer, raw0, arg1, arg2, arg3, success, sequence;
};
struct dxio_storage {
	uint8_t engine[CZN_MAX_ENGINE_BYTES] __aligned(64);
	uint8_t ancillary[CZN_MAX_ANCILLARY_BYTES] __aligned(64);
	struct dxio_port ports[CZN_MAX_PORTS];
	uint32_t record[128][2];
	struct dxio_journal journal[256]; /* Ring; every full request also UART logged. */
	uint32_t record_count;
	uint32_t saved_routes[256];
	uint8_t saved_route_valid[256];
	struct b565d4_uart2_requal uart_requal;
};
_Static_assert(sizeof(struct dxio_storage) <= DXIO_BYTES, "Owned DXIO arena");

static struct dxio_storage *store;
static size_t port_count, engine_bytes, ancillary_bytes;
static uint32_t vetting_ack_sequence;
static uint64_t first_ticks, last_ticks, total_ticks;
static uint32_t mhz, requests, events, attempted, mapped, configured, early;
static unsigned int routes_active;
/* Saved values remain immutable; saved_route_valid now marks pending restores. */
static uint32_t sb_route_held;
static bool routes_restore_failed;
static bool ack38_uart_pending, ack38_uart_attempted;
static bool ack38_ecam_window;
enum ack38_quiet_reason {
	ACK38_QUIET_REQUIRE = 1,
	ACK38_QUIET_READY_TIMEOUT,
	ACK38_QUIET_COMPLETION_TIMEOUT,
	ACK38_QUIET_DONE_WITHOUT_ACK,
};

/* Private RAM receipt: no CBMEM or 72-byte requalification layout change. */
static volatile struct {
	uint64_t grace_ticks;
	uint32_t outer, raw[6], reason, line;
	bool active, captured, done;
} ack38_quiet;

static struct {
	uint64_t begin, end;
	uint32_t polls, limit_us;
} last_wait;

static void ack38_quiet_stop(uint32_t reason, unsigned int line) __noreturn;
static void ack38_quiet_stop(uint32_t reason, unsigned int line)
{
	ack38_quiet.reason = reason;
	ack38_quiet.line = line;
	__asm__ __volatile__("" ::: "memory");

	/* Export an already-terminal quiet failure; keep the quiet waits unchanged.
	 * UART IO can still block, so an absent receipt is inconclusive.
	 */
	if (reason >= ACK38_QUIET_REQUIRE &&
	    reason <= ACK38_QUIET_DONE_WITHOUT_ACK) {
		native208_uart_mute(false);
		printk(BIOS_ERR, "NATIVE212 QUIET_STOP reason=%u line=%u "
		       "requests=%u events=%u active=%u ACKcaptured=%u done=%u window=%u "
		       "boundary=%u last_outer=%08x ACKraw0=%08x "
		       "ff=%u dropped_bytes=%u dropped_flushes=%u "
		       "diagnostic_only=1 no_requal=1 no_ACK_reissue=1\n",
		       reason, line, requests, events, ack38_quiet.active, ack38_quiet.captured,
		       ack38_quiet.done, ack38_ecam_window,
		       store ? store->uart_requal.boundary : 0,
		       ack38_quiet.outer, ack38_quiet.raw[0],
		       native197_uart_ff_retries(), native198_uart_dropped_bytes(),
		       native198_uart_dropped_flushes());
		printk(BIOS_ERR, "DXIO stopped: ports=%zu wait_us=%u polls=%u\n",
		       port_count, last_wait.limit_us, last_wait.polls);
		if (store) {
			const struct dxio_journal *j =
				&store->journal[(requests - 1) % ARRAY_SIZE(store->journal)];

			printk(BIOS_ERR, "DXIO pending: sequence=%u opcode=%08x\n",
			       j->sequence, j->opcode);
			for (size_t i = 0; i < port_count; i++) {
				const struct dxio_port *p = &store->ports[i];

				printk(BIOS_ERR, "DXIO port %zu lanes=%u..%u wrapper=%u "
				       "port=%u slot=%u devfn=%02x allocated=%u\n",
				       i, p->first, p->last, p->wrapper, p->port,
				       p->map_slot, p->devfn, p->allocated);
			}
		}
	}
	native84_terminal();
}

static int cleanup_routes(void);
static const uint32_t initial_requests[26][6];

static void flush(void)
{
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static void refusal(const char *reason, unsigned int line) __noreturn;
static void refusal(const char *reason, unsigned int line)
{
	if (ack38_quiet.active)
		ack38_quiet_stop(ACK38_QUIET_REQUIRE, line);
	printk(BIOS_ERR, "NATIVE108 REFUSAL %s line=%u requests=%u events=%u "
	       "mapped=%u configured=%u early=%u storage=%p retained_until_cold=1\n",
	       reason, line, requests, events, mapped, configured, early, store);
	flush();
	(void)cleanup_routes();
	native84_terminal();
}
#define REQUIRE(c, why) do { if (!(c)) refusal(why, __LINE__); } while (0)

int native106_dxio_budget_ok(void)
{
	uint64_t now = rdtscll();

	if (!mhz || now < last_ticks || now - first_ticks >= total_ticks)
		return 0;
	last_ticks = now;
	return 1;
}

int native106_dxio_delay(unsigned int usec)
{
	if (usec > 1000000U)
		return 0;
	for (unsigned int done = 0; done < usec;) {
		unsigned int step = MIN(10U, usec - done);

		if (!native106_dxio_budget_ok())
			return 0;
		udelay(step);
		done += step;
	}
	return native106_dxio_budget_ok();
}

void native106_dxio_io(const char *label, uint32_t address,
	uint32_t before, uint32_t after)
{
	printk(BIOS_INFO, "NATIVE108 IO %s address=%08x before=%08x after=%08x\n",
	       label, address, before, after);
	flush();
}

/* Every individual CF8/CFC access restores the complete incoming CF8 selector
 * before reporting. B8 is a write-only selection here and deliberately remains.
 */
static int cfg_once(uint8_t bus, uint8_t devfn, uint16_t reg, uint32_t *value, int write)
{
	uint32_t saved, selected;
	int ok;

	if (reg > 0xfc || (reg & 3) || bus > 127)
		return 0;
	saved = inl(PCI_IO_CONFIG_INDEX);
	selected = pci_io_encode_addr(PCI_DEV(bus, devfn >> 3, devfn & 7), reg);
	outl(selected, PCI_IO_CONFIG_INDEX);
	ok = inl(PCI_IO_CONFIG_INDEX) == selected;
	if (ok) {
		if (write)
			outl(*value, PCI_IO_CONFIG_DATA);
		else
			*value = inl(PCI_IO_CONFIG_DATA);
	}
	outl(saved, PCI_IO_CONFIG_INDEX);
	return inl(PCI_IO_CONFIG_INDEX) == saved && ok;
}

static int cfg(uint8_t bus, uint8_t devfn, uint16_t reg, uint32_t *value, int write)
{
	return native106_dxio_budget_ok() && cfg_once(bus, devfn, reg, value, write);
}

/* Same D0F0 identity target; no legacy selector while the quiet phase is live. */
static int rpc_identity(uint32_t *id)
{
	if (ack38_quiet.active) {
		if (!native106_dxio_budget_ok())
			return 0;
		*id = *(volatile uint32_t *)(uintptr_t)0xf0000000U;
		return 1;
	}
	return cfg(0, 0, 0, id, 0);
}

/* Error cleanup deliberately ignores the expired training budget, performs
 * one bounded attempt per saved bus0 Type1 bridge, and never recursively calls
 * refusal. It does not undo reset/SMU effects or free published storage.
 */
static int cleanup_routes_scope(bool keep_sb)
{
	uint32_t saved;
	int ok = 1;

	if (!routes_active || !store)
		return !sb_route_held && !routes_restore_failed;
	/* A failed finite attempt is terminal evidence, never an automatic retry. */
	if (routes_restore_failed)
		return 0;
	printk(BIOS_INFO, "NATIVE213 TEMPORARY ROUTES RESTORE keep_SB=%u\n", keep_sb);
	flush();
	saved = inl(PCI_IO_CONFIG_INDEX);
	for (unsigned int devfn = 0; devfn < 256; devfn++) {
		uint32_t selected, actual;

		if (!store->saved_route_valid[devfn] || (keep_sb && devfn == 0x11))
			continue;
		selected = pci_io_encode_addr(PCI_DEV(0, devfn >> 3, devfn & 7),
			PCI_PRIMARY_BUS);
		outl(selected, PCI_IO_CONFIG_INDEX);
		if (inl(PCI_IO_CONFIG_INDEX) != selected) {
			ok = 0;
			continue;
		}
		outl(store->saved_routes[devfn], PCI_IO_CONFIG_DATA);
		actual = inl(PCI_IO_CONFIG_DATA);
		if (actual == store->saved_routes[devfn])
			store->saved_route_valid[devfn] = 0;
		else
			ok = 0;
		printk(BIOS_INFO, "NATIVE108 ROUTE RESTORE devfn=%02x expected=%08x actual=%08x\n",
		       devfn, store->saved_routes[devfn], actual);
	}
	outl(saved, PCI_IO_CONFIG_INDEX);
	ok &= inl(PCI_IO_CONFIG_INDEX) == saved;
	routes_active = 0;
	for (unsigned int devfn = 0; devfn < 256; devfn++)
		if (store->saved_route_valid[devfn])
			routes_active = 1;
	if (!ok)
		routes_restore_failed = true;
	else if (!keep_sb)
		sb_route_held = 0;
	flush();
	return ok;
}

static int cleanup_routes(void)
{
	return cleanup_routes_scope(false);
}

/* The arena was zeroed before the sole census. Unsaved entries remain zero;
 * all saved complete DWORDs remain immutable after their valid bits clear.
 * A saved sibling route that includes bus2 must never be re-enabled while SB
 * owns that bus. This is a RAM check of ordinary secondary/subordinate ranges.
 */
static bool saved_sibling_routes_exclude_bus2(void)
{
	if (!store)
		return false;
	for (unsigned int devfn = 0; devfn < 256; devfn++) {
		uint32_t buses = store->saved_routes[devfn];
		unsigned int secondary = (buses >> 8) & 0xff;
		unsigned int subordinate = (buses >> 16) & 0xff;

		if (devfn != 0x11 && secondary <= 2 && subordinate >= 2)
			return false;
	}
	return true;
}

/* RAM-only ownership proof; no new config, SMN or endpoint access. */
static bool retained_sb_route_owned(void)
{
	if (!store || mapped != 1 || configured != 1 || early != 1 ||
	    routes_active != 1 || routes_restore_failed ||
	    !saved_sibling_routes_exclude_bus2() ||
	    port_count < 3 || store->ports[port_count - 1].devfn != 0x11 ||
	    !store->ports[port_count - 1].allocated ||
	    (store->saved_routes[0x11] & 0xff) ||
	    sb_route_held != ((store->saved_routes[0x11] & 0xff000000U) | 0x00020200U))
		return false;
	for (unsigned int devfn = 0; devfn < 256; devfn++)
		if (store->saved_route_valid[devfn] != (devfn == 0x11))
			return false;
	return true;
}

/* The retained native87 guard qualified this mapping before DXIO. The same
 * board's vendor SMU transport uses this ECAM B8/BC pair. Only the exact ACK38
 * window selects it here and remains active through validated DONE.
 * B8 stays selected as before; CF8 is never touched.
 */
_Static_assert(CONFIG_ECAM_MMCONF_BASE_ADDRESS == 0xf0000000U &&
	CONFIG_ECAM_MMCONF_BUS_NUMBER == 128 &&
	CONFIG_ECAM_MMCONF_LENGTH == 0x08000000U, "Retained guarded ECAM mapping");

static int ack38_ecam_once(uint16_t reg, uint32_t *value, int write)
{
	if (!ack38_ecam_window || !value || (reg != 0xb8 && reg != 0xbc))
		return 0;
	volatile uint32_t *target = (void *)(uintptr_t)(0xf0000000U + reg);

	if (write)
		*target = *value;
	else
		*value = *target;
	return 1;
}

static int smn(uint32_t address, uint32_t *value, int write)
{
	uint32_t index = address;

	if (ack38_ecam_window)
		return native106_dxio_budget_ok() && ack38_ecam_once(0xb8, &index, 1) &&
			native106_dxio_budget_ok() && ack38_ecam_once(0xbc, value, write);
	return cfg(0, 0, 0xb8, &index, 1) && cfg(0, 0, 0xbc, value, write);
}

static uint32_t smn_read(uint32_t address)
{
	uint32_t value = 0;

	printk(BIOS_INFO, "NATIVE108 SMN READ BEGIN %08x\n", address);
	flush();
	REQUIRE(smn(address, &value, 0), "SMN read/CF8 restore");
	native106_dxio_io("SMN READ", address, value, value);
	return value;
}

/* Exact ACK38 only: a quiet CPU interval before post-store device access. */
static uint64_t ack38_cpu_grace(void)
{
	uint64_t begin = rdtscll();

	for (uint32_t polls = 0; polls < 1000000000U; polls++) {
		__asm__ __volatile__("pause" ::: "memory");
		uint64_t now = rdtscll();

		if (now < begin)
			native84_terminal();
		if (now - begin >= 3600000000ULL)
			return now - begin;
	}
	/* A stopped TSC must not resume register or console traffic early. */
	native84_terminal();
}

/* The exact ACK38 receipt is released only after validated kind2/state0d.
 * No response1 interpretation replaces that event; never reissue the ACK.
 */
static void ack38_uart_after_wait(bool wait_ok, uint32_t response, uint64_t grace_ticks)
{
	if (!ack38_quiet.active || !ack38_quiet.done || !ack38_quiet.captured ||
	    !store || !ack38_uart_attempted || store->uart_requal.boundary != 2)
		ack38_quiet_stop(ACK38_QUIET_DONE_WITHOUT_ACK, __LINE__);
	store->uart_requal.result = b565d4_uart2_ack38_requalify(&store->uart_requal);
	store->uart_requal.boundary = 3;
	__asm__ __volatile__("" ::: "memory");
	const struct b565d4_uart2_requal *r = &store->uart_requal;

	/* DONE is already validated; the helper ran while TX was still muted. */
	native208_uart_mute(false);
	ack38_ecam_window = false;
	ack38_quiet.active = false;

	printk(BIOS_INFO, "NATIVE209 ACK38 UART REQUAL ECAM_ACK38=1 LPC_ECAM=1 after_DONE=1 event_tag=00000102 event_state=0d wait_ok=%u response=%08x "
	       "grace_us=1000000 grace_tsc_delta=%08x:%08x TSC_MHZ=%u "
	       "ACKraw=%08x/%08x/%08x/%08x/%08x/%08x attempted=%u result=%u "
	       "boundary=%u step=%u LPCid=%08x LPC44=%08x/%08x LPC48=%08x/%08x "
	       "LSR=%02x/%02x LCR=%02x/%02x DLAB=%02x ASTrev=%08x "
	       "SCU84=%08x SCU90=%08x CF8untouched=%u no_ACK_reissue=1\n",
	       wait_ok, response, (uint32_t)(grace_ticks >> 32), (uint32_t)grace_ticks, mhz,
	       ack38_quiet.raw[0], ack38_quiet.raw[1], ack38_quiet.raw[2],
	       ack38_quiet.raw[3], ack38_quiet.raw[4], ack38_quiet.raw[5],
	       r->attempted, r->result, r->boundary, r->helper_step, r->lpc_id,
	       r->lpc44_before, r->lpc44_after, r->lpc48_before, r->lpc48_after,
	       r->lsr_before, r->lsr_after, r->lcr_before, r->lcr_after,
	       r->dlab_readback, r->ast_revision, r->scu84_after, r->scu90_after,
	       r->cf8_untouched);
	flush();
	/* The store already completed before the wait; only its UART receipt moved. */
	native106_dxio_io("SMN WRITE ISSUED", DXIO_MESSAGE, 9, 9);
}

static void smn_write(uint32_t address, uint32_t value)
{
	bool requal = ack38_uart_pending && !ack38_uart_attempted && store &&
		requests == vetting_ack_sequence + 1 &&
		store->journal[vetting_ack_sequence % ARRAY_SIZE(store->journal)].sequence ==
			vetting_ack_sequence &&
		store->journal[vetting_ack_sequence % ARRAY_SIZE(store->journal)].opcode == 0x308 &&
		address == DXIO_MESSAGE && value == 9;

	REQUIRE(!requal || mhz == 3600, "ACK38 grace TSC3600");
	REQUIRE(!requal || retained_sb_route_owned(), "ACK38 owned retained SB route");

	printk(BIOS_INFO, "NATIVE108 SMN WRITE BEGIN %08x value=%08x\n", address, value);
	flush();
	if (requal) {
		ack38_uart_pending = false;
		ack38_uart_attempted = true;
		store->uart_requal.attempted = 1;
		store->uart_requal.boundary = 1;
		ack38_quiet.active = true;
		native208_uart_mute(true);
		ack38_ecam_window = true;
		__asm__ __volatile__("" ::: "memory");
	}
	REQUIRE(smn(address, &value, 1), "SMN write/provider");
	if (requal) {
		store->uart_requal.boundary = 2;
		__asm__ __volatile__("" ::: "memory");
		/* Do not touch the console or requalify providers while ACK38 is pending. */
		return;
	}
	native106_dxio_io("SMN WRITE ISSUED", address, value, value);
}

static void smn_rmw(uint32_t address, uint32_t mask, uint32_t bits)
{
	uint32_t before = smn_read(address), value = (before & mask) | bits;

	smn_write(address, value);
	/* This is the actual readback, not a promise about undocumented W1C bits. */
	native106_dxio_io("SMN RMW READBACK", address, before, smn_read(address));
}

/* The known GnbSetTomRN current-map branch. This programs NBIO's copy of TOM,
 * not CPU MTRRs or a new RAM ownership map. Original09c58b30..09c58d13 supplies
 * these three RMWs; auxiliary TOM3/default/control writes remain unselected.
 */
static void nbio_tom_setup(void)
{
	static const uint32_t auxiliary[] = {
		0x13b10138U, 0x13b1005cU, 0x13b10060U, 0x13b0004cU,
	};
	msr_t sys = rdmsr(0xc0010010U), top = rdmsr(0xc001001aU);
	msr_t top2 = rdmsr(0xc001001dU);
	uint32_t id = 0, before90 = 0, after90 = 0, old64, old68;
	uint32_t expected90, expected64, expected68, before_aux[4];

	printk(BIOS_INFO, "NATIVE108 NBIO TOM BEGIN SYS=%08x:%08x TOM=%08x:%08x "
	       "TOM2=%08x:%08x store=%p\n", sys.hi, sys.lo, top.hi, top.lo,
	       top2.hi, top2.lo, store);
	flush();
	REQUIRE(!sys.hi && sys.lo == 0x007c0000U && !top.hi &&
		top.lo == 0xc0000000U && top2.raw > 0x100000000ULL &&
		!(top2.lo & 0x007fffffU) && !(top2.hi & ~0xffU) &&
		native_apob_require_top(top2.raw) == NATIVE_APOB_OK,
		"fresh representable TOM2 agrees with current owned APOB top");
	printk(BIOS_INFO, "NATIVE222 NBIO TOM2 APOB AGREEMENT raw=%08x:%08x "
	       "representable=1 owned_snapshot=1 fallback=0\n", top2.hi, top2.lo);
	flush();
	/* This revalidates the current own CBMEM APOB snapshot and its raw1 PCI
	 * hole. The existing capture/inspector already proves the parent's gap.
	 */
	REQUIRE(native_apob_require_pci_hole(0xc0000000ULL, 0xf0000000ULL) ==
		NATIVE_APOB_OK && store &&
		(uint64_t)(uintptr_t)store + sizeof(*store) <= top.lo,
		"owned APOB/current PCI hole and buffer below TOM");
	REQUIRE(cfg(0, 0, 0, &id, 0) && id == 0x16301022U &&
		cfg(0, 0, 0x90, &before90, 0), "NBIO TOM config identity/read");
	old64 = smn_read(0x13b10064U);
	old68 = smn_read(0x13b10068U);
	for (unsigned int i = 0; i < ARRAY_SIZE(auxiliary); i++)
		before_aux[i] = smn_read(auxiliary[i]);
	expected90 = (before90 & 0x007fffffU) | (top.lo & 0xff800000U);
	expected64 = (old64 & 0x007ffffeU) | (top2.lo & 0xff800000U) | 1U;
	/* Original uses high8 and clears9 bits. The guard above admits only
	 * high8-representable values, preserving this existing RMW expression.
	 */
	expected68 = (old68 & 0xfffffe00U) | (top2.hi & 0x1ffU);
	printk(BIOS_INFO, "NATIVE108 NBIO TOM WRITE config90=%08x->%08x "
	       "smn64=%08x->%08x smn68=%08x->%08x\n", before90, expected90,
	       old64, expected64, old68, expected68);
	flush();
	REQUIRE(cfg(0, 0, 0x90, &expected90, 1) &&
		cfg(0, 0, 0x90, &after90, 0), "NBIO TOM config write/readback");
	native106_dxio_io("CONFIG90 TOM READBACK", 0x90, before90, after90);
	REQUIRE(after90 == expected90, "NBIO TOM config mismatch");
	smn_write(0x13b10064U, expected64);
	uint32_t after64 = smn_read(0x13b10064U);
	native106_dxio_io("TOM2 LOW READBACK", 0x13b10064U, old64, after64);
	REQUIRE(after64 == expected64, "NBIO TOM2 low mismatch");
	smn_write(0x13b10068U, expected68);
	uint32_t after68 = smn_read(0x13b10068U);
	native106_dxio_io("TOM2 HIGH READBACK", 0x13b10068U, old68, after68);
	REQUIRE(after68 == expected68, "NBIO TOM2 high mismatch");
	for (unsigned int i = 0; i < ARRAY_SIZE(auxiliary); i++)
		native106_dxio_io("AUX OBSERVATION ONLY", auxiliary[i],
			before_aux[i], smn_read(auxiliary[i]));
	printk(BIOS_INFO, "NATIVE108 NBIO TOM THREE RMW COMPLETE; AUX WRITES=0\n");
	flush();
}

static int wait_response(uint32_t *response, uint32_t limit_us)
{
	last_wait.begin = last_wait.end = rdtscll();
	last_wait.polls = 0;
	last_wait.limit_us = limit_us;
	*response = 0;

	for (unsigned int count = 0; count < limit_us / 10U; count++) {

		if (!native106_dxio_budget_ok() || !smn(DXIO_RESPONSE, response, 0))
			return 0;
		last_wait.polls++;
		last_wait.end = rdtscll();
		if (*response)
			return 1;
		if (last_wait.end < last_wait.begin ||
		    last_wait.end - last_wait.begin >= (uint64_t)mhz * limit_us ||
		    !native106_dxio_delay(10))
			return 0;
	}
	return 0;
}

/* Separate outer2 GetVersion, not inner DXIO opcode2. This command writes
 * RESPONSE and MESSAGE, but never the argument bank. Keep all DXIO sequence,
 * journal, callback and ACK accounting unchanged. Existing smn() is the same
 * pre-ACK CF8/B8/BC provider; direct calls retain explicit failure receipts.
 */
static void native210_smu_get_version(void)
{
	static bool queried;
	uint32_t id = 0, ready = 0, outer = 0, version = 0, value = 0;
	uint32_t stage = 1, ready_polls = 0, completion_polls = 0;
	uint32_t clear_attempted = 0, message_attempted = 0, arg0_reads = 0;
	uint64_t ready_ticks = 0, completion_ticks = 0;
	int ready_ok = 0, completion_ok = 0, arg0_ok = 0;
	const char *failure = NULL;

	REQUIRE(!queried && attempted == 1 && store && !requests && !events &&
		!mapped && !configured && !early && !routes_active &&
		!ack38_uart_pending && !ack38_uart_attempted && !ack38_ecam_window &&
		!ack38_quiet.active && !ack38_quiet.captured && !ack38_quiet.done &&
		native106_dxio_budget_ok(), "GetVersion once/pre-DXIO phase");
	queried = true;
	printk(BIOS_INFO, "NATIVE210 SMU GETVERSION BEGIN outer=00000002 "
	       "RESPONSE=03b10564 MESSAGE=03b10528 ARG0=03b10998 "
	       "ready_us=1000000 completion_us=1000000 ARG_writes=0 "
	       "command_writes=RESPONSE0_then_MESSAGE2 DXIO_requests=%u events=%u\n",
	       requests, events);
	flush();
	if (!rpc_identity(&id) || id != 0x16301022U) {
		failure = "GetVersion D0F0 identity/provider";
		goto report;
	}
	stage = 2;
	ready_ok = wait_response(&ready, DXIO_SIMPLE_US);
	ready_ticks = last_wait.end - last_wait.begin;
	ready_polls = last_wait.polls;
	if (!ready_ok || ready != 1) {
		failure = "GetVersion readiness status/timeout/provider";
		goto report;
	}
	stage = 3;
	clear_attempted = 1;
	if (!smn(DXIO_RESPONSE, &value, 1)) {
		failure = "GetVersion RESPONSE0 provider";
		goto report;
	}
	stage = 4;
	if (!smn(DXIO_RESPONSE, &value, 0) || value) {
		failure = "GetVersion RESPONSE0 readback/provider";
		goto report;
	}
	stage = 5;
	value = 2;
	__asm__ __volatile__("mfence" ::: "memory");
	message_attempted = 1;
	if (!smn(DXIO_MESSAGE, &value, 1)) {
		failure = "GetVersion MESSAGE2 provider";
		goto report;
	}
	stage = 6;
	completion_ok = wait_response(&outer, DXIO_SIMPLE_US);
	completion_ticks = last_wait.end - last_wait.begin;
	completion_polls = last_wait.polls;
	if (!completion_ok || outer != 1) {
		failure = "GetVersion completion status/timeout/provider";
		goto report;
	}
	stage = 7;
	arg0_reads = 1;
	arg0_ok = smn(DXIO_ARGS, &version, 0);
	if (!arg0_ok) {
		failure = "GetVersion ARG0 provider";
		goto report;
	}
	stage = 8;
report:
	printk(BIOS_INFO, "NATIVE210 SMU GETVERSION RESULT stage=%u id=%08x "
	       "ready_ok=%u ready=%08x completion_ok=%u response=%08x "
	       "ARG0_valid=%u ARG0=%08x clear_attempted=%u message_attempted=%u "
	       "ARG0_reads=%u ARG_writes=0 ready_polls=%u completion_polls=%u "
	       "ready_tsc_delta=%08x:%08x completion_tsc_delta=%08x:%08x "
	       "TSC_MHZ=%u ready_us=1000000 completion_us=1000000 "
	       "DXIO_requests=%u events=%u failed=%u version_required=0\n",
	       stage, id, ready_ok, ready, completion_ok, outer, arg0_ok, version,
	       clear_attempted, message_attempted, arg0_reads, ready_polls,
	       completion_polls, (uint32_t)(ready_ticks >> 32), (uint32_t)ready_ticks,
	       (uint32_t)(completion_ticks >> 32), (uint32_t)completion_ticks, mhz,
	       requests, events, failure != NULL);
	flush();
	if (failure)
		refusal(failure, __LINE__);
}

/* One bounded diagnostic snapshot even if the overall budget expired. Only
 * CF8/B8 selectors are written; no response/message/argument register is
 * written, no operation is reissued, and the published storage remains live.
 * Raw words with response0 are in-flight observations, not valid RPC output.
 */
static void timeout_snapshot(const char *phase, uint32_t opcode)
{
	uint32_t raw[7] = { 0 }, valid = 0;
	uint64_t end;

	for (unsigned int i = 0; i < ARRAY_SIZE(raw); i++) {
		uint32_t address = i ? DXIO_ARGS + 4U * (i - 1) : DXIO_RESPONSE;

		if (ack38_ecam_window ?
		    (ack38_ecam_once(0xb8, &address, 1) &&
		     ack38_ecam_once(0xbc, &raw[i], 0)) :
		    (cfg_once(0, 0, 0xb8, &address, 1) &&
		     cfg_once(0, 0, 0xbc, &raw[i], 0)))
			valid |= 1U << i;
	}
	end = rdtscll();
	printk(BIOS_ERR, "NATIVE108 RPC TIMEOUT phase=%s op=%08x limit_us=%u "
	       "polls=%u start_tsc=%08x:%08x last_tsc=%08x:%08x "
	       "snapshot_tsc=%08x:%08x valid=%02x response=%08x "
	       "uncompleted_raw=%08x/%08x/%08x/%08x/%08x/%08x no_reissue=1\n",
	       phase, opcode, last_wait.limit_us, last_wait.polls,
	       (uint32_t)(last_wait.begin >> 32), (uint32_t)last_wait.begin,
	       (uint32_t)(last_wait.end >> 32), (uint32_t)last_wait.end,
	       (uint32_t)(end >> 32), (uint32_t)end, valid,
	       raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6]);
	flush();
}

static void visibility(const char *where)
{
	uint32_t max = cpuid_eax(0x80000000U);
	msr_t sys = rdmsr(0xc0010010U);
	struct cpuid_result sme = { 0 };

	if (max >= 0x8000001fU)
		sme = cpuid_ext(0x8000001fU, 0);
	printk(BIOS_INFO, "NATIVE108 VISIBILITY %s SYSCFG=%08x:%08x "
	       "CPUID1F=%08x/%08x WBINVD BEGIN\n", where, sys.hi, sys.lo,
	       sme.eax, sme.ebx);
	flush();
	/* Root-selected unencrypted physical-address publication, not a DMA lease. */
	REQUIRE(!(sys.lo & (1U << 23)), "SME active direct-PA unsupported");
	__asm__ __volatile__("wbinvd" ::: "memory");
	printk(BIOS_INFO, "NATIVE108 WBINVD END %s\n", where);
	flush();
}

/* Only the finite operation families used below. Published pointer roles and
 * exact lengths are checked here, independently of the native caller.
 */
static int allowed_request(const uint32_t a[6])
{
	switch (a[0]) {
	case 0: case 0x13: case 0x24: case 0x307: case 0x308: case 9:
		return !(a[1] | a[2] | a[3] | a[4] | a[5]);
	case 0x14:
		return !a[1] && a[2] == (uintptr_t)store->engine &&
			a[3] == engine_bytes / 4 && !a[4] && !a[5];
	case 0x23:
		if (a[1])
			return 0;
		return (a[2] == (uintptr_t)store->engine && a[3] == engine_bytes / 4 &&
			!a[4] && a[5] == DXIO_ENGINE_BUFFER) ||
			(a[2] == (uintptr_t)store->ancillary && a[3] == ancillary_bytes / 4 &&
			!a[4] && a[5] == 6) ||
			(!a[2] && !a[3] && a[4] == 1 && (a[5] == 2 || a[5] == 5));
	case 0x22:
		for (unsigned int i = 0; i < ARRAY_SIZE(initial_requests); i++)
			if (initial_requests[i][0] == 0x22 &&
			    !memcmp(a, initial_requests[i], 6 * sizeof(uint32_t)))
				return 1;
		return 0;
	case 0x3a:
		return a[1] == 1 && ((a[2] == 2 && a[3] == 0x30d40 &&
			a[4] == 0x30d40 && a[5] == 0x927c0) ||
			(!a[2] && a[3] == 9 && a[4] == 1 && !a[5]) ||
			(a[2] == 4 && a[3] <= 1 && a[4] >= 0xa1 &&
			 a[4] <= 0xa4 && a[5] <= 15));
	default:
		return 0;
	}
}

static void request(uint32_t a[6])
{
	uint32_t outer = 0, ready = 0, id = 0, opcode = a[0];
	uint32_t completion_us = ((opcode == 0x23 && a[2]) || opcode == 0x14) ?
		DXIO_POINTER_US : DXIO_SIMPLE_US;
	uint64_t grace_ticks = 0;
	struct dxio_journal *j;

	REQUIRE(store && allowed_request(a) && requests < DXIO_MAX_REQUESTS &&
		native106_dxio_budget_ok(), "RPC family/count/time");
	printk(BIOS_INFO, "NATIVE108 RPC BEGIN seq=%u input=%08x/%08x/%08x/%08x/%08x/%08x\n",
	       requests, a[0], a[1], a[2], a[3], a[4], a[5]);
	flush();
	j = &store->journal[requests % ARRAY_SIZE(store->journal)];
	memset(j, 0, sizeof(*j));
	j->opcode = opcode;
	j->sequence = requests++;
	/* Reset callbacks may follow VETTING before DONE. Allow link training
	 * to complete on every quiet event query, not only the first one after
	 * the acknowledgement. Readiness and the whole-training budget remain
	 * unchanged; no outstanding request is reissued.
	 */
	if (ack38_quiet.active && ack38_quiet.captured && !ack38_quiet.done &&
	    ack38_ecam_window && opcode == 9 &&
	    !a[1] && !a[2] && !a[3] && !a[4] && !a[5])
		completion_us = DXIO_POINTER_US;
	REQUIRE(rpc_identity(&id) && id == 0x16301022U, "D0F0 renewed");
	if (!wait_response(&ready, DXIO_SIMPLE_US)) {
		if (ack38_quiet.active)
			ack38_quiet_stop(ACK38_QUIET_READY_TIMEOUT, __LINE__);
		timeout_snapshot("ready", opcode);
		refusal("mailbox ready timeout", __LINE__);
	}
	REQUIRE(ready == 1, "mailbox not ready");
	if ((opcode == 0x23 && a[2]) || opcode == 0x14)
		visibility("publish/READ target");
	smn_write(DXIO_RESPONSE, 0);
	for (unsigned int i = 0; i < 6; i++)
		smn_write(DXIO_ARGS + 4 * i, a[i]);
	__asm__ __volatile__("mfence" ::: "memory");
	smn_write(DXIO_MESSAGE, 9);
	bool deferred_uart = j->sequence == vetting_ack_sequence && opcode == 0x308 &&
		ack38_uart_attempted && store->uart_requal.boundary == 2;
	if (deferred_uart)
		grace_ticks = ack38_cpu_grace();
	/* Original 1s completion cap starts after the separate CPU-only grace.
	 * The unchanged whole-training 60s budget includes that grace.
	 */
	if (!wait_response(&outer, completion_us)) {
		if (ack38_quiet.active) {
			ack38_quiet.outer = outer;
			ack38_quiet_stop(ACK38_QUIET_COMPLETION_TIMEOUT, __LINE__);
		}
		timeout_snapshot("completion", opcode);
		ack38_ecam_window = false;
		refusal("mailbox completion timeout", __LINE__);
	}
	for (unsigned int i = 0; i < 6; i++)
		REQUIRE(smn(DXIO_ARGS + 4 * i, &a[i], 0), "RPC output/provider");
	j->outer = outer;
	j->raw0 = a[0]; j->arg1 = a[1]; j->arg2 = a[2]; j->arg3 = a[3];
	if (deferred_uart) {
		ack38_quiet.grace_ticks = grace_ticks;
		ack38_quiet.outer = outer;
		for (unsigned int i = 0; i < 6; i++)
			ack38_quiet.raw[i] = a[i];
	}
	if (!ack38_quiet.active)
		ack38_ecam_window = false;
	uint64_t wait_ticks = last_wait.end - last_wait.begin;
	printk(BIOS_INFO, "NATIVE108 RPC END seq=%u outer=%08x raw=%08x/%08x/%08x/%08x/%08x/%08x "
	       "completion_tsc_delta=%08x:%08x TSC_MHZ=%u limit_us=%u polls=%u\n",
	       j->sequence, outer, a[0], a[1], a[2], a[3], a[4], a[5],
	       (uint32_t)(wait_ticks >> 32), (uint32_t)wait_ticks, mhz,
	       last_wait.limit_us, last_wait.polls);
	flush();
	REQUIRE(outer == 1 && (a[0] == 3 ||
		(opcode == 0x307 && a[0] == 0x307) ||
		(opcode == 0x308 && a[0] == 0x308)), "RPC outer/raw status");
	j->success = 1;
	if (deferred_uart)
		ack38_quiet.captured = true;
	if (j->sequence == vetting_ack_sequence && opcode == 0x308) {
		printk(BIOS_INFO, "NATIVE198 UART OBSERVATION rpc=%u opcode=00000308 "
		       "ff=%u dropped_bytes=%u dropped_flushes=%u full_wait_caps=%u "
		       "recoveries=%u degraded=%u per_wait_cap=%u ff_not_ready=1\n",
		       j->sequence, native197_uart_ff_retries(), native198_uart_dropped_bytes(),
		       native198_uart_dropped_flushes(), native198_uart_full_wait_caps(),
		       native198_uart_recoveries(), native198_uart_degraded(),
		       native84_profile_policy.uart_max_polls);
		flush();
	}
	if (opcode == 0x14)
		visibility("completed SMU engine write");
}

static void simple_request(uint32_t opcode)
{
	uint32_t a[6] = { opcode, 0, 0, 0, 0, 0 };

	request(a);
}

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put32(uint8_t *p, uint32_t value)
{
	for (unsigned int i = 0; i < 4; i++)
		p[i] = value >> (8 * i);
}

static const uint8_t initial_ancillary[352] = {
	0x06, 0x00, 0x57, 0x00, 0x07, 0x01, 0x56, 0x00, 0x2f, 0x00, 0x01, 0x00, 0x08, 0x65, 0x00, 0x00,
	0x10, 0x65, 0x00, 0x00, 0x18, 0xe5, 0x00, 0x00, 0x20, 0xe5, 0x00, 0x00, 0xa8, 0x63, 0x00, 0x00,
	0xd0, 0x03, 0x00, 0x00, 0xc0, 0x02, 0x00, 0x00, 0x58, 0x20, 0x00, 0x00, 0x70, 0x03, 0x00, 0x00,
	0x78, 0x03, 0x00, 0x00, 0x80, 0x03, 0x00, 0x00, 0x88, 0x03, 0x00, 0x00, 0x90, 0x03, 0x00, 0x00,
	0x98, 0x03, 0x00, 0x00, 0xa0, 0x03, 0x00, 0x00, 0xe8, 0x20, 0x00, 0x00, 0xe3, 0x04, 0x00, 0x00,
	0x53, 0x14, 0x00, 0x00, 0xdb, 0x04, 0x00, 0x00, 0x49, 0x18, 0x00, 0x00, 0x1a, 0x47, 0x00, 0x00,
	0x62, 0xe7, 0x00, 0x00, 0x22, 0x24, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x8a, 0x00, 0x00, 0x00,
	0x92, 0x00, 0x00, 0x00, 0x22, 0x02, 0x00, 0x00, 0x7a, 0x00, 0x00, 0x00, 0x09, 0x50, 0x47, 0x00,
	0x09, 0xa0, 0x09, 0x00, 0x09, 0x90, 0x09, 0x00, 0x09, 0x80, 0x09, 0x00, 0x09, 0x98, 0x09, 0x00,
	0x09, 0x88, 0x09, 0x00, 0x09, 0xb0, 0x09, 0x00, 0x09, 0xb8, 0x09, 0x00, 0x09, 0xc8, 0xa9, 0x00,
	0x11, 0x50, 0x47, 0x00, 0x11, 0xa0, 0x09, 0x00, 0x11, 0x90, 0x09, 0x00, 0x11, 0x80, 0x09, 0x00,
	0x11, 0x98, 0x09, 0x00, 0x11, 0x88, 0x09, 0x00, 0x11, 0xb0, 0x09, 0x00, 0x11, 0xb8, 0x09, 0x00,
	0x11, 0xc8, 0xa9, 0x00, 0x26, 0x00, 0x00, 0x00, 0x08, 0x65, 0x00, 0x00, 0x10, 0x65, 0x00, 0x00,
	0x18, 0xe5, 0x00, 0x00, 0x20, 0xe5, 0x00, 0x00, 0xa8, 0x43, 0x00, 0x00, 0xd0, 0x03, 0x00, 0x00,
	0xc0, 0x02, 0x00, 0x00, 0x58, 0x20, 0x00, 0x00, 0x70, 0x03, 0x00, 0x00, 0x78, 0x03, 0x00, 0x00,
	0x80, 0x03, 0x00, 0x00, 0x88, 0x03, 0x00, 0x00, 0x90, 0x03, 0x00, 0x00, 0x98, 0x03, 0x00, 0x00,
	0xa0, 0x03, 0x00, 0x00, 0xe8, 0x20, 0x00, 0x00, 0xe3, 0x04, 0x00, 0x00, 0x53, 0x14, 0x00, 0x00,
	0xdb, 0x04, 0x00, 0x00, 0x49, 0x18, 0x00, 0x00, 0x1a, 0x47, 0x00, 0x00, 0x62, 0xe7, 0x00, 0x00,
	0x22, 0x24, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x92, 0x00, 0x00, 0x00,
	0x22, 0x02, 0x00, 0x00, 0x7a, 0x00, 0x00, 0x00, 0x01, 0x50, 0x47, 0x00, 0x01, 0xa0, 0x09, 0x00,
	0x01, 0x90, 0x09, 0x00, 0x01, 0x80, 0x09, 0x00, 0x01, 0x98, 0x09, 0x00, 0x01, 0x88, 0x09, 0x00,
	0x01, 0xb0, 0x09, 0x00, 0x01, 0xb8, 0x09, 0x00, 0x01, 0xc8, 0xa9, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint32_t initial_requests[26][6] = {
	{ 0x13U, 0x0U, 0x0U, 0x0U, 0x0U, 0x0U },
	{ 0x0U, 0x0U, 0x0U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x22U, 0x0U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x1cU, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x21U, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x22U, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x1aU, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x1bU, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x1dU, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x23U, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x24U, 0x1U, 0x50U, 0x0U, 0x0U },
	{ 0x22U, 0xeU, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0xfU, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x10U, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x1fU, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0x14U, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x22U, 0xdU, 0x0U, 0x0U, 0x0U, 0x0U },
	{ 0x24U, 0x0U, 0x0U, 0x0U, 0x0U, 0x0U },
	{ 0x23U, 0x0U, 0x0U, 0x58U, 0x0U, 0x6U },
	{ 0x23U, 0x0U, 0x0U, 0x0U, 0x1U, 0x2U },
	{ 0x23U, 0x0U, 0x0U, 0x0U, 0x1U, 0x5U },
	{ 0x23U, 0x0U, 0x0U, 0x24U, 0x0U, 0x3U },
	{ 0x3aU, 0x1U, 0x2U, 0x30d40U, 0x30d40U, 0x927c0U },
	{ 0x3aU, 0x1U, 0x0U, 0x9U, 0x1U, 0x0U },
	{ 0x22U, 0x23U, 0x1U, 0x0U, 0x0U, 0x0U },
	{ 0x307U, 0x0U, 0x0U, 0x0U, 0x0U, 0x0U },
};



static void engine_read(void)
{
	uint32_t a[6] = { 0x14, 0, (uintptr_t)store->engine, engine_bytes / 4, 0, 0 };

	request(a);
	printk(BIOS_INFO, "NATIVE266 ENGINE COUNT requested=%zu returned=%u bytes=%zu\n",
	       port_count, get32(store->engine + 12), engine_bytes);
	for (unsigned int pos = 0; pos < engine_bytes; pos += 16)
		printk(BIOS_INFO, "NATIVE108 ENGINE RAW offset=%03x %08x %08x %08x %08x\n",
		       pos, get32(store->engine + pos), get32(store->engine + pos + 4),
		       get32(store->engine + pos + 8), get32(store->engine + pos + 12));
	flush();
	if (CONFIG(B565D4_PCIE_FOUR_X4_PROBE)) {
		printk(BIOS_NOTICE, "NATIVE266 FOUR_X4 ALLOCATION PROBE requested=%zu returned=%u "
		       "STOP BEFORE HOST PORT MAPPING; NO PAYLOAD\n",
		       port_count, get32(store->engine + 12));
		flush();
		native84_terminal();
	}
	REQUIRE(store->engine[12] == port_count && !store->engine[13] &&
		!store->engine[14] && !store->engine[15], "engine count");
	for (unsigned int i = 0; i < port_count; i++) {
		const uint8_t *e = store->engine + 16 + 40 * i;
		const struct dxio_port *p = &store->ports[i];

		printk(BIOS_INFO, "NATIVE108 ENGINE %u lanes=%u..%u group=%u "
		       "devfnword=%08x control=%08x returned_port=%u tail=%08x\n",
		       i, e[2], e[3], e[5], get32(e + 12), get32(e + 16),
		       e[0x19], get32(e + 36));
		REQUIRE(e[0] == 1 && !e[1] && e[2] == p->first && e[3] == p->last &&
			e[4] == 1 && e[5] == p->group &&
			((get32(e + 16) >> 11) & 0x1f) == p->last - p->first + 1U,
			"engine topology/role/width");
	}
	flush();
}

static void mapping(void)
{
	static const uint8_t seed[10] = { 9, 10, 11, 17, 18, 19, 20, 21, 22, 23 };
	uint8_t map[10] = { 0 }, unused[10];

	REQUIRE(!mapped, "MAPPED repeated");
	engine_read();
	for (unsigned int i = 0; i < ARRAY_SIZE(map); i++)
		printk(BIOS_NOTICE, "NATIVE269 NBIF BEFORE slot=%u raw=%08x\n", i,
		       smn_read(0x13b100b8U + 4 * i));
	flush();
	memcpy(unused, seed, sizeof(unused));
	for (unsigned int i = 0; i < port_count; i++) {
		const uint8_t *e = store->engine + 16 + 40 * i;
		struct dxio_port *p = &store->ports[i];
		unsigned int ordinal = e[0x19], slot = p->map_slot;
		uint32_t returned = get32(e + 12);

		/* Vendor callback 09c6b348 matches returned byte19 against the source
		 * hardware PortId, independently of the descriptor's array index.
		 * Never reinterpret DMA byte27 as source-side policy27.
		 */
		REQUIRE(p->wrapper <= 1 && e[0x18] == 1U - p->wrapper &&
			ordinal == p->port && slot == p->port + (p->wrapper ? 3U : 0U) &&
			slot < 10 && !map[slot] &&
			(returned & 0xff) == p->devfn && !(returned & 0xffffff00U),
			"returned block/port/devfn mapping");
		REQUIRE((get32(e + 16) & 1) && get32(e + 36) == 1,
			"initial mapped engine state");
		p->allocated = 1;
		map[slot] = p->devfn;
		for (unsigned int j = 0; j < 10; j++)
			if (unused[j] == p->devfn)
				unused[j] = 0;
		printk(BIOS_INFO, "NATIVE108 MAP sidecar=%u wrapper=%u localport=%u slot=%u ordinal=%u devfn=%02x "
		       "policy27=%02x\n", i, p->wrapper, p->port, slot, ordinal, p->devfn, p->flags27);
	}
	for (unsigned int i = 0; i < 10; i++) {
		if (!map[i]) {
			unsigned int j;

			for (j = 0; j < 10 && !unused[j]; j++)
				;
			REQUIRE(j < 10, "unique map fill");
			map[i] = unused[j];
			unused[j] = 0;
		}
		smn_rmw(0x13b100b8U + 4 * i, 0xffffff00U, map[i]);
	}
	mapped = 1;
}

/* Original6683d forms ((wrapper_byte0a<<8)+(engine_byte1d>>4 &7))<<12.
 * The sidecar now matches observed SMU block18/Port19 for each topology;
 * mapping() validates that association before any per-port register write.
 */
static uint32_t offset(const struct dxio_port *p)
{
	REQUIRE(p->allocated && p->wrapper <= 1 && p->port <= 7, "port offset");
	return ((uint32_t)p->wrapper << 20) | ((uint32_t)p->port << 12);
}

static void before_reconfigure(void)
{
	REQUIRE(mapped, "reconfigure without mapping");
	for (unsigned int i = 0; i < port_count; i++) {
		const struct dxio_port *p = &store->ports[i];
		uint32_t o = offset(p), v = p->flags27;

		smn_rmw(0x111402d8U + o, 0xfffeffffU, (v & 4) << 14);
		smn_rmw(0x111402d8U + o, 0xfffff7ffU, (v & 8) << 8);
		smn_rmw(0x111402d8U + o, 0xfffffcffU, (v & 3) << 8);
		smn_rmw(0x11140390U + o, 0xffffdfffU, 1U << 13); /* PCD1b5=1 */
		smn_rmw(0x11140080U + o, 0xffefffffU, 1U << 20); /* PCD129=1 */
	}
	for (unsigned int wrapper = 0; wrapper < 2; wrapper++) {
		static const uint8_t defaults[4] = { 3, 3, 7, 7 };
		const uint8_t *eq = NULL;

		for (unsigned int i = 0; i < port_count; i++) {
			if (store->ports[i].wrapper != wrapper)
				continue;
			if (eq)
				REQUIRE(!memcmp(eq, store->ports[i].eq, 4), "wrapper EQ agreement");
			else
				eq = store->ports[i].eq;
		}
		REQUIRE(eq, "wrapper EQ source");

		for (unsigned int k = 0; k < 4; k++) {
			printk(BIOS_INFO, "NATIVE108 EQ wrapper=%u selector=%02x value=%u default=%u\n",
			       wrapper, 0xa1 + k, eq[k], defaults[k]);
			if (eq[k] != defaults[k]) {
				uint32_t a[6] = { 0x3a, 1, 4, wrapper, 0xa1 + k, eq[k] };

				request(a);
			}
		}
		/* Explicit Original PCD21c=0: no eight-port override writes. */
	}
	flush();
}

static void after_reconfigure(void)
{
	REQUIRE(mapped && !configured, "CONFIGURED order/repeated");
	for (unsigned int i = 0; i < port_count; i++) {
		uint32_t o = offset(&store->ports[i]);
		uint32_t value = smn_read(0x11100388U + o);

		/* Original66c5b cbf..d60: both record append AND original-value
		 * writeback are inside bit0/non-null-record/count<128 conditions.
		 * This trial always owns the record table; capacity refusal is strict.
		 */
		if (value & 1) {
			REQUIRE(store->record_count < 128, "CONFIGURED record capacity");
			store->record[store->record_count][0] = 0x11100388U + o;
			store->record[store->record_count++][1] = value;
			smn_write(0x11100388U + o, value); /* Original writeback; semantics opaque. */
		}
		smn_rmw(0x11140080U + o, 0xffff7fffU, 0);
	}
	for (unsigned int w = 0; w < 2; w++)
		/* PCD24c=0, PCD2fb=1. */
		smn_rmw(0x11180080U + (w << 20), 0x7fffffffU, 0x80000000U);
	configured = 1;
}

static uint32_t config_read(uint8_t bus, uint8_t devfn, uint16_t reg)
{
	uint32_t value = 0;

	printk(BIOS_INFO, "NATIVE108 PCI READ BEGIN %02x:%02x.%u reg=%02x\n",
	       bus, devfn >> 3, devfn & 7, reg);
	flush();
	REQUIRE(cfg(bus, devfn, reg, &value, 0), "PCI read/CF8 restore");
	native106_dxio_io("PCI READ", (bus << 20) | (devfn << 12) | reg, value, value);
	return value;
}

static uint32_t config_write(uint8_t bus, uint8_t devfn, uint16_t reg, uint32_t value)
{
	uint32_t actual;

	printk(BIOS_INFO, "NATIVE108 PCI WRITE BEGIN %02x:%02x.%u reg=%02x value=%08x\n",
	       bus, devfn >> 3, devfn & 7, reg, value);
	flush();
	REQUIRE(cfg(bus, devfn, reg, &value, 1), "PCI write/CF8 restore");
	actual = config_read(bus, devfn, reg);
	native106_dxio_io("PCI WRITE", (bus << 20) | (devfn << 12) | reg, value, actual);
	return actual;
}

static uint16_t pcie_status(uint8_t devfn)
{
	uint32_t cap = config_read(0, devfn, 0x34) & 0xff;

	for (unsigned int n = 0; n < 48 && cap; n++) {
		uint32_t header;

		REQUIRE(cap >= 0x40 && cap <= 0xec && !(cap & 3), "PCI capability chain");
		header = config_read(0, devfn, cap);
		if ((header & 0xff) == PCI_CAP_ID_PCIE)
			return config_read(0, devfn, cap + 0x10) >> 16;
		cap = (header >> 8) & 0xff;
	}
	refusal("missing PCIe capability", __LINE__);
}

static void early_trained(void)
{
	struct dxio_port *sb = &store->ports[port_count - 1];
	uint32_t old, temporary, actual, id;
	unsigned int route_count = 0;
	uint16_t status;
	uint32_t a[6];

	REQUIRE(mapped && !early && !routes_active && !sb_route_held &&
		!routes_restore_failed && sb->devfn == 0x11, "EARLYTRAINED order");
	static const uint8_t roots[] = { 9, 10, 11, 17, 18, 19, 20, 21, 22, 23 };
	for (unsigned int i = 0; i < ARRAY_SIZE(roots); i++) {
		uint32_t root_id = config_read(0, roots[i], PCI_VENDOR_ID);
		uint16_t link = (root_id == 0x16331022U || root_id == 0x16341022U) ?
			pcie_status(roots[i]) : 0;

		printk(BIOS_NOTICE, "NATIVE269 EARLY ROOT devfn=%02x id=%08x "
		       "link_status=%04x active=%u\n", roots[i], root_id, link,
		       !!(link & PCI_EXP_LNKSTA_DLLLA));
	}
	flush();
	status = pcie_status(sb->devfn);
	REQUIRE(status & PCI_EXP_LNKSTA_DLLLA, "SB early link not active");
	old = config_read(0, sb->devfn, PCI_PRIMARY_BUS);
	/* Fresh bus0 Type1 census includes disabled/internal sibling routes.
	 * Close every present sibling before borrowing bus2, and retain exact
	 * complete bus/latency DWORDs in owned CBMEM for every error path.
	 */
	memset(store->saved_route_valid, 0, sizeof(store->saved_route_valid));
	for (unsigned int devfn = 0; devfn < 256; devfn++) {
		uint32_t vendor = config_read(0, devfn, PCI_VENDOR_ID);

		if (!vendor || vendor == 0xffffffffU ||
		    ((config_read(0, devfn, 0x0c) >> 16) & 0x7f) != 1)
			continue;
		store->saved_routes[devfn] = config_read(0, devfn, PCI_PRIMARY_BUS);
		store->saved_route_valid[devfn] = 1;
		route_count++;
	}
	REQUIRE(route_count && store->saved_route_valid[sb->devfn] &&
		store->saved_routes[sb->devfn] == old && !(old & 0xff),
		"fresh Type1 census and SB primary/latency provenance");
	REQUIRE(saved_sibling_routes_exclude_bus2(), "saved sibling routes overlap borrowed bus2");
	routes_active = 1;
	for (unsigned int devfn = 0; devfn < 256; devfn++)
		if (store->saved_route_valid[devfn])
			config_write(0, devfn, PCI_PRIMARY_BUS,
				(store->saved_routes[devfn] & 0xff000000U) | 0x00feff00U);
	temporary = (old & 0xff0000ffU) | 0x00020200U;
	actual = config_write(0, sb->devfn, PCI_PRIMARY_BUS, temporary);
	REQUIRE(actual == temporary, "SB borrowed route readback");
	engine_read();
	id = config_read(2, 0, PCI_VENDOR_ID);
	REQUIRE(id != 0 && id != 0xffffffffU && (id & 0xffff) != 0x10b5,
		"unknown/absent/PLX early topology");
	REQUIRE(id == 0x43ef1022U, "expected Original-AM4 chipset identity");
	/* Restore all siblings now; retain only the verified SB bus2 route. */
	REQUIRE(cleanup_routes_scope(true), "sibling route/CF8 restore failed");
	sb_route_held = actual;
	printk(BIOS_INFO, "NATIVE213 SB ROUTE HELD devfn=%02x bus=%08x siblings_restored=%u\n",
	       sb->devfn, sb_route_held, route_count - 1);
	flush();
	/* b81b non-PLX branch writes ONLY current SB engine DWORD0c=200.
	 * All other firmware-returned bytes survive the complete engine reload.
	 */
	put32(store->engine + 16 + (port_count - 1) * 40 + 12, 0x200);
	a[0] = 0x23; a[1] = 0; a[2] = (uintptr_t)store->engine;
	a[3] = engine_bytes / 4; a[4] = 0; a[5] = DXIO_ENGINE_BUFFER;
	request(a);
	early = 1;
}

static void run_events(void)
{
	unsigned int phase = 0;

	while (events < DXIO_MAX_EVENTS && native106_dxio_budget_ok()) {
		uint32_t a[6] = { 9, 0, 0, 0, 0, 0 };
		uint8_t kind, state;

		request(a);
		events++;
		/* Exact full tags in the Original productive trace. Upper bits are
		 * not ignored: raw102=state, raw204=reset. Bare/unknown tags refuse.
		 */
		REQUIRE(a[1] == 0x102 || a[1] == 0x204, "exact full event kind tag");
		kind = a[1] & 0xff;
		REQUIRE(a[2] <= 0xff, "event state/level width");
		state = a[2];
		printk(BIOS_INFO, "NATIVE108 EVENT %u phase=%u kind=%02x state/level=%02x group=%08x\n",
		       events, phase, kind, state, a[3]);
		flush();
		if (kind == 4) {
			REQUIRE(a[2] <= 1 && a[3] <= 0xff &&
				native106_dxio_reset(a[3], a[2]), "reset callback");
		} else if (kind == 2) {
			switch (state) {
			case 0x0f:
				REQUIRE(phase == 0, "late MAPPED");
				mapping();
				before_reconfigure();
				break;
			case 5:
				REQUIRE(phase == 0, "late CONFIGURED");
				after_reconfigure();
				/* Vendor first dispatcher returns/cleans temporary bus,
				 * then outer EarlyInit issues308 before second dispatcher.
				 * Our temporary routes are already restored by callback.
				 */
				phase = 1;
				printk(BIOS_INFO, "NATIVE108 FIRST DISPATCH RETURN; OUTER RESUME\n");
				flush();
				break;
			case 7:
				early_trained();
				break;
			case 8:
				if (phase == 1 && !ack38_uart_attempted) {
					REQUIRE(retained_sb_route_owned(), "VETTING owned SB route");
					vetting_ack_sequence = requests;
					ack38_uart_pending = true;
				}
				break; /* VETTING diagnostic, not DONE. */
			case 0x0d:
				REQUIRE(mapped && configured && early && phase == 1,
					"DONE missing callbacks");
				REQUIRE(retained_sb_route_owned(), "DONE owned SB route");
				if (ack38_quiet.active) {
					if (!ack38_quiet.captured || ack38_quiet.done ||
					    ack38_quiet.outer != 1 ||
					    (ack38_quiet.raw[0] != 3 && ack38_quiet.raw[0] != 0x308))
						ack38_quiet_stop(ACK38_QUIET_DONE_WITHOUT_ACK, __LINE__);
					ack38_quiet.done = true;
					ack38_uart_after_wait(true, ack38_quiet.outer,
						ack38_quiet.grace_ticks);
				}
				REQUIRE(cleanup_routes(), "DONE SB route/CF8 restore failed");
				REQUIRE(!routes_active && !sb_route_held && !routes_restore_failed,
					"DONE routes pending at enumeration handoff");
				printk(BIOS_INFO, "NATIVE213 SB ROUTE RESTORED full_saved_DWORD=1 CF8_verified=1 pending=0\n");
				flush();
				return;
			default:
				refusal("unknown DXIO state", __LINE__);
			}
		} else {
			refusal("unknown DXIO event kind", __LINE__);
		}
		simple_request(0x308);
	}
	refusal("event/time budget exhausted", __LINE__);
}

void native106_dxio_train(void)
{
	struct cpuid_result cpu;
	void *allocation;
	uintptr_t aligned;
	uint64_t allocation_end;

	native84_stage_check();
	REQUIRE(!attempted++, "trainer reentry");
	cpu = cpuid_ext(0, 0);
	REQUIRE(cpu.ebx == 0x68747541U && cpu.edx == 0x69746e65U &&
		cpu.ecx == 0x444d4163U && cpuid_eax(0x80000000U) >= 0x80000001U,
		"AMD CPU identity");
	cpu = cpuid_ext(0x80000001U, 0);
	REQUIRE(b565d4_is_cezanne(cpu.eax) && (cpu.ebx >> 28) == 2, "Cezanne AM4");
	mhz = tsc_freq_mhz();
	REQUIRE(mhz && mhz <= 100000, "warm TSC frequency");
	first_ticks = last_ticks = rdtscll();
	total_ticks = (uint64_t)mhz * 60000000U;
	printk(BIOS_INFO, "NATIVE108 FULL CZN TRAINER BEGIN TSC_MHZ=%u "
	       "policy=BoardTopology+StrapTemplates boardExpanderABI pulse8=0 PCD2bdFresh0 "
	       "PCD129=1/1b5=1/21c=0/24c=0/2fb=1/14c=0 EQ3/3/7/7 policy27=0\n", mhz);
	flush();
	REQUIRE(!cbmem_find(DXIO_CBMEM_ID), "private CBMEM ID already exists");
	allocation = cbmem_add(DXIO_CBMEM_ID, DXIO_BYTES + 63);
	aligned = ((uintptr_t)allocation + 63U) & ~(uintptr_t)63U;
	allocation_end = (uint64_t)(uintptr_t)allocation + DXIO_BYTES + 63;
	REQUIRE(allocation && (uintptr_t)allocation >= NATIVE84_CBMEM_BEGIN &&
		allocation_end <= NATIVE84_PARENT_END &&
		(uint64_t)aligned + DXIO_BYTES <= allocation_end, "owned CBMEM arena");
	store = (void *)aligned;
	memset(store, 0, sizeof(*store));
	enum czn_pcie_topology topology = b565d4_pcie_topology();
	port_count = czn_pcie_ports(topology, store->ports, ARRAY_SIZE(store->ports));
	REQUIRE(port_count >= 3 && port_count <= CZN_MAX_PORTS, "board topology count");
	engine_bytes = czn_pcie_engine(store->engine, sizeof(store->engine),
		store->ports, port_count);
	REQUIRE(engine_bytes && engine_bytes <= sizeof(store->engine), "bounded engine construction");
	ancillary_bytes = czn_pcie_ancillary(store->ancillary, sizeof(store->ancillary),
		store->ports, port_count, initial_ancillary, sizeof(initial_ancillary),
		CONFIG(B565D4_PCIE_ACS) &&
		(!CONFIG(B565D4_BOARD_SETUP) || b565d4_setup_get(B565D4_OPT_ACS)));
	printk(BIOS_NOTICE, "NATIVE306 DXIO ACS policy=%u wrapper_features=7\n",
	       CONFIG(B565D4_PCIE_ACS) &&
	       (!CONFIG(B565D4_BOARD_SETUP) || b565d4_setup_get(B565D4_OPT_ACS)));
	REQUIRE(ancillary_bytes && ancillary_bytes <= sizeof(store->ancillary),
		"bounded topology strap construction");
	REQUIRE(get32(store->engine + 12) == port_count &&
		(topology != CZN_PCIE_X16 || czn_pcie_x16_ancillary_matches(store->ancillary,
		 ancillary_bytes, initial_ancillary, sizeof(initial_ancillary), CONFIG(B565D4_PCIE_ACS) &&
		 (!CONFIG(B565D4_BOARD_SETUP) || b565d4_setup_get(B565D4_OPT_ACS)))),
		"initial buffer readback");
	printk(BIOS_INFO, "NATIVE266 TOPOLOGY mode=%u ports=%zu gfx=%zu engine_bytes=%zu\n",
	       topology, port_count, port_count - 2, engine_bytes);
	printk(BIOS_INFO, "NATIVE108 OWN arena=%p..%08x engine=%p/%zu ancillary=%p/%zu "
	       "never_free_until_cold=1\n", allocation, (uint32_t)allocation_end,
	       store->engine, engine_bytes, store->ancillary, ancillary_bytes);
	flush();
	visibility("initial buffers");
	nbio_tom_setup();
	REQUIRE(native106_dxio_reset_preflight(), "board reset preflight");
	uint32_t skip = smn_read(0x03810a84U);
	REQUIRE(!(skip & (1U << 11)), "vendor EarlyInit skip bit set");
	smn_rmw(0x1013a008U, ~(1U << 7), 0);
	smn_rmw(0x10134e00U, ~(1U << 28), 0);
	native210_smu_get_version();
	for (unsigned int i = 0; i < ARRAY_SIZE(initial_requests); i++) {
		uint32_t a[6];

		memcpy(a, initial_requests[i], sizeof(a));
		if (i == 18) {
			a[2] = (uintptr_t)store->ancillary;
			a[3] = ancillary_bytes / 4;
		}
		if (i == 21) {
			a[2] = (uintptr_t)store->engine;
			a[3] = engine_bytes / 4;
			a[5] = DXIO_ENGINE_BUFFER;
		}
		request(a);
		if (i == 1)
			REQUIRE(a[1] == 0x301, "selected Original CZN firmware version");
	}
	run_events();
	engine_read(); /* Original post-DONE readback, never infer link from DONE alone. */
	REQUIRE(config_read(0, 17, 0) == 0x16341022U &&
		(pcie_status(17) & PCI_EXP_LNKSTA_DLLLA), "mandatory chipset root/link");
	for (unsigned int i = 0; i < port_count - 2; i++) {
		const struct dxio_port *port = &store->ports[i];
		uint32_t id = config_read(0, port->devfn, 0);
		uint32_t expected_id = port->wrapper ? 0x16341022U : 0x16331022U;
		uint16_t status = id == expected_id ? pcie_status(port->devfn) : 0;

		REQUIRE(id == expected_id || !id || id == 0xffffffffU, "slot root identity");
		printk(BIOS_NOTICE, "NATIVE274 SLOT ENGINE devfn=%02x lanes=%u..%u id=%08x "
		       "link_status=%04x active=%u optional_endpoint=1\n", port->devfn,
		       port->first, port->last, id, status, !!(status & PCI_EXP_LNKSTA_DLLLA));
	}
	printk(BIOS_INFO, "NATIVE108 DONE CHIPSET LINK ACTIVE requests=%u events=%u "
	       "record_count=%u; GENUINE PCI ENUMERATION NEXT\n", requests,
	       events, store->record_count);
	flush();
}
