#ifndef _BMKERNEL_H_
#define _BMKERNEL_H_

void test_bmkernel_init(void);
void test_bmkernel_exit(void);

void bmkernel_stress_init(void);
void sync_engines_status(u8 *free_engines);

#endif
