/* SPDX-License-Identifier: GPL-2.0-only */
#include <b565d4_cpu_ids.h>
#include <b565d4_cpu.h>
#include <commonlib/helpers.h>
#include <tests/test.h>

static void test_desktop_families(void **state)
{
	static const struct {
		uint32_t signature;
		enum b565d4_cpu_family family;
	} cases[] = {
		{ 0x00800f10, B565D4_CPU_SUMMIT_RIDGE },
		{ 0x00800f80, B565D4_CPU_PINNACLE_RIDGE },
		{ 0x00870f00, B565D4_CPU_MATISSE },
		{ 0x00870f10, B565D4_CPU_MATISSE },
		{ 0x00a20f00, B565D4_CPU_VERMEER },
		{ 0x00a20f10, B565D4_CPU_VERMEER },
		{ 0x00a50f00, B565D4_CPU_CEZANNE },
	};

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		for (uint32_t stepping = 0; stepping < 16; stepping++) {
			uint32_t signature = cases[i].signature | stepping;

			assert_int_equal(b565d4_cpu_family(signature), cases[i].family);
			assert_int_equal(b565d4_is_cezanne(signature),
					 cases[i].family == B565D4_CPU_CEZANNE);
		}
	}
}

static void test_excluded_and_unknown_models(void **state)
{
	static const uint32_t cases[] = {
		0, 0xffffffff, 0x000306c3, /* Invalid and Intel signatures. */
		0x00810f10, 0x00810f80,  /* Raven Ridge and Picasso APUs. */
		0x00860f01,             /* Renoir APU. */
		0x00830f10, 0x00a00f10, /* Server/Threadripper families. */
		0x00a60f12,             /* AM5 Raphael. */
		0x00870f20, 0x00a20f20, /* Unestablished neighbouring models. */
	};

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		assert_int_equal(b565d4_cpu_family(cases[i]), B565D4_CPU_UNKNOWN);
		assert_false(b565d4_is_cezanne(cases[i]));
	}
}

static void test_reserved_signature_bits(void **state)
{
	for (unsigned int bit = 28; bit < 32; bit++) {
		uint32_t signature = B565D4_CPUID_MATISSE_B0 | (1U << bit);

		assert_int_equal(b565d4_cpu_family(signature), B565D4_CPU_UNKNOWN);
		assert_false(b565d4_is_cezanne(signature));
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_desktop_families),
		cmocka_unit_test(test_excluded_and_unknown_models),
		cmocka_unit_test(test_reserved_signature_bits),
	};

	return cb_run_group_tests(tests, NULL, NULL);
}
