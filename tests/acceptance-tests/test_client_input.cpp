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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "mir/graphics/display.h"
#include "mir/graphics/viewable_area.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/shell/placement_strategy.h"
#include "mir/shell/surface_factory.h"
#include "mir/shell/surface.h"
#include "mir/surfaces/surface_controller.h"
#include "mir/surfaces/surface_stack_model.h"

#include "src/server/input/android/android_input_manager.h"
#include "src/server/input/android/android_input_targeter.h"

#include "mir_toolkit/mir_client_library.h"

#include "mir_test/fake_shared.h"
#include "mir_test/fake_event_hub.h"
#include "mir_test/event_factory.h"
#include "mir_test/wait_condition.h"
#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_framework/input_testing_server_configuration.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <thread>
#include <functional>
#include <map>

namespace mi = mir::input;
namespace mia = mi::android;
namespace mis = mi::synthesis;
namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace me = mir::events;
namespace ms = mir::surfaces;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace mtf = mir_test_framework;

namespace
{
    char const* const mir_test_socket = mtf::test_socket_file().c_str();
}

namespace
{

struct ClientConfig : mtf::TestingClientConfiguration
{
    ClientConfig() :
        connection(0),
        surface(0)
    {
    }

    static void connection_callback(MirConnection* connection, void* context)
    {
        ClientConfig* config = reinterpret_cast<ClientConfig *>(context);
        config->connection = connection;
    }

    static void create_surface_callback(MirSurface* surface, void* context)
    {
        ClientConfig* config = reinterpret_cast<ClientConfig *>(context);
        config->surface_created(surface);
    }

    static void release_surface_callback(MirSurface* surface, void* context)
    {
        ClientConfig* config = reinterpret_cast<ClientConfig *>(context);
        config->surface_released(surface);
    }

    virtual void connected(MirConnection* new_connection)
    {
        connection = new_connection;
    }

    virtual void surface_created(MirSurface* new_surface)
    {
        surface = new_surface;
    }

    virtual void surface_released(MirSurface* /* released_surface */)
    {
        surface = NULL;
    }

    MirConnection* connection;
    MirSurface* surface;
};

struct MockInputHandler
{
    MOCK_METHOD1(handle_input, void(MirEvent const*));
};

struct InputClient : ClientConfig
{
    InputClient(std::string const& surface_name)
      : surface_name(surface_name)
    {
    }

    static void handle_input(MirSurface* /* surface */, MirEvent const* ev, void* context)
    {
        auto client = static_cast<InputClient *>(context);

        client->handler->handle_input(ev);
    }

    virtual void expect_input(mt::WaitCondition&)
    {
    }
    
    virtual MirSurfaceParameters parameters()
    {
        MirSurfaceParameters const request_params =
         {
             surface_name.c_str(),
             surface_width, surface_height,
             mir_pixel_format_abgr_8888,
             mir_buffer_usage_hardware
         };
        return request_params;
    }

    void exec()
    {
        handler = std::make_shared<MockInputHandler>();

        expect_input(events_received);

        mir_wait_for(mir_connect(
            mir_test_socket,
            __PRETTY_FUNCTION__,
            connection_callback,
            this));
         ASSERT_TRUE(connection != NULL);

         MirEventDelegate const event_delegate =
         {
             handle_input,
             this
         };
         auto request_params = parameters();
         mir_wait_for(mir_connection_create_surface(connection, &request_params, create_surface_callback, this));

         mir_surface_set_event_handler(surface, &event_delegate);

         events_received.wait_for_at_most_seconds(60);

         mir_surface_release_sync(surface);
         
         mir_connection_release(connection);

         // ClientConfiguration d'tor is not called on client side so we need this
         // in order to not leak the Mock object.
         handler.reset();
    }
    
    std::shared_ptr<MockInputHandler> handler;
    mt::WaitCondition events_received;
    
    std::string const surface_name;
    
    static int const surface_width = 100;
    static int const surface_height = 100;
};

MATCHER(KeyDownEvent, "")
{
    if (arg->type != mir_event_type_key)
        return false;
    if (arg->key.action != mir_key_action_down) // Key down
        return false;

    return true;
}
MATCHER_P(KeyOfSymbol, keysym, "")
{
    if (static_cast<xkb_keysym_t>(arg->key.key_code) == (uint)keysym)
        return true;
    return false;
}

MATCHER(HoverEnterEvent, "")
{
    if (arg->type != mir_event_type_motion)
        return false;
    if (arg->motion.action != mir_motion_action_hover_enter)
        return false;

    return true;
}
MATCHER(HoverExitEvent, "")
{
    if (arg->type != mir_event_type_motion)
        return false;
    if (arg->motion.action != mir_motion_action_hover_exit)
        return false;

    return true;
}

MATCHER_P2(ButtonDownEvent, x, y, "")
{
    if (arg->type != mir_event_type_motion)
        return false;
    if (arg->motion.action != mir_motion_action_down)
        return false;
    if (arg->motion.button_state == 0)
        return false;
    if (arg->motion.pointer_coordinates[0].x != x)
        return false;
    if (arg->motion.pointer_coordinates[0].y != y)
        return false;
    return true;
}

MATCHER_P2(ButtonUpEvent, x, y, "")
{
    if (arg->type != mir_event_type_motion)
        return false;
    if (arg->motion.action != mir_motion_action_up)
        return false;
    if (arg->motion.pointer_coordinates[0].x != x)
        return false;
    if (arg->motion.pointer_coordinates[0].y != y)
        return false;
    return true;
}

MATCHER_P2(MotionEventWithPosition, x, y, "")
{
    if (arg->type != mir_event_type_motion)
        return false;
    if (arg->motion.action != mir_motion_action_move &&
        arg->motion.action != mir_motion_action_hover_move)
        return false;
    if (arg->motion.pointer_coordinates[0].x != x)
        return false;
    if (arg->motion.pointer_coordinates[0].y != y)
        return false;
    return true;
}

MATCHER(MovementEvent, "")
{
    if (arg->type != mir_event_type_motion)
        return false;
    if (arg->motion.action != mir_motion_action_move &&
        arg->motion.action != mir_motion_action_hover_move)
        return false;
    return true;
}

}


using TestClientInput = BespokeDisplayServerTestFixture;

TEST_F(TestClientInput, clients_receive_key_input)
{
    using namespace ::testing;
    
    int const num_events_produced = 3;
    static std::string const test_client_name = "1";

    struct ServerConfiguration : mtf::InputTestingServerConfiguration
    {
        void inject_input()
        {
            wait_until_client_appears(test_client_name);
            for (int i = 0; i < num_events_produced; i++)
                fake_event_hub->synthesize_event(mis::a_key_down_event()
                                                 .of_scancode(KEY_ENTER));
        }
    } server_config;
    launch_server_process(server_config);
    
    struct KeyReceivingClient : InputClient
    {
        KeyReceivingClient() : InputClient(test_client_name) {}
        void expect_input(mt::WaitCondition& events_received) override
        {
            using namespace ::testing;
            InSequence seq;

            EXPECT_CALL(*handler, handle_input(KeyDownEvent())).Times(2);
            EXPECT_CALL(*handler, handle_input(KeyDownEvent())).Times(1)
                .WillOnce(mt::WakeUp(&events_received));
        }
    } client_config;
    launch_client_process(client_config);
}

TEST_F(TestClientInput, clients_receive_us_english_mapped_keys)
{
    using namespace ::testing;
    static std::string const test_client_name = "1";
    
    struct ServerConfiguration : mtf::InputTestingServerConfiguration
    {
        void inject_input()
        {
            wait_until_client_appears(test_client_name);

            fake_event_hub->synthesize_event(mis::a_key_down_event()
                                             .of_scancode(KEY_LEFTSHIFT));
            fake_event_hub->synthesize_event(mis::a_key_down_event()
                                             .of_scancode(KEY_4));

        }
    } server_config;
    launch_server_process(server_config);
    
    struct KeyReceivingClient : InputClient
    {
        KeyReceivingClient() : InputClient(test_client_name) {}

        void expect_input(mt::WaitCondition& events_received) override
        {
            using namespace ::testing;

            InSequence seq;
            EXPECT_CALL(*handler, handle_input(AllOf(KeyDownEvent(), KeyOfSymbol(XKB_KEY_Shift_L)))).Times(1);
            EXPECT_CALL(*handler, handle_input(AllOf(KeyDownEvent(), KeyOfSymbol(XKB_KEY_dollar)))).Times(1)
                .WillOnce(mt::WakeUp(&events_received));
        }
    } client_config;
    launch_client_process(client_config);
}

TEST_F(TestClientInput, clients_receive_motion_inside_window)
{
    using namespace ::testing;
    static std::string const test_client_name = "1";
    
    struct ServerConfiguration : public mtf::InputTestingServerConfiguration
    {
        void inject_input()
        {
            wait_until_client_appears(test_client_name);
            
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(InputClient::surface_width,
                                                                                 InputClient::surface_height));
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(2,2));
        }
    } server_config;
    launch_server_process(server_config);
    
    struct MotionReceivingClient : InputClient
    {
        MotionReceivingClient() : InputClient(test_client_name) {}

        void expect_input(mt::WaitCondition& events_received) override
        {
            using namespace ::testing;
            
            InSequence seq;

            // We should see the cursor enter
            EXPECT_CALL(*handler, handle_input(HoverEnterEvent())).Times(1);
            EXPECT_CALL(*handler, handle_input(
                MotionEventWithPosition(InputClient::surface_width,
                                        InputClient::surface_height))).Times(1)
                .WillOnce(mt::WakeUp(&events_received));
            // But we should not receive an event for the second movement outside of our surface!
        }
    } client_config;
    launch_client_process(client_config);
}

TEST_F(TestClientInput, clients_receive_button_events_inside_window)
{
    using namespace ::testing;
    
    static std::string const test_client_name = "1";
    
    struct ServerConfiguration : public mtf::InputTestingServerConfiguration
    {
        void inject_input()
        {
            wait_until_client_appears(test_client_name);
            fake_event_hub->synthesize_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
        }
    } server_config;
    launch_server_process(server_config);
    
    struct ButtonReceivingClient : InputClient
    {
        ButtonReceivingClient() : InputClient(test_client_name) {}

        void expect_input(mt::WaitCondition& events_received) override
        {
            using namespace ::testing;
            
            InSequence seq;

            // The cursor starts at (0, 0).
            EXPECT_CALL(*handler, handle_input(ButtonDownEvent(0, 0))).Times(1)
                .WillOnce(mt::WakeUp(&events_received));
        }
    } client_config;
    launch_client_process(client_config);
}

namespace
{
typedef std::map<std::string, geom::Rectangle> GeometryList;

struct StaticPlacementStrategy : public msh::PlacementStrategy
{
    StaticPlacementStrategy(GeometryList const& positions)
        : surface_geometry_by_name(positions)
    {
    }

    msh::SurfaceCreationParameters place(msh::SurfaceCreationParameters const& request_parameters)
    {
        auto placed = request_parameters;
        auto geometry = surface_geometry_by_name[request_parameters.name];

        placed.top_left = geometry.top_left;
        placed.size = geometry.size;
        
        return placed;
    }
    GeometryList surface_geometry_by_name;
};

}

TEST_F(TestClientInput, multiple_clients_receive_motion_inside_windows)
{
    using namespace ::testing;
    
    static int const screen_width = 1000;
    static int const screen_height = 800;
    static int const client_height = screen_height/2;
    static int const client_width = screen_width/2;
    static std::string const test_client_1 = "1";
    static std::string const test_client_2 = "2";
    
    struct ServerConfiguration : mtf::InputTestingServerConfiguration
    {
        std::shared_ptr<msh::PlacementStrategy> the_shell_placement_strategy() override
        {
            static GeometryList positions;
            positions[test_client_1] = geom::Rectangle{geom::Point{geom::X{0}, geom::Y{0}},
                geom::Size{geom::Width{client_width}, geom::Height{client_height}}};
            positions[test_client_2] = geom::Rectangle{geom::Point{geom::X{screen_width/2}, geom::Y{screen_height/2}},
                geom::Size{geom::Width{client_width}, geom::Height{client_height}}};

            return std::make_shared<StaticPlacementStrategy>(positions);
        }
        
        geom::Rectangle the_screen_geometry() override
        {
            return geom::Rectangle{geom::Point{geom::X{0}, geom::Y{0}},
                    geom::Size{geom::Width{screen_width}, geom::Height{screen_height}}};
        }

        void inject_input() override
        {
            wait_until_client_appears(test_client_1);
            wait_until_client_appears(test_client_2);
            // In the bounds of the first surface
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(screen_width/2-1, screen_height/2-1));
            // In the bounds of the second surface
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(screen_width/2, screen_height/2));
        }
    } server_config;
    
    launch_server_process(server_config);
    
    struct InputClientOne : InputClient
    {
        InputClientOne()
           : InputClient(test_client_1)
        {
        }
        
        void expect_input(mt::WaitCondition& events_received) override
        {
            InSequence seq;
            EXPECT_CALL(*handler, handle_input(HoverEnterEvent())).Times(1);
            EXPECT_CALL(*handler, handle_input(MotionEventWithPosition(client_width - 1, client_height - 1))).Times(1);
            EXPECT_CALL(*handler, handle_input(HoverExitEvent())).Times(1)
                .WillOnce(mt::WakeUp(&events_received));
        }
    } client_1;

    struct InputClientTwo : InputClient
    {
        InputClientTwo()
            : InputClient(test_client_2)
        {
        }
        
        void expect_input(mt::WaitCondition& events_received) override
        {
            InSequence seq;
            EXPECT_CALL(*handler, handle_input(HoverEnterEvent())).Times(1);
            EXPECT_CALL(*handler, handle_input(MotionEventWithPosition(client_width - 1, client_height - 1))).Times(1)
                .WillOnce(mt::WakeUp(&events_received));
        }
    } client_2;

    launch_client_process(client_1);
    launch_client_process(client_2);
}

namespace
{
struct RegionApplyingSurfaceFactory : public msh::SurfaceFactory
{
    RegionApplyingSurfaceFactory(std::shared_ptr<msh::SurfaceFactory> real_factory,
        std::initializer_list<geom::Rectangle> const& input_rectangles)
        : underlying_factory(real_factory),
          input_rectangles(input_rectangles)
    {
    }
    
    std::shared_ptr<msh::Surface> create_surface(msh::SurfaceCreationParameters const& params,
                                                 mf::SurfaceId id,
                                                 std::shared_ptr<me::EventSink> const& sink)
    {
        auto surface = underlying_factory->create_surface(params, id, sink);

        surface->set_input_region(input_rectangles);

        return surface;
    }
    
    std::shared_ptr<msh::SurfaceFactory> const underlying_factory;
    std::vector<geom::Rectangle> const input_rectangles;
};
}
TEST_F(TestClientInput, clients_do_not_receive_motion_outside_input_region)
{
    using namespace ::testing;
    static std::string const test_client_name = "1";
    
    static int const screen_width = 100;
    static int const screen_height = 100;
    
    static geom::Rectangle const screen_geometry{geom::Point{geom::X{0}, geom::Y{0}},
        geom::Size{geom::Width{screen_width}, geom::Height{screen_height}}};

    static std::initializer_list<geom::Rectangle> client_input_regions = {
        {{geom::X{0}, geom::Y{0}}, {geom::Width{screen_width-80}, geom::Height{screen_height}}},
        {{geom::X{screen_width-20}, geom::Y{0}}, {geom::Width{screen_width-80}, geom::Height{screen_height}}}
    };

    struct ServerConfiguration : mtf::InputTestingServerConfiguration
    {
        std::shared_ptr<msh::PlacementStrategy> the_shell_placement_strategy() override
        {
            static GeometryList positions;
            positions[test_client_name] = screen_geometry;
            
            return std::make_shared<StaticPlacementStrategy>(positions);
        }
        std::shared_ptr<msh::SurfaceFactory> the_shell_surface_factory() override
        {
            return std::make_shared<RegionApplyingSurfaceFactory>(InputTestingServerConfiguration::the_shell_surface_factory(),
                client_input_regions);
        }
        geom::Rectangle the_screen_geometry() override
        {
            return screen_geometry;
        }
        
        void inject_input() override
        {
            wait_until_client_appears(test_client_name);
            
            // First we will move the cursor in to the input region on the left side of the window. We should see a click here
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(1, 1));
            fake_event_hub->synthesize_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
            fake_event_hub->synthesize_event(mis::a_button_up_event().of_button(BTN_LEFT));
            // Now in to the dead zone in the center of the window. We should not see a click here.
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(49, 49));
            fake_event_hub->synthesize_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
            fake_event_hub->synthesize_event(mis::a_button_up_event().of_button(BTN_LEFT));
            // Now in to the right edge of the window, in the right input region. Again we should see a click
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(49, 49));
            fake_event_hub->synthesize_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
            fake_event_hub->synthesize_event(mis::a_button_up_event().of_button(BTN_LEFT));
        }
    } server_config;
    
    launch_server_process(server_config);

    struct ClientConfig : InputClient
    {
        ClientConfig()
            : InputClient(test_client_name)
        {
        }

        void expect_input(mt::WaitCondition& events_received) override
        {

            EXPECT_CALL(*handler, handle_input(HoverEnterEvent())).Times(AnyNumber());
            EXPECT_CALL(*handler, handle_input(HoverExitEvent())).Times(AnyNumber());
            EXPECT_CALL(*handler, handle_input(MovementEvent())).Times(AnyNumber());

            {
                // We should see two of the three button pairs.
                InSequence seq;
                EXPECT_CALL(*handler, handle_input(ButtonDownEvent(1, 1))).Times(1);
                EXPECT_CALL(*handler, handle_input(ButtonUpEvent(1, 1))).Times(1);
                EXPECT_CALL(*handler, handle_input(ButtonDownEvent(99, 99))).Times(1);
                EXPECT_CALL(*handler, handle_input(ButtonUpEvent(99, 99))).Times(1)
                    .WillOnce(mt::WakeUp(&events_received));
            }
        }
    } client_config;
    launch_client_process(client_config);
}

namespace
{
typedef std::map<std::string, ms::DepthId> DepthList;

struct StackingSurfaceController : public ms::SurfaceController
{
    StackingSurfaceController(std::shared_ptr<ms::SurfaceStackModel> const& surface_stack_model, DepthList const& depths)
        : SurfaceController(surface_stack_model),
          surface_depths_by_name(depths)
    {
    }
    
    std::weak_ptr<ms::Surface> create_surface(msh::SurfaceCreationParameters const& params) override
    {
        return surface_stack->create_surface(params, surface_depths_by_name[params.name]);
    }
    
    DepthList surface_depths_by_name;
};
}

TEST_F(TestClientInput, surfaces_obscure_motion_events_by_stacking)
{
    using namespace ::testing;
    
    static std::string const test_client_name_1 = "1";
    static std::string const test_client_name_2 = "2";

    static int const screen_width = 100;
    static int const screen_height = 100;
    
    static geom::Rectangle const screen_geometry{geom::Point{geom::X{0}, geom::Y{0}},
        geom::Size{geom::Width{screen_width}, geom::Height{screen_height}}};

    struct ServerConiguration : mtf::InputTestingServerConfiguration
    {
        std::shared_ptr<msh::PlacementStrategy> the_shell_placement_strategy() override
        {
            static GeometryList positions;
            positions[test_client_name_1] = screen_geometry;
            
            auto smaller_geometry = screen_geometry;
            smaller_geometry.size.width = geom::Width{screen_width/2};
            positions[test_client_name_2] = smaller_geometry;
            
            return std::make_shared<StaticPlacementStrategy>(positions);
       }
        
        std::shared_ptr<msh::SurfaceBuilder> the_surface_builder() override
        {
            static DepthList depths;
            depths[test_client_name_1] = ms::DepthId{0};
            depths[test_client_name_2] = ms::DepthId{1};
            
            return std::make_shared<StackingSurfaceController>(the_surface_stack_model(), depths);
        }
        
        void inject_input() override
        {
            wait_until_client_appears(test_client_name_1);
            wait_until_client_appears(test_client_name_2);
            
            // First we will move the cursor in to the region where client 2 obscures client 1
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(1, 1));
            fake_event_hub->synthesize_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
            fake_event_hub->synthesize_event(mis::a_button_up_event().of_button(BTN_LEFT));
            // Now we move to the unobscured region of client 1
            fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(50, 0));
            fake_event_hub->synthesize_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
            fake_event_hub->synthesize_event(mis::a_button_up_event().of_button(BTN_LEFT));
        }
    } server_config;
    
    launch_server_process(server_config);

    struct ClientConfigOne : InputClient
    {
        ClientConfigOne()
            : InputClient(test_client_name_1)
        {
        }

        void expect_input(mt::WaitCondition& events_received) override
        {
            EXPECT_CALL(*handler, handle_input(HoverEnterEvent())).Times(AnyNumber());
            EXPECT_CALL(*handler, handle_input(HoverExitEvent())).Times(AnyNumber());
            EXPECT_CALL(*handler, handle_input(MovementEvent())).Times(AnyNumber());

            {
                // We should only see one button event sequence.
                InSequence seq;
                EXPECT_CALL(*handler, handle_input(ButtonDownEvent(51, 1))).Times(1);
                EXPECT_CALL(*handler, handle_input(ButtonUpEvent(51, 1))).Times(1)
                    .WillOnce(mt::WakeUp(&events_received));
            }
        }
    } client_config_1;
    launch_client_process(client_config_1);

    struct ClientConfigTwo : InputClient
    {
        ClientConfigTwo()
            : InputClient(test_client_name_2)
        {
        }

        void expect_input(mt::WaitCondition& events_received) override
        {
            EXPECT_CALL(*handler, handle_input(HoverEnterEvent())).Times(AnyNumber());
            EXPECT_CALL(*handler, handle_input(HoverExitEvent())).Times(AnyNumber());
            EXPECT_CALL(*handler, handle_input(MovementEvent())).Times(AnyNumber());

            {
                // Likewise we should only see one button sequence.
              InSequence seq;
              EXPECT_CALL(*handler, handle_input(ButtonDownEvent(1, 1))).Times(1);
              EXPECT_CALL(*handler, handle_input(ButtonUpEvent(1, 1))).Times(1)
                  .WillOnce(mt::WakeUp(&events_received));
            }
        }
    } client_config_2;
    launch_client_process(client_config_2);
}
