#ifndef _CACHE_H
#define _CACHE_H

extern void invalidate_dcache_range(unsigned long start, unsigned long size);
extern void flush_dcache_range(unsigned long start, unsigned long size);
extern void invalidate_icache_all(void);
extern void invalidate_dcache_all(void);

#endif
