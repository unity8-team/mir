/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir_toolkit/mir_client_library_drm.h"

#include "mir_test_framework/connected_client_headless_server.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mtf = mir_test_framework;
using namespace testing;

using MirClientLibraryDrmTest = mtf::ConnectedClientHeadlessServer;

TEST_F(MirClientLibraryDrmTest, SetsGbmDeviceInPlatformData)
{
    struct gbm_device* dev = reinterpret_cast<struct gbm_device*>(connection);

    mir_connection_drm_set_gbm_device(connection, dev);

    MirPlatformPackage pkg;
    mir_connection_get_platform(connection, &pkg);
    EXPECT_THAT(pkg.data_items, Eq(sizeof(dev) / sizeof(int)));
    EXPECT_THAT(*reinterpret_cast<struct gbm_device**>(&pkg.data[0]), Eq(dev));
}
