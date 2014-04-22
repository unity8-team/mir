/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "src/server/scene/default_input_registrar.h"

#include "mir_test_doubles/stub_input_registrar_observer.h"
#include "mir_test_doubles/mock_input_registrar_observer.h"
#include "mir_test/fake_shared.h"

namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace ms = mir::scene;

TEST(DefaultInputRegistrar, observer_receives_callbacks)
{
    using namespace ::testing;
    ms::DefaultInputRegistrar registrar;
    NiceMock<mtd::MockInputRegistrarObserver> observer1;
    NiceMock<mtd::MockInputRegistrarObserver> observer2;
    registrar.add_observer(mt::fake_shared(observer1));
    registrar.add_observer(mt::fake_shared(observer2));

    EXPECT_CALL(observer1,input_channel_opened(_,_,_));
    EXPECT_CALL(observer2,input_channel_opened(_,_,_));


    EXPECT_CALL(observer1,input_channel_closed(_));
    EXPECT_CALL(observer2,input_channel_closed(_));

    registrar.input_channel_opened({},{}, mir::input::InputReceptionMode::normal);
    registrar.input_channel_closed({});
}

TEST(DefaultInputRegistrar, unregistered_observer_stops_receiving_callbacks)
{
    using namespace ::testing;
    ms::DefaultInputRegistrar registrar;
    NiceMock<mtd::MockInputRegistrarObserver> observer1;
    NiceMock<mtd::MockInputRegistrarObserver> observer2;
    registrar.add_observer(mt::fake_shared(observer1));
    registrar.add_observer(mt::fake_shared(observer2));

    EXPECT_CALL(observer1,input_channel_opened(_,_,_));
    EXPECT_CALL(observer2,input_channel_opened(_,_,_));

    EXPECT_CALL(observer2,input_channel_closed(_));

    registrar.input_channel_opened({},{}, mir::input::InputReceptionMode::normal);
    registrar.remove_observer(mt::fake_shared(observer1));
    registrar.input_channel_closed({});
}
