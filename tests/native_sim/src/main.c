/* SPDX-License-Identifier: MIT */
#include <zephyr/ztest.h>

int dj_core_test_run(void);

ZTEST(dirtyjtag_native_sim, test_universal_core_scenarios)
{
    zassert_equal(dj_core_test_run(), 0,
                  "DirtyJTAG universal core scenario set failed");
}

ZTEST_SUITE(dirtyjtag_native_sim, NULL, NULL, NULL, NULL, NULL);
