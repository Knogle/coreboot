/* SPDX-License-Identifier: GPL-2.0-only */
#include <b565d4_cpu_topology.h>
#include <b565d4_cpu.h>
#include <string.h>
#include <tests/test.h>

static void test_cezanne_steppings(void **state)
{
	for (unsigned int stepping = 0; stepping < 16; stepping++)
		assert_true(b565d4_is_cezanne(0x00a50f00 | stepping));
	assert_false(b565d4_is_cezanne(0x00a20f10)); /* Vermeer needs its own SoC path. */
	assert_false(b565d4_is_cezanne(0x00860f01)); /* Renoir. */
	assert_false(b565d4_is_cezanne(0x00870f10)); /* Matisse. */
}

static void make_topology(uint8_t *record, const uint8_t *cores,
			  unsigned int count, bool smt)
{
	memset(record, 0, B565D4_CEZANNE_TOPOLOGY_BYTES);
	record[0x31 + 25] = 0xff;
	for (unsigned int i = 0; i < B565D4_CEZANNE_MAX_CORES; i++) {
		uint8_t *entry = record + 0x32 + i * 3;

		entry[0] = i < count ? cores[i] : 0xff;
		entry[1] = i < count;
		entry[2] = i < count && smt;
		entry[25] = 0xff;
	}
}

static void test_fused_six_core(void **state)
{
	const uint8_t cores[] = { 0, 1, 2, 3, 4, 6 };
	uint8_t record[B565D4_CEZANNE_TOPOLOGY_BYTES];
	struct b565d4_cpu_topology topology;

	make_topology(record, cores, 6, true);
	assert_true(b565d4_decode_cezanne_topology(record, sizeof(record), 12, &topology));
	assert_int_equal(topology.launch_mask, 0xfff);
	assert_int_equal(topology.logical_threads, 12);
	assert_int_equal(topology.threads_per_core, 2);
	assert_memory_equal(topology.physical_core, cores, sizeof(cores));
}

static void test_four_and_eight_cores(void **state)
{
	const uint8_t cores[] = { 0, 2, 4, 7, 1, 3, 5, 6 };
	uint8_t record[B565D4_CEZANNE_TOPOLOGY_BYTES];
	struct b565d4_cpu_topology topology;

	make_topology(record, cores, 4, true);
	assert_true(b565d4_decode_cezanne_topology(record, sizeof(record), 8, &topology));
	assert_int_equal(topology.launch_mask, 0xff);
	make_topology(record, cores, 8, true);
	assert_true(b565d4_decode_cezanne_topology(record, sizeof(record), 16, &topology));
	assert_int_equal(topology.launch_mask, 0xffff);
}

static void test_smt_disabled_and_one_thread(void **state)
{
	const uint8_t cores[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	uint8_t record[B565D4_CEZANNE_TOPOLOGY_BYTES];
	struct b565d4_cpu_topology topology;

	make_topology(record, cores, 8, false);
	assert_true(b565d4_decode_cezanne_topology(record, sizeof(record), 8, &topology));
	assert_int_equal(topology.launch_mask, 0xff);
	assert_int_equal(topology.threads_per_core, 1);
	make_topology(record, cores, 1, false);
	assert_true(b565d4_decode_cezanne_topology(record, sizeof(record), 1, &topology));
	assert_int_equal(topology.launch_mask, 1);
}

static void test_fused_six_core_smt_disabled(void **state)
{
	const uint8_t cores[] = { 0, 1, 2, 3, 4, 6 };
	uint8_t record[B565D4_CEZANNE_TOPOLOGY_BYTES];
	struct b565d4_cpu_topology topology;

	make_topology(record, cores, 6, false);
	assert_true(b565d4_decode_cezanne_topology(record, sizeof(record), 6, &topology));
	assert_int_equal(topology.launch_mask, 0x3f);
	assert_int_equal(topology.threads_per_core, 1);
	assert_memory_equal(topology.physical_core, cores, sizeof(cores));
	/* A second thread with a one-thread stride would alias another core. */
	record[0x32 + 3 + 2] = 1;
	assert_false(b565d4_decode_cezanne_topology(record, sizeof(record), 7, &topology));
}

static void test_count_and_extent_mismatch(void **state)
{
	const uint8_t cores[] = { 0, 1, 2, 3 };
	uint8_t record[B565D4_CEZANNE_TOPOLOGY_BYTES];
	struct b565d4_cpu_topology topology;

	make_topology(record, cores, 4, true);
	assert_false(b565d4_decode_cezanne_topology(record, sizeof(record), 12, &topology));
	assert_false(b565d4_decode_cezanne_topology(record, sizeof(record), 0, &topology));
	assert_false(b565d4_decode_cezanne_topology(record, sizeof(record), 17, &topology));
	for (size_t size = 0; size < sizeof(record); size++)
		assert_false(b565d4_decode_cezanne_topology(record, size, 8, &topology));
	assert_false(b565d4_decode_cezanne_topology(NULL, sizeof(record), 8, &topology));
	assert_false(b565d4_decode_cezanne_topology(record, sizeof(record), 8, NULL));
}

static void test_malformed_topology(void **state)
{
	const uint8_t cores[] = { 0, 1, 2, 3 };
	uint8_t record[B565D4_CEZANNE_TOPOLOGY_BYTES];
	struct b565d4_cpu_topology topology, unchanged;
	static const struct {
		size_t offset;
		uint8_t value;
	} invalid[] = {
		{ 0x31, 1 },         /* Wrong complex. */
		{ 0x31 + 25, 1 },    /* Overlapping second complex. */
		{ 0x32 + 3, 0 },     /* Duplicate physical core. */
		{ 0x32, 8 },         /* Invalid physical core. */
		{ 0x32 + 1, 2 },     /* Invalid thread flag. */
		{ 0x32 + 1, 0 },     /* Thread 1 without thread 0. */
		{ 0x32 + 12 + 1, 1 }, /* Thread on an absent core. */
		{ 0x32 + 25 + 1, 1 }, /* Thread on an absent complex. */
	};

	memset(&unchanged, 0xa5, sizeof(unchanged));
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		make_topology(record, cores, 4, true);
		record[invalid[i].offset] = invalid[i].value;
		topology = unchanged;
		assert_false(b565d4_decode_cezanne_topology(record, sizeof(record), 8,
						      &topology));
		assert_memory_equal(&topology, &unchanged, sizeof(topology));
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_cezanne_steppings),
		cmocka_unit_test(test_fused_six_core),
		cmocka_unit_test(test_four_and_eight_cores),
		cmocka_unit_test(test_smt_disabled_and_one_thread),
		cmocka_unit_test(test_fused_six_core_smt_disabled),
		cmocka_unit_test(test_count_and_extent_mismatch),
		cmocka_unit_test(test_malformed_topology),
	};

	return cb_run_group_tests(tests, NULL, NULL);
}
