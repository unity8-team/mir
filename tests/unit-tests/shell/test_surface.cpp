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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/shell/surface.h"
#include "mir/surfaces/surface.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/surfaces/surface_stack_model.h"
#include "mir/shell/surface_builder.h"

#include "mir_test_doubles/stub_buffer_stream.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/mock_input_targeter.h"
#include "mir_test_doubles/stub_input_targeter.h"
#include "mir_test/fake_shared.h"

#include <stdexcept>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace me = mir::events;
namespace ms = mir::surfaces;
namespace msh = mir::shell;
namespace mf = mir::frontend;
namespace mc = mir::compositor;
namespace mi = mir::input;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{
struct ShellSurface : testing::Test
{
    std::shared_ptr<mtd::StubBufferStream> const stub_buffer_stream;
    std::shared_ptr<ms::Surface> weak_surface;

    ShellSurface() :
        stub_buffer_stream(std::make_shared<mtd::StubBufferStream>())
    {
        using namespace testing;

        weak_surface = std::make_shared<ms::Surface>(std::string("oov"), geom::Point(),
                                                     stub_buffer_stream,
                                                     std::shared_ptr<mi::InputChannel>(), [](){});
    }
};
}

//TODO: this should be eliminated with the msh cleanup --kdub
TEST_F(ShellSurface, destroy)
{
    bool destroy_called = false;
    {
        msh::Surface test(weak_surface,[&](std::weak_ptr<ms::Surface>)
            {
                destroy_called = true;
            });
    }
    EXPECT_TRUE(destroy_called);
}

TEST_F(ShellSurface, destroy_manual)
{
    bool destroy_called = false;
    msh::Surface test(weak_surface,[&](std::weak_ptr<ms::Surface>)
        {
            destroy_called = true;
        });

    test.destroy();
    EXPECT_TRUE(destroy_called);
}

TEST_F(ShellSurface, size_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.size();
    });

    weak_surface.reset();

    EXPECT_THROW({
        test.size();
    }, std::runtime_error);
}

TEST_F(ShellSurface, top_left_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.top_left();
    });

    weak_surface.reset();

    EXPECT_THROW({
        test.top_left();
    }, std::runtime_error);
}

TEST_F(ShellSurface, name_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.name();
    });

    weak_surface.reset();

    EXPECT_THROW({
        test.name();
    }, std::runtime_error);
}

TEST_F(ShellSurface, pixel_format_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.pixel_format();
    });

    weak_surface.reset();

    EXPECT_THROW({
        test.pixel_format();
    }, std::runtime_error);
}

TEST_F(ShellSurface, hide_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.hide();
    });

    weak_surface.reset();

    EXPECT_THROW({
        test.hide();
    }, std::runtime_error);
}

TEST_F(ShellSurface, show_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.show();
    });

    weak_surface.reset();

    EXPECT_THROW({
        test.show();
    }, std::runtime_error);
}

TEST_F(ShellSurface, visible_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.visible();
    });

    weak_surface.reset();

    EXPECT_THROW({
        test.visible();
    }, std::runtime_error);
}

TEST_F(ShellSurface, destroy_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.destroy();
    });

    weak_surface.reset();

    EXPECT_NO_THROW({
        test.destroy();
    });
}

TEST_F(ShellSurface, force_request_to_complete_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.force_requests_to_complete();
    });

    weak_surface.reset();

    EXPECT_NO_THROW({
        test.force_requests_to_complete();
    });
}

TEST_F(ShellSurface, advance_client_buffer_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_NO_THROW({
        test.advance_client_buffer();
    });

    weak_surface.reset();

    EXPECT_THROW({
        test.advance_client_buffer();
    }, std::runtime_error);
}

TEST_F(ShellSurface, input_fds_throw_behavior)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    weak_surface.reset();

    EXPECT_THROW({
            test.server_input_fd();
    }, std::runtime_error);
    EXPECT_THROW({
            test.client_input_fd();
    }, std::runtime_error);
}

TEST_F(ShellSurface, attributes)
{
    using namespace testing;

    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_THROW({
        test.configure(static_cast<MirSurfaceAttrib>(111), 222);
    }, std::logic_error);
}

TEST_F(ShellSurface, types)
{
    using namespace testing;

    msh::Surface surf(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_EQ(mir_surface_type_normal, surf.type());

    EXPECT_EQ(mir_surface_type_utility,
              surf.configure(mir_surface_attrib_type,
                             mir_surface_type_utility));
    EXPECT_EQ(mir_surface_type_utility, surf.type());

    EXPECT_THROW({
        surf.configure(mir_surface_attrib_type, 999);
    }, std::logic_error);
    EXPECT_THROW({
        surf.configure(mir_surface_attrib_type, -1);
    }, std::logic_error);
    EXPECT_EQ(mir_surface_type_utility, surf.type());

    EXPECT_EQ(mir_surface_type_dialog,
              surf.configure(mir_surface_attrib_type,
                             mir_surface_type_dialog));
    EXPECT_EQ(mir_surface_type_dialog, surf.type());

    EXPECT_EQ(mir_surface_type_freestyle,
              surf.configure(mir_surface_attrib_type,
                             mir_surface_type_freestyle));
    EXPECT_EQ(mir_surface_type_freestyle, surf.type());
}

TEST_F(ShellSurface, states)
{
    using namespace testing;

    msh::Surface surf(weak_surface,[](std::weak_ptr<ms::Surface>){});

    EXPECT_EQ(mir_surface_state_restored, surf.state());

    EXPECT_EQ(mir_surface_state_vertmaximized,
              surf.configure(mir_surface_attrib_state,
                             mir_surface_state_vertmaximized));
    EXPECT_EQ(mir_surface_state_vertmaximized, surf.state());

    EXPECT_THROW({
        surf.configure(mir_surface_attrib_state, 999);
    }, std::logic_error);
    EXPECT_THROW({
        surf.configure(mir_surface_attrib_state, -1);
    }, std::logic_error);
    EXPECT_EQ(mir_surface_state_vertmaximized, surf.state());

    EXPECT_EQ(mir_surface_state_minimized,
              surf.configure(mir_surface_attrib_state,
                             mir_surface_state_minimized));
    EXPECT_EQ(mir_surface_state_minimized, surf.state());

    EXPECT_EQ(mir_surface_state_fullscreen,
              surf.configure(mir_surface_attrib_state,
                             mir_surface_state_fullscreen));
    EXPECT_EQ(mir_surface_state_fullscreen, surf.state());
}

TEST_F(ShellSurface, take_input_focus)
{
    using namespace ::testing;

    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});
    
    mtd::MockInputTargeter targeter;
    EXPECT_CALL(targeter, focus_changed(_)).Times(1);
    
    test.take_input_focus(mt::fake_shared(targeter));
}

TEST_F(ShellSurface, take_input_focus_throw_behavior)
{
    using namespace ::testing;

    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});
    weak_surface.reset();

    mtd::StubInputTargeter targeter;
    
    EXPECT_THROW({
            test.take_input_focus(mt::fake_shared(targeter));
    }, std::runtime_error);
}

TEST_F(ShellSurface, with_most_recent_buffer_do_uses_compositor_buffer)
{
    msh::Surface test(weak_surface,[](std::weak_ptr<ms::Surface>){});

    mc::Buffer* buf_ptr{nullptr};

    test.with_most_recent_buffer_do(
        [&](mc::Buffer& buffer)
        {
            buf_ptr = &buffer;
        });

    EXPECT_EQ(stub_buffer_stream->stub_compositor_buffer.get(),
              buf_ptr);
}
