/*
 * Copyright © 2013 Intel Corporation
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

#include "sna.h"

#include <unistd.h>
#include <pthread.h>

static int max_threads = -1;

static struct thread {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    void (*func)(void *arg);
    void *arg;
} *threads;

static void *__run__(void *arg)
{
	struct thread *t = arg;

	pthread_mutex_lock(&t->mutex);
	while (1) {
		while (t->func == NULL)
			pthread_cond_wait(&t->cond, &t->mutex);
		pthread_mutex_unlock(&t->mutex);

		assert(t->func);
		t->func(t->arg);

		pthread_mutex_lock(&t->mutex);
		t->func = NULL;
		pthread_cond_signal(&t->cond);
	}
	pthread_mutex_unlock(&t->mutex);

	return NULL;
}

void sna_threads_init(void)
{
	int n;

	if (max_threads != -1)
		return;

	max_threads = sysconf (_SC_NPROCESSORS_ONLN) / 2;
	if (max_threads <= 1)
		goto bail;

	DBG(("%s: creating a thread pool of %d threads\n",
	     __func__, max_threads));

	threads = malloc (sizeof(threads[0])*max_threads);
	if (threads == NULL)
		goto bail;

	for (n = 0; n < max_threads; n++) {
		pthread_mutex_init(&threads[n].mutex, NULL);
		pthread_cond_init(&threads[n].cond, NULL);

		threads[n].func = NULL;
		if (pthread_create(&threads[n].thread, NULL,
				   __run__, &threads[n]))
			goto bail;
	}

	return;

bail:
	max_threads = 0;
}

void sna_threads_run(void (*func)(void *arg), void *arg)
{
	int n;

	assert(max_threads > 0);

	for (n = 0; n < max_threads; n++) {
		if (threads[n].func)
			continue;

		pthread_mutex_lock(&threads[n].mutex);
		if (threads[n].func) {
			pthread_mutex_unlock(&threads[n].mutex);
			continue;
		}

		goto execute;
	}

	n = rand() % max_threads;
	pthread_mutex_lock(&threads[n].mutex);
	while (threads[n].func)
		pthread_cond_wait(&threads[n].cond, &threads[n].mutex);

execute:
	threads[n].func = func;
	threads[n].arg = arg;
	pthread_cond_signal(&threads[n].cond);
	pthread_mutex_unlock(&threads[n].mutex);
}

void sna_threads_wait(void)
{
	int n;

	assert(max_threads > 0);

	for (n = 0; n < max_threads; n++) {
		if (threads[n].func == NULL)
			continue;

		pthread_mutex_lock(&threads[n].mutex);
		while (threads[n].func)
			pthread_cond_wait(&threads[n].cond, &threads[n].mutex);
		pthread_mutex_unlock(&threads[n].mutex);
	}
}

int sna_use_threads(int width, int height, int threshold)
{
	int num_threads;

	if (max_threads <= 0)
		return 1;

	num_threads = height / (128/width + 1) / threshold-1;
	if (num_threads <= 0)
		return 1;

	if (num_threads > max_threads)
		num_threads = max_threads;
	return num_threads;
}

struct thread_composite {
	pixman_image_t *src, *mask, *dst;
	pixman_op_t op;
	int16_t src_x, src_y;
	int16_t mask_x, mask_y;
	int16_t dst_x, dst_y;
	uint16_t width, height;
};

static void thread_composite(void *arg)
{
	struct thread_composite *t = arg;
	pixman_image_composite(t->op, t->src, t->mask, t->dst,
			       t->src_x, t->src_y,
			       t->mask_x, t->mask_y,
			       t->dst_x, t->dst_y,
			       t->width, t->height);
}

void sna_image_composite(pixman_op_t        op,
			 pixman_image_t    *src,
			 pixman_image_t    *mask,
			 pixman_image_t    *dst,
			 int16_t            src_x,
			 int16_t            src_y,
			 int16_t            mask_x,
			 int16_t            mask_y,
			 int16_t            dst_x,
			 int16_t            dst_y,
			 uint16_t           width,
			 uint16_t           height)
{
	int num_threads;

	num_threads = sna_use_threads(width, height, 16);
	if (num_threads <= 1) {
		pixman_image_composite(op, src, mask, dst,
				       src_x, src_y,
				       mask_x, mask_y,
				       dst_x, dst_y,
				       width, height);
	} else {
		struct thread_composite threads[num_threads];
		int y, dy, n;

		y = dst_y;
		dy = (height + num_threads - 1) / num_threads;
		for (n = 0; n < num_threads; n++) {
			threads[n].op = op;
			threads[n].src = src;
			threads[n].mask = mask;
			threads[n].dst = dst;
			threads[n].src_x = src_x;
			threads[n].src_y = src_y + y - dst_y;
			threads[n].mask_x = mask_x;
			threads[n].mask_y = mask_y + y - dst_y;
			threads[n].dst_x = dst_x;
			threads[n].dst_y = y;
			threads[n].width = width;
			threads[n].height = dy;

			sna_threads_run(thread_composite, &threads[n]);

			y += dy;
			if (y + dy > dst_y + height)
				dy = dst_y + height - y;
		}
		sna_threads_wait();
	}
}
