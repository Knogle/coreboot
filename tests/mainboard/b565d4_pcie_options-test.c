/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <commonlib/helpers.h>
#include <tests/test.h>
#include "board_config.h"

static void test_pcie_stored_choices(void **state)
{
	const enum czn_pcie_topology defaults[] = {
		CZN_PCIE_X16, CZN_PCIE_X8X4X4, CZN_PCIE_FOUR_X4,
	};

	for (size_t i = 0; i < ARRAY_SIZE(defaults); i++) {
		assert_int_equal(b565d4_pcie_topology_from_option(B565D4_PCIE_SLOT_DEFAULT,
			defaults[i]), defaults[i]);
		assert_int_equal(b565d4_pcie_topology_from_option(B565D4_PCIE_SLOT_X16,
			defaults[i]), CZN_PCIE_X16);
		assert_int_equal(b565d4_pcie_topology_from_option(B565D4_PCIE_SLOT_X8X4X4,
			defaults[i]), CONFIG(B565D4_DESKTOP_NATIVE) ? defaults[i] : CZN_PCIE_X8X4X4);
		assert_int_equal(b565d4_pcie_topology_from_option(B565D4_PCIE_SLOT_FOUR_X4,
			defaults[i]), CZN_PCIE_FOUR_X4);
	}
}

static void test_pcie_invalid_values_use_default(void **state)
{
	const enum czn_pcie_topology defaults[] = {
		CZN_PCIE_X16, CZN_PCIE_X8X4X4, CZN_PCIE_FOUR_X4,
	};

	for (size_t i = 0; i < ARRAY_SIZE(defaults); i++) {
		for (unsigned int value = 4; value <= 255; value++)
			assert_int_equal(b565d4_pcie_topology_from_option(value, defaults[i]),
				defaults[i]);
		assert_int_equal(b565d4_pcie_topology_from_option(UINT_MAX, defaults[i]),
			defaults[i]);
	}
}

static void test_pcie_override_cannot_select_allocation_probe(void **state)
{
	for (unsigned int value = 0; value <= 255; value++)
		assert_int_not_equal(b565d4_pcie_topology_from_option(value, CZN_PCIE_FOUR_X4),
			CZN_PCIE_FOUR_X4_PROBE);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_pcie_stored_choices),
		cmocka_unit_test(test_pcie_invalid_values_use_default),
		cmocka_unit_test(test_pcie_override_cannot_select_allocation_probe),
	};

	return cb_run_group_tests(tests, NULL, NULL);
}
