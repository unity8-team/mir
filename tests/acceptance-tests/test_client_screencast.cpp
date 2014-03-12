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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir_toolkit/mir_screencast.h"
#include "mir_toolkit/mir_client_library.h"

#include "mir_test_framework/stubbed_server_configuration.h"
#include "mir_test_framework/in_process_server.h"
#include "mir_test_framework/using_stub_client_platform.h"

#include "mir_test_doubles/null_display_changer.h"
#include "mir_test_doubles/stub_display_configuration.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/mock_screencast.h"
#include "mir_test/fake_shared.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;
namespace mg = mir::graphics;
namespace mf = mir::frontend;
namespace mt = mir::test;
namespace geom = mir::geometry;

namespace
{

class StubChanger : public mtd::NullDisplayChanger
{
public:
    StubChanger()
        : stub_display_config{{{connected, !used}, {connected, used}}}
    {
    }

    std::shared_ptr<mg::DisplayConfiguration> active_configuration() override
    {
        return mt::fake_shared(stub_display_config);
    }

    mtd::StubDisplayConfig stub_display_config;

private:
    static bool const connected;
    static bool const used;
};

bool const StubChanger::connected{true};
bool const StubChanger::used{true};

struct StubServerConfig : mir_test_framework::StubbedServerConfiguration
{
    std::shared_ptr<mf::DisplayChanger> the_frontend_display_changer() override
    {
        return mt::fake_shared(stub_changer);
    }

    std::shared_ptr<mf::Screencast> the_screencast() override
    {
        return mt::fake_shared(mock_screencast);
    }

    StubChanger stub_changer;
    mtd::MockScreencast mock_screencast;
};

class MirScreencastTest : public mir_test_framework::InProcessServer
{
public:
    mir::DefaultServerConfiguration& server_config() override { return server_config_; }

    mtd::MockScreencast& mock_screencast() { return server_config_.mock_screencast; }

    StubServerConfig server_config_;
    mtf::UsingStubClientPlatform using_stub_client_platform;
    MirScreencastParameters default_screencast_params {
        {0, 0, 1, 1}, 1, 1, mir_pixel_format_abgr_8888};
};

}

TEST_F(MirScreencastTest, creation_with_invalid_connection_fails)
{
    using namespace testing;

    auto screencast = mir_connection_create_screencast_sync(nullptr, &default_screencast_params);
    ASSERT_EQ(nullptr, screencast);
}

TEST_F(MirScreencastTest, contacts_server_screencast_for_create_and_release)
{
    using namespace testing;

    auto const connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    ASSERT_TRUE(mir_connection_is_valid(connection));

    mf::ScreencastSessionId const screencast_session_id{99};

    InSequence seq;

    EXPECT_CALL(mock_screencast(),
                create_session(_, _, _))
        .WillOnce(Return(screencast_session_id));

    EXPECT_CALL(mock_screencast(), capture(screencast_session_id))
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));

    EXPECT_CALL(mock_screencast(), destroy_session(screencast_session_id));

    auto screencast = mir_connection_create_screencast_sync(connection, &default_screencast_params);
    ASSERT_NE(nullptr, screencast);
    mir_screencast_release_sync(screencast);

    mir_connection_release(connection);
}

TEST_F(MirScreencastTest, contacts_server_screencast_with_provided_params)
{
    using namespace testing;

    auto const connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    ASSERT_TRUE(mir_connection_is_valid(connection));

    mf::ScreencastSessionId const screencast_session_id{99};

    geom::Size const size{default_screencast_params.width, default_screencast_params.height};
    geom::Rectangle const region {
        {default_screencast_params.region.left, default_screencast_params.region.top},
        {default_screencast_params.region.width, default_screencast_params.region.height}};
    MirPixelFormat pixel_format {default_screencast_params.pixel_format};

    InSequence seq;

    EXPECT_CALL(mock_screencast(),
                create_session(region, size, pixel_format))
        .WillOnce(Return(screencast_session_id));

    EXPECT_CALL(mock_screencast(), capture(_))
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));

    EXPECT_CALL(mock_screencast(), destroy_session(_));

    auto screencast = mir_connection_create_screencast_sync(connection, &default_screencast_params);
    ASSERT_NE(nullptr, screencast);

    mir_screencast_release_sync(screencast);

    mir_connection_release(connection);
}

TEST_F(MirScreencastTest, creation_with_invalid_params_fails)
{
    using namespace testing;

    auto const connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    ASSERT_TRUE(mir_connection_is_valid(connection));

    MirScreencastParameters params = default_screencast_params;
    params.width = params.height = 0;
    auto screencast = mir_connection_create_screencast_sync(connection, &params);
    ASSERT_EQ(nullptr, screencast);

    params = default_screencast_params;
    params.region.width = params.region.height = 0;
    screencast = mir_connection_create_screencast_sync(connection, &params);
    ASSERT_EQ(nullptr, screencast);

    params = default_screencast_params;
    params.pixel_format = mir_pixel_format_invalid;

    screencast = mir_connection_create_screencast_sync(connection, &params);
    ASSERT_EQ(nullptr, screencast);

    mir_connection_release(connection);
}

TEST_F(MirScreencastTest, gets_valid_egl_native_window)
{
    using namespace testing;

    auto const connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    ASSERT_TRUE(mir_connection_is_valid(connection));

    mf::ScreencastSessionId const screencast_session_id{99};

    InSequence seq;
    EXPECT_CALL(mock_screencast(), create_session(_, _, _))
        .WillOnce(Return(screencast_session_id));
    EXPECT_CALL(mock_screencast(), capture(_))
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));
    EXPECT_CALL(mock_screencast(), destroy_session(_));

    auto screencast = mir_connection_create_screencast_sync(connection, &default_screencast_params);
    ASSERT_NE(nullptr, screencast);

    auto egl_native_window = mir_screencast_egl_native_window(screencast);
    EXPECT_NE(MirEGLNativeWindowType(), egl_native_window);

    mir_screencast_release_sync(screencast);

    mir_connection_release(connection);
}

TEST_F(MirScreencastTest, fails_on_client_when_server_request_fails)
{
    using namespace testing;

    auto const connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    ASSERT_TRUE(mir_connection_is_valid(connection));

    EXPECT_CALL(mock_screencast(), create_session(_, _, _))
        .WillOnce(Throw(std::runtime_error("")));

    auto screencast = mir_connection_create_screencast_sync(connection, &default_screencast_params);
    ASSERT_EQ(nullptr, screencast);

    mir_connection_release(connection);
}
