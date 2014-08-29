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

#include <Unity/Application/objectlistmodel.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace qtmir;

TEST(ObjectListModelTests, TestInsert)
{
    using namespace testing;

    ObjectListModel<QObject> model;
    QObject object1;
    QObject object2;
    QObject object3;
    QObject object4;
    QObject object5;

    // first
    model.insert(0, &object1);
    EXPECT_THAT(model.list(), ElementsAre(&object1));

    // append
    model.insert(model.rowCount(), &object2);
    EXPECT_THAT(model.list(), ElementsAre(&object1, &object2));

    // prepend
    model.insert(0, &object3);
    EXPECT_THAT(model.list(), ElementsAre(&object3, &object1, &object2));

    // overflow
    model.insert(14, &object4);
    EXPECT_THAT(model.list(), ElementsAre(&object3, &object1, &object2, &object4));

    // middle
    model.insert(2, &object5);
    EXPECT_THAT(model.list(), ElementsAre(&object3, &object1, &object5, &object2, &object4));
}

TEST(ObjectListModelTests, TestMove)
{
    using namespace testing;

    ObjectListModel<QObject> model;
    QObject object1;
    QObject object2;
    QObject object3;
    QObject object4;
    QObject object5;

    model.insert(0, &object1);
    model.insert(1, &object2);
    model.insert(2, &object3);
    model.insert(3, &object4);
    model.insert(4, &object5);

    // move before
    model.insert(0, &object3);
    EXPECT_THAT(model.list(), ElementsAre(&object3, &object1, &object2, &object4, &object5));

    // move after
    model.insert(3, &object1);
    EXPECT_THAT(model.list(), ElementsAre(&object3, &object2, &object4, &object1, &object5));

    // move end
    model.insert(4, &object3);
    EXPECT_THAT(model.list(), ElementsAre(&object2, &object4, &object1, &object5, &object3));

    // no move
    model.insert(1, &object4);
    EXPECT_THAT(model.list(), ElementsAre(&object2, &object4, &object1, &object5, &object3));

    // move past end.
    model.insert(model.rowCount(), &object2);
    EXPECT_THAT(model.list(), ElementsAre(&object4, &object1, &object5, &object3, &object2));
}

TEST(ObjectListModelTests, TestRemove)
{
    using namespace testing;

    ObjectListModel<QObject> model;
    QObject object1;
    QObject object2;
    QObject object3;
    QObject object4;
    QObject object5;
    QObject object6;

    model.insert(0, &object1);
    model.insert(1, &object2);
    model.insert(2, &object3);
    model.insert(3, &object4);
    model.insert(4, &object5);

    // first
    model.remove(&object1);
    EXPECT_THAT(model.list(), ElementsAre(&object2, &object3, &object4, &object5));

    // just-removed
    model.remove(&object1);
    EXPECT_THAT(model.list(), ElementsAre(&object2, &object3, &object4, &object5));

    // last
    model.remove(&object5);
    EXPECT_THAT(model.list(), ElementsAre(&object2, &object3, &object4));

    // middle
    model.remove(&object3);
    EXPECT_THAT(model.list(), ElementsAre(&object2, &object4));

    // non-existant
    model.remove(&object6);
    EXPECT_THAT(model.list(), ElementsAre(&object2, &object4));
}
