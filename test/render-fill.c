#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xutil.h> /* for XDestroyImage */
#include <pixman.h> /* for pixman blt functions */

#include "test.h"

static const uint8_t ops[] = {
	PictOpClear,
	PictOpSrc,
	PictOpDst,
};

static void fill_rect(struct test_display *dpy, Picture p, uint8_t op,
		      int x, int y, int w, int h,
		      uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
	XRenderColor render_color;

	render_color.red   = red * alpha;
	render_color.green = green * alpha;
	render_color.blue  = blue * alpha;
	render_color.alpha = alpha << 8;

	XRenderFillRectangle(dpy->dpy, op, p, &render_color, x, y, w,h);
}

static void pixel_tests(struct test *t, int reps, int sets, enum target target)
{
	struct test_target tt;
	XImage image;
	uint32_t *cells = malloc(t->real.width*t->real.height*4);
	struct {
		uint16_t x, y;
	} *pixels = malloc(reps*sizeof(*pixels));
	int r, s;

	test_target_create_render(&t->real, target, &tt);

	printf("Testing setting of single pixels (%s): ",
	       test_target_name(target));
	fflush(stdout);

	for (s = 0; s < sets; s++) {
		for (r = 0; r < reps; r++) {
			int x = rand() % (tt.width - 1);
			int y = rand() % (tt.height - 1);
			int red = rand() % 0xff;
			int green = rand() % 0xff;
			int blue = rand() % 0xff;
			int alpha = rand() % 0xff;

			fill_rect(&t->real, tt.picture, PictOpSrc,
				  x, y, 1, 1,
				  red, green, blue, alpha);

			pixels[r].x = x;
			pixels[r].y = y;
			cells[y*tt.width+x] = color(red, green, blue, alpha);
		}

		test_init_image(&image, &t->real.shm, tt.format, 1, 1);

		for (r = 0; r < reps; r++) {
			uint32_t result;
			uint32_t x = pixels[r].x;
			uint32_t y = pixels[r].y;

			XShmGetImage(t->real.dpy, tt.draw, &image,
				     x, y, AllPlanes);

			result = *(uint32_t *)image.data;
			if (!pixel_equal(image.depth, result,
					 cells[y*tt.width+x])) {
				uint32_t mask = depth_mask(image.depth);
				die("failed to set pixel (%d,%d) to %08x [%08x], found %08x [%08x] instead\n",
				    x, y,
				    cells[y*tt.width+x] & mask,
				    cells[y*tt.width+x],
				    result & mask);
			}
		}
	}
	printf("passed [%d iterations x %d]\n", reps, sets);

	test_target_destroy_render(&t->real, &tt);
	free(pixels);
	free(cells);
}

static void clear(struct test_display *dpy, struct test_target *tt)
{
	XRenderColor render_color = {0};
	XRenderFillRectangle(dpy->dpy, PictOpClear, tt->picture, &render_color,
			     0, 0, tt->width, tt->height);
}

static void area_tests(struct test *t, int reps, int sets, enum target target)
{
	struct test_target tt;
	XImage image;
	uint32_t *cells = calloc(sizeof(uint32_t), t->real.width*t->real.height);
	int r, s, x, y;

	printf("Testing area sets (%s): ", test_target_name(target));
	fflush(stdout);

	test_target_create_render(&t->real, target, &tt);
	clear(&t->real, &tt);

	test_init_image(&image, &t->real.shm, tt.format, tt.width, tt.height);

	for (s = 0; s < sets; s++) {
		for (r = 0; r < reps; r++) {
			int w = rand() % tt.width;
			int h = rand() % tt.height;
			int red = rand() % 0xff;
			int green = rand() % 0xff;
			int blue = rand() % 0xff;
			int alpha = rand() % 0xff;

			x = rand() % (2*tt.width) - tt.width;
			y = rand() % (2*tt.height) - tt.height;

			fill_rect(&t->real, tt.picture, PictOpSrc,
				  x, y, w, h, red, green, blue, alpha);

			if (x < 0)
				w += x, x = 0;
			if (y < 0)
				h += y, y = 0;
			if (x >= tt.width || y >= tt.height)
				continue;

			if (x + w > tt.width)
				w = tt.width - x;
			if (y + h > tt.height)
				h = tt.height - y;
			if (w <= 0 || h <= 0)
				continue;

			pixman_fill(cells, tt.width, 32, x, y, w, h,
				    color(red, green, blue, alpha));
		}

		XShmGetImage(t->real.dpy, tt.draw, &image, 0, 0, AllPlanes);

		for (y = 0; y < tt.height; y++) {
			for (x = 0; x < tt.width; x++) {
				uint32_t result =
					*(uint32_t *)(image.data +
						      y*image.bytes_per_line +
						      image.bits_per_pixel*x/8);
				if (!pixel_equal(image.depth, result, cells[y*tt.width+x])) {
					uint32_t mask;
					if (image.depth == 32)
						mask = 0xffffffff;
					else
						mask = (1 << image.depth) - 1;
					die("failed to set pixel (%d,%d) to %08x[%08x], found %08x instead\n",
					    x, y,
					    cells[y*tt.width+x] & mask,
					    cells[y*tt.width+x],
					    result & mask);
				}
			}
		}
	}

	printf("passed [%d iterations x %d]\n", reps, sets);

	test_target_destroy_render(&t->real, &tt);
	free(cells);
}

static void rect_tests(struct test *t, int reps, int sets, enum target target)
{
	struct test_target real, ref;
	int r, s;

	printf("Testing area fills (%s): ", test_target_name(target));
	fflush(stdout);

	test_target_create_render(&t->real, target, &real);
	clear(&t->real, &real);

	test_target_create_render(&t->ref, target, &ref);
	clear(&t->ref, &ref);

	for (s = 0; s < sets; s++) {
		for (r = 0; r < reps; r++) {
			int x = rand() % (2*real.width) - real.width;
			int y = rand() % (2*real.height) - real.height;
			int w = rand() % real.width;
			int h = rand() % real.height;
			int op = ops[rand() % sizeof(ops)];
			int red = rand() % 0xff;
			int green = rand() % 0xff;
			int blue = rand() % 0xff;
			int alpha = rand() % 0xff;

			fill_rect(&t->real, real.picture,
				  op, x, y, w, h,
				  red, green, blue, alpha);
			fill_rect(&t->ref, ref.picture,
				  op, x, y, w, h,
				  red, green, blue, alpha);
		}

		test_compare(t,
			     real.draw, real.format,
			     ref.draw, ref.format,
			     0, 0, real.width, real.height);
	}

	printf("passed [%d iterations x %d]\n", reps, sets);

	test_target_destroy_render(&t->real, &real);
	test_target_destroy_render(&t->ref, &ref);
}

int main(int argc, char **argv)
{
	struct test test;
	int i;

	test_init(&test, argc, argv);

	for (i = 0; i <= DEFAULT_ITERATIONS; i++) {
		int reps = 1 << i;
		int sets = 1 << (12 - i);
		enum target t;

		if (sets < 2)
			sets = 2;

		for (t = TARGET_FIRST; t <= TARGET_LAST; t++) {
			pixel_tests(&test, reps, sets, t);
			area_tests(&test, reps, sets, t);
			rect_tests(&test, reps, sets, t);
		}
	}

	return 0;
}
