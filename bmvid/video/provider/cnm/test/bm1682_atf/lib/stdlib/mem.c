/*
 * Copyright (c) 2013-2014, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h> /* size_t */
#include <stdio.h>
/*
 * Fill @count bytes of memory pointed to by @dst with @val
 */
void *memset(void *dst, int val, size_t count)
{
    if ((unsigned long)dst%sizeof(long) == 0 && \
        (count%sizeof(long) == 0) && \
        (val == 0))
    {
        unsigned long *ptr = dst;

        while ( count )
        {
            *ptr++ = 0;
            count -= sizeof(long);
        }
    }
    else
    {
        char *ptr = dst;

        while (count--)
            *ptr++ = val;
    }

    return dst;
}

/*
 * Compare @len bytes of @s1 and @s2
 */
int memcmp(const void *s1, const void *s2, size_t len)
{
    if ((unsigned long)s1%sizeof(long) == 0 && \
        (unsigned long)s2%sizeof(long) == 0 && \
        (len%sizeof(long) == 0))
    {
        const unsigned long *s = s1;
        const unsigned long *d = s2;
        unsigned long sc;
        unsigned long dc;

        while (len) {
            sc = *s++;
            dc = *d++;
            if (sc - dc)
            {
                printf("[s1 0x%016lx s2 0x%016lx offset 0x%08lx]s1: 0x%016lx  s2: 0x%016lx\n", (unsigned long)s1, (unsigned long)s2, (s-1)-(unsigned long *)s1, sc, dc);
                return -1;
            }
            len -= sizeof(unsigned long);
        }
    }
    else
    {
        const unsigned char *s = s1;
        const unsigned char *d = s2;
        unsigned char sc;
        unsigned char dc;

        while (len--) {
            sc = *s++;
            dc = *d++;
            if (sc - dc)
                return (sc - dc);
        }
    }

    return 0;
}

/*
 * Copy @len bytes from @src to @dst
 */
void *memcpy(void *dst, const void *src, size_t len)
{

    if ((unsigned long)dst%sizeof(long) == 0 && \
        (unsigned long)src%sizeof(long) == 0 && \
        (len%sizeof(long) == 0))
    {
        long *d = dst;
        const long *s = src;

        while (len)
        {
            *d++ = *s++;
            len -= sizeof(long);
        }

    }
    else
    {
        const char *s = src;
        char *d = dst;

        while (len--)
            *d++ = *s++;
    }

    return dst;
}

/*
 * Move @len bytes from @src to @dst
 */
void *memmove(void *dst, const void *src, size_t len)
{
    /*
     * The following test makes use of unsigned arithmetic overflow to
     * more efficiently test the condition !(src <= dst && dst < str+len).
     * It also avoids the situation where the more explicit test would give
     * incorrect results were the calculation str+len to overflow (though
     * that issue is probably moot as such usage is probably undefined
     * behaviour and a bug anyway.
     */
    if ((size_t)dst - (size_t)src >= len) {
        /* destination not in source data, so can safely use memcpy */
        return memcpy(dst, src, len);
    } else {
        /* copy backwards... */
        const char *end = dst;
        const char *s = (const char *)src + len;
        char *d = (char *)dst + len;
        while (d != end)
            *--d = *--s;
    }
    return dst;
}

/*
 * Scan @len bytes of @src for value @c
 */
void *memchr(const void *src, int c, size_t len)
{
    const char *s = src;

    while (len--) {
        if (*s == c)
            return (void *) s;
        s++;
    }

    return NULL;
}
