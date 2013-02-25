/*
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"

#include <cpuid.h>

unsigned sna_cpu_detect(void)
{
	unsigned int eax, ebx, ecx, edx;
	unsigned features = 0;

	__cpuid(0, eax, ebx, ecx, edx);

	if (eax & bit_SSE3)
		features |= SSE3;

	if (eax & bit_SSSE3)
		features |= SSSE3;

	if (eax & bit_SSE4_1)
		features |= SSE4_1;

	if (eax & bit_SSE4_2)
		features |= SSE4_2;

	if (eax & bit_AVX)
		features |= AVX;

	if (edx & bit_MMX)
		features |= MMX;

	if (edx & bit_SSE)
		features |= SSE;

	if (edx & bit_SSE2)
		features |= SSE2;

	if (edx & bit_SSE4a)
		features |= SSE4a;

	__cpuid(7, eax, ebx, ecx, edx);

	if (eax & bit_AVX2)
		features |= AVX2;

	return features;
}
