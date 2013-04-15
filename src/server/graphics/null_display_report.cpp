/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/graphics/null_display_report.h"

namespace mg = mir::graphics;

void mg::NullDisplayReport::report_successful_setup_of_native_resources() {}
void mg::NullDisplayReport::report_successful_egl_make_current_on_construction() {}
void mg::NullDisplayReport::report_successful_egl_buffer_swap_on_construction() {}
void mg::NullDisplayReport::report_successful_drm_mode_set_crtc_on_construction() {}
void mg::NullDisplayReport::report_successful_display_construction() {}
