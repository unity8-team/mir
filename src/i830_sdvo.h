/*
 * Copyright © 2006 Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *
 */

typedef struct _i830_sdvo_caps {
  CARD8 vendor_id;
  CARD8 device_id;
  CARD8 device_rev_id;
  CARD8 sdvo_version_major;
  CARD8 sdvo_version_minor;
  CARD8 caps;
  CARD8 output_0_supported;
  CARD8 output_1_supported;
} __attribute__((packed)) i830_sdvo_caps;

typedef struct _i830_sdvo_dtd {
    CARD16 clock;
    CARD8 h_active;
    CARD8 h_blank;
    CARD8 h_high;
    CARD8 v_active;
    CARD8 v_blank;
    CARD8 v_high;

    CARD8 h_sync_off;
    CARD8 h_sync_width;
    CARD8 v_sync_off_width;
    CARD8 sync_off_width_high;
    CARD8 dtd_flags;
    CARD8 sdvo_flags;
    CARD8 v_sync_off_high;
    CARD8 reserved;
} __attribute__((packed)) i830_sdvo_dtd;

void
i830SDVOSave(ScrnInfoPtr pScrn, int output_index);

void
i830SDVOPreRestore(ScrnInfoPtr pScrn, int output_index);

void
i830SDVOPostRestore(ScrnInfoPtr pScrn, int output_index);

Bool
I830DetectSDVODisplays(ScrnInfoPtr pScrn, int output_index);

void
I830DumpSDVO(ScrnInfoPtr pScrn);
