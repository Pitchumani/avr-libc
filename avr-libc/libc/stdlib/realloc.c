/*
 * Copyright (c) 2004 Joerg Wunsch
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#include <stdlib.h>
#include <string.h>

#include "stdlib_private.h"

#ifndef MALLOC_TEST
#include <avr/io.h>
#endif

void *
realloc(void *ptr, size_t len)
{
	struct __freelist *fp1, *fp2, *fp3, *ofp3;
	char *cp, *cp1;
	void *memp;
	size_t s, incr;

	/* Trivial case, required by C standard. */
	if (ptr == 0)
		return malloc(len);

	cp = (char *)ptr;
	cp -= sizeof(size_t);
	fp1 = (struct __freelist *)cp;

	cp = (char *)ptr + len; /* new next pointer */
	fp2 = (struct __freelist *)cp;

	/*
	 * See whether we are growing or shrinking.  When shrinking,
	 * we split off a chunk for the released portion, and call
	 * free() on it.  Therefore, we can only shrink if the new
	 * size is at least sizeof(struct __freelist) smaller than the
	 * previous size.
	 */
	if (len <= fp1->sz) {
		/* The first test catches a possible unsigned int
		 * rollover condition. */
		if (fp1->sz <= sizeof(struct __freelist) ||
		    len > fp1->sz - sizeof(struct __freelist))
			return ptr;
		fp2->sz = fp1->sz - len - sizeof(size_t);
		fp2->nx = fp1->nx;
		fp1->sz = len;
		fp1->nx = fp2;
		free(&(fp2->nx));
		return ptr;
	}

	/*
	 * If we get here, we are growing.  First, see whether there
	 * is space in the free list on top of our current chunk.
	 */
	incr = len - fp1->sz - sizeof(size_t);
	cp = (char *)ptr + fp1->sz;
	fp2 = (struct __freelist *)cp;
	for (s = 0, ofp3 = 0, fp3 = __flp;
	     fp3;
	     ofp3 = fp3, fp3 = fp3->nx) {
		if (fp3 == fp2 && fp3->sz >= incr) {
			/* found something that fits */
			if (incr <= fp3->sz &&
			    incr > fp3->sz - sizeof(struct __freelist)) {
				/* it just fits, so use it entirely */
				fp1->sz += fp3->sz + sizeof(size_t);
				if (ofp3)
					ofp3->nx = fp3->nx;
				else
					__flp = fp3->nx;
				return ptr;
			}
			/* split off a new freelist entry */
			cp = (char *)ptr + len;
			fp2 = (struct __freelist *)cp;
			fp2->nx = fp3->nx;
			fp2->sz = fp3->sz - incr - sizeof(size_t);
			if (ofp3)
				ofp3->nx = fp2;
			else
				__flp = fp2;
			fp1->sz = len;
			return ptr;
		}
		/*
		 * Find the largest chunk on the freelist while
		 * walking it.
		 */
		if (fp3->sz > s)
			s = fp3->sz;
	}
	/*
	 * If we are the topmost chunk in memory, and there was no
	 * large enough chunk on the freelist that could be re-used
	 * (by a call to malloc() below), quickly extend the
	 * allocation area if possible, without need to copy the old
	 * data.
	 */
	if (__brkval == (char *)ptr + fp1->sz && len > s) {
		cp1 = __malloc_heap_end;
		if (cp1 == 0)
			cp1 = STACK_POINTER() - __malloc_margin;
		if (cp < cp1) {
			__brkval = cp;
			fp1->sz = len;
			return ptr;
		}
		/* If that failed, we are out of luck. */
		return 0;
	}

	/*
	 * Call malloc() for a new chunk, then copy over the data, and
	 * release the old region.
	 */
	if ((memp = malloc(len)) == 0)
		return 0;
	memcpy(memp, ptr, fp1->sz);
	free(ptr);
	return memp;
}

