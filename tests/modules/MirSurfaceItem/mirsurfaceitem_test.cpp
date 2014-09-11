/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <gtest/gtest.h>

// the test subject
#include <mirsurfaceitem.h>

// mocks
#include <mock_surface.h>
#include <mock_session.h>

using namespace qtmir;

using mir::scene::MockSurface;
using qtmir::MockSession;

TEST(MirSurfaceItemTest, Foo)
{
    std::shared_ptr<MockSurface> mockSurface = std::make_shared<MockSurface>();
    MockSession *mockSession = new MockSession;
    /*
    MirSurfaceItem *surfaceItem = new MirSurfaceItem(mockSurface,
        QPointer<mockSession);
    delete surfaceItem;
    */
}
