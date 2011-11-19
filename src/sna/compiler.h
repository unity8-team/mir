/*
 * Copyright (c) 2011 Intel Corporation
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

#ifndef _SNA_COMPILER_H_
#define _SNA_COMPILER_H_

#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#define likely(expr) (__builtin_expect (!!(expr), 1))
#define unlikely(expr) (__builtin_expect (!!(expr), 0))
#define noinline __attribute__((noinline))
#define fastcall __attribute__((regparm(3)))
#else
#define likely(expr) (expr)
#define unlikely(expr) (expr)
#define noinline
#define fastcall
#endif

#ifdef HAVE_VALGRIND_H
#define VG(x) x
#else
#define VG(x)
#endif

#define VG_CLEAR(s) VG(memset(&s, 0, sizeof(s)))

#endif /* _SNA_COMPILER_H_ */
