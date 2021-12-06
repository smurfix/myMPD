/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_MEM_H
#define MYMPD_MEM_H

#include <assert.h>
#include <stdlib.h>

static inline void *malloc_assert(size_t size) {
    void *p = malloc(size);
    assert(p);
    return p;
}

static inline void *realloc_assert(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    assert(p);
    return p;
}

#define FREE_PTR(PTR) do { \
    if (PTR != NULL) \
        free(PTR); \
    PTR = NULL; \
} while (0)

#endif
