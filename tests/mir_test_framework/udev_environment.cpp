/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 * Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir_test_framework/udev_environment.h"

#include <umockdev.h>

#include <fstream>
#include <sstream>

namespace mtf = mir::mir_test_framework;

mtf::UdevEnvironment::UdevEnvironment()
{
    testbed = umockdev_testbed_new();
}

mtf::UdevEnvironment::~UdevEnvironment() noexcept
{
    g_object_unref(testbed);
}

void mtf::UdevEnvironment::add_standard_drm_devices()
{
    // Temporary, until umockdev grows add_from_file
    std::ifstream udev_dump(UDEVMOCK_DIR"/standard-drm-devices.umockdev");
    std::stringstream buffer;
    buffer<<udev_dump.rdbuf();

    umockdev_testbed_add_from_string(testbed,
                                     buffer.str().c_str(),
                                     NULL);
}
