/*
 * Basic general purpose allocator for managing special purpose
 * memory, for example, memory that is not managed by the regular
 * kmalloc/kfree interface.  Uses for this includes on-device special
 * memory, uncached memory etc.
 *
 * It is safe to use the allocator in NMI handlers and other special
 * unblockable contexts that could otherwise deadlock on locks.  This
 * is implemented by using atomic operations and retries on any
 * conflicts.  The disadvantage is that there may be livelocks in
 * extreme cases.  For better scalability, one allocator can be used
 * for each CPU.
 *
 * The lockless operation only works if there is enough memory
 * available.  If new memory is added to the pool a lock has to be
 * still taken.  So any user relying on locklessness has to ensure
 * that sufficient memory is preallocated.
 *
 * The basic atomic operation of this allocator is cmpxchg on long.
 * On architectures that don't have NMI-safe cmpxchg implementation,
 * the allocator can NOT be used in NMI handler.  So code uses the
 * allocator in NMI handler should depend on
 * CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG.
 *
 * Copyright 2005 (C) Jes Sorensen <jes@trained-monkey.org>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/bitmap.h>
#include <linux/rculist.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include "bm_genalloc.h"
static inline size_t chunk_size(const struct bm_gen_pool_chunk *chunk)
{
	return chunk->end_addr - chunk->start_addr + 1;
}

static int set_bits_ll(unsigned long *addr, unsigned long mask_to_set)
{
	unsigned long val, nval;

	nval = *addr;
	do
	{
		val = nval;
		if (val & mask_to_set)
			return -EBUSY;
		cpu_relax();
	} while ((nval = cmpxchg(addr, val, val | mask_to_set)) != val);

	return 0;
}

static int clear_bits_ll(unsigned long *addr, unsigned long mask_to_clear)
{
	unsigned long val, nval;

	nval = *addr;
	do
	{
		val = nval;
		if ((val & mask_to_clear) != mask_to_clear)
			return -EBUSY;
		cpu_relax();
	} while ((nval = cmpxchg(addr, val, val & ~mask_to_clear)) != val);

	return 0;
}

/*
 * bitmap_set_ll - set the specified number of bits at the specified position
 * @map: pointer to a bitmap
 * @start: a bit position in @map
 * @nr: number of bits to set
 *
 * Set @nr bits start from @start in @map lock-lessly. Several users
 * can set/clear the same bitmap simultaneously without lock. If two
 * users set the same bit, one user will return remain bits, otherwise
 * return 0.
 */
static int bitmap_set_ll(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_set >= 0)
	{
		if (set_bits_ll(p, mask_to_set))
			return nr;
		nr -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		p++;
	}
	if (nr)
	{
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		if (set_bits_ll(p, mask_to_set))
			return nr;
	}

	return 0;
}

/*
 * bitmap_clear_ll - clear the specified number of bits at the specified position
 * @map: pointer to a bitmap
 * @start: a bit position in @map
 * @nr: number of bits to set
 *
 * Clear @nr bits start from @start in @map lock-lessly. Several users
 * can set/clear the same bitmap simultaneously without lock. If two
 * users clear the same bit, one user will return remain bits,
 * otherwise return 0.
 */
static int bitmap_clear_ll(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_clear >= 0)
	{
		if (clear_bits_ll(p, mask_to_clear))
			return nr;
		nr -= bits_to_clear;
		bits_to_clear = BITS_PER_LONG;
		mask_to_clear = ~0UL;
		p++;
	}
	if (nr)
	{
		mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
		if (clear_bits_ll(p, mask_to_clear))
			return nr;
	}

	return 0;
}
/**
 * bm_gen_pool_create - create a new special memory pool
 * @min_alloc_order: log base 2 of number of bytes each bitmap bit represents
 * @nid: node id of the node the pool structure should be allocated on, or -1
 *
 * Create a new special memory pool that can be used to manage special purpose
 * memory not managed by the regular kmalloc/kfree interface.
 */
struct bm_gen_pool *bm_gen_pool_create(int min_alloc_order, int nid)
{
	struct bm_gen_pool *pool;

	pool = kmalloc_node(sizeof(struct bm_gen_pool), GFP_KERNEL, nid);
	if (pool != NULL)
	{
		spin_lock_init(&pool->lock);
		INIT_LIST_HEAD(&pool->chunks);
		pool->min_alloc_order = min_alloc_order;
		pool->algo = bm_gen_pool_first_fit;
		pool->data = NULL;
		pool->name = NULL;
	}
	return pool;
}

/**
 * bm_gen_pool_add_virt - add a new chunk of special memory to the pool
 * @pool: pool to add new memory chunk to
 * @virt: virtual starting address of memory chunk to add to pool
 * @phys: physical starting address of memory chunk to add to pool
 * @size: size in bytes of the memory chunk to add to pool
 * @nid: node id of the node the chunk structure and bitmap should be
 *       allocated on, or -1
 *
 * Add a new chunk of special memory to the specified pool.
 *
 * Returns 0 on success or a -ve errno on failure.
 */
int bm_gen_pool_add_virt(struct bm_gen_pool *pool, unsigned long virt, phys_addr_t phys,
												 size_t size, int nid)
{
	struct bm_gen_pool_chunk *chunk;
	int nbits = size >> pool->min_alloc_order;
	int nbytes = sizeof(struct bm_gen_pool_chunk) +
							 BITS_TO_LONGS(nbits) * sizeof(long);

	chunk = vzalloc_node(nbytes, nid);
	if (unlikely(chunk == NULL))
		return -ENOMEM;

	chunk->phys_addr = phys;
	chunk->start_addr = virt;
	chunk->end_addr = virt + size - 1;
	atomic_long_set(&chunk->avail, size);

	spin_lock(&pool->lock);
	list_add_rcu(&chunk->next_chunk, &pool->chunks);
	spin_unlock(&pool->lock);

	return 0;
}


/**
 * bm_gen_pool_destroy - destroy a special memory pool
 * @pool: pool to destroy
 *
 * Destroy the specified special memory pool. Verifies that there are no
 * outstanding allocations.
 */
void bm_gen_pool_destroy(struct bm_gen_pool *pool)
{
	struct list_head *_chunk, *_next_chunk;
	struct bm_gen_pool_chunk *chunk;
	int order = pool->min_alloc_order;
	int bit, end_bit;

	list_for_each_safe(_chunk, _next_chunk, &pool->chunks)
	{
		chunk = list_entry(_chunk, struct bm_gen_pool_chunk, next_chunk);
		list_del(&chunk->next_chunk);

		end_bit = chunk_size(chunk) >> order;
		bit = find_next_bit(chunk->bits, end_bit, 0);
		// BUG_ON(bit < end_bit);
		if (unlikely((bit < end_bit) != 0))
		{
			WARN_ON(1);
			return;
		}

		vfree(chunk);
	}
	kfree_const(pool->name);
	kfree(pool);
}

/**
 * bm_gen_pool_alloc - allocate special memory from the pool
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
unsigned long bm_gen_pool_alloc(struct bm_gen_pool *pool, size_t size)
{
	return bm_gen_pool_alloc_algo(pool, size, pool->algo, pool->data);
}

/**
 * bm_gen_pool_alloc_algo - allocate special memory from the pool
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 * @algo: algorithm passed from caller
 * @data: data passed to algorithm
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
unsigned long bm_gen_pool_alloc_algo(struct bm_gen_pool *pool, size_t size,
																		 bm_genpool_algo_t algo, void *data)
{
	struct bm_gen_pool_chunk *chunk;
	unsigned long addr = 0;
	int order = pool->min_alloc_order;
	int nbits, start_bit, end_bit, remain;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	if (unlikely((in_nmi()) != 0))
	{
		WARN_ON(1);
		return 0;
	}
#endif

	if (size == 0)
		return 0;

	nbits = (size + (1UL << order) - 1) >> order;
	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk)
	{
		if (size > atomic_long_read(&chunk->avail))
			continue;

		start_bit = 0;
		end_bit = chunk_size(chunk) >> order;
	retry:
		start_bit = algo(chunk->bits, end_bit, start_bit,
										 nbits, data, pool, chunk->start_addr);
		if (start_bit >= end_bit)
			continue;
		remain = bitmap_set_ll(chunk->bits, start_bit, nbits);
		if (remain)
		{
			remain = bitmap_clear_ll(chunk->bits, start_bit,
															 nbits - remain);
			// BUG_ON(remain);
			if (unlikely((remain) != 0))
			{
				WARN_ON(1);
				return 0;
			}
			goto retry;
		}

		addr = chunk->start_addr + ((unsigned long)start_bit << order);
		size = (size_t)nbits << order;
		atomic_long_sub(size, &chunk->avail);
		break;
	}
	rcu_read_unlock();
	return addr;
}

/**
 * bm_gen_pool_free - free allocated special memory back to the pool
 * @pool: pool to free to
 * @addr: starting address of memory to free back to pool
 * @size: size in bytes of memory to free
 *
 * Free previously allocated special memory back to the specified
 * pool.  Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
void bm_gen_pool_free(struct bm_gen_pool *pool, unsigned long addr, size_t size)
{
	struct bm_gen_pool_chunk *chunk;
	int order = pool->min_alloc_order;
	int start_bit, nbits, remain;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	// BUG_ON(in_nmi());
	if (unlikely((in_nmi()) != 0))
	{
		WARN_ON(1);
		return;
	}
#endif

	nbits = (size + (1UL << order) - 1) >> order;
	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk)
	{
		if (addr >= chunk->start_addr && addr <= chunk->end_addr)
		{
			// BUG_ON(addr + size - 1 > chunk->end_addr);
			if (unlikely((addr + size - 1 > chunk->end_addr) != 0))
			{
				WARN_ON(1);
				return;
			}
			start_bit = (addr - chunk->start_addr) >> order;
			remain = bitmap_clear_ll(chunk->bits, start_bit, nbits);
			// BUG_ON(remain);
			if (unlikely((remain) != 0))
			{
				WARN_ON(1);
				return;
			}
			size = (size_t)nbits << order;
			atomic_long_add(size, &chunk->avail);
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
	// BUG();
	WARN_ON(1);
}


/**
 * bm_gen_pool_avail - get available free space of the pool
 * @pool: pool to get available free space
 *
 * Return available free space of the specified pool.
 */
size_t bm_gen_pool_avail(struct bm_gen_pool *pool)
{
	struct bm_gen_pool_chunk *chunk;
	size_t avail = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk)
			avail += atomic_long_read(&chunk->avail);
	rcu_read_unlock();
	return avail;
}

/**
 * bm_gen_pool_size - get size in bytes of memory managed by the pool
 * @pool: pool to get size
 *
 * Return size in bytes of memory managed by the pool.
 */
size_t bm_gen_pool_size(struct bm_gen_pool *pool)
{
	struct bm_gen_pool_chunk *chunk;
	size_t size = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk)
			size += chunk_size(chunk);
	rcu_read_unlock();
	return size;
}



/**
 * bm_gen_pool_first_fit - find the first available region
 * of memory matching the size requirement (no alignment constraint)
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: additional data - unused
 * @pool: pool to find the fit region memory from
 */
unsigned long bm_gen_pool_first_fit(unsigned long *map, unsigned long size,
																		unsigned long start, unsigned int nr, void *data,
																		struct bm_gen_pool *pool, unsigned long start_addr)
{
	return bitmap_find_next_zero_area(map, size, start, nr, 0);
}
