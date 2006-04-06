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

struct vbt_header {
    char signature[20];			/**< Always 'BIOS_DATA_BLOCK' */
    CARD16 version;			/**< decimal */
    CARD16 header_size;			/**< in bytes */
    CARD16 vbt_size;			/**< in bytes */
    CARD8 vbt_checksum;
    CARD8 reserved0;
    CARD32 bdb_offset;			/**< from beginning of VBT */
    CARD32 aim1_offset;			/**< from beginning of VBT */
    CARD32 aim2_offset;			/**< from beginning of VBT */
    CARD32 aim3_offset;			/**< from beginning of VBT */
    CARD32 aim4_offset;			/**< from beginning of VBT */
};

struct bdb_header {
    char signature[16];			/**< Always 'BIOS_DATA_BLOCK' */
    CARD16 version;			/**< decimal */
    CARD16 header_size;			/**< in bytes */
    CARD16 bdb_size;			/**< in bytes */
};

Bool
i830GetLVDSInfoFromBIOS(ScrnInfoPtr pScrn);
