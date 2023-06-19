#include "boot_test.h"
#include "cache.h"
#include "enc_test.h"
#include "testcase_wave_enc.h"

extern void invalidate_dcache_all(void);

void inv_dcache_range(unsigned long start, unsigned long size)
{
	invalidate_dcache_range(start, size);
}

#ifndef BUILD_TEST_CASE_all
int testcase_wave_enc(void)
{
	int ret = 0;

	uartlog("%s starts\n", __func__);

    invalidate_dcache_all();

	ret = TestEncoder();
	uartlog("%s ends %s\n", __func__, ret?"passed":"failed");

	return ret;
}

module_testcase_ext("0", testcase_wave_enc, "wave_enc");
#endif
