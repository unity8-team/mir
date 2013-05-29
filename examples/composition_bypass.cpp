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

#include "mir/run_mir.h"
#include "mir/report_exception.h"

#include "mir/default_server_configuration.h"
#include "mir/compositor/bypass_compositing_strategy.h"
#include "mir/compositor/buffer_swapper.h"

// TODO this is a frig, but keeps CB code separate for the spike
#include "../src/server/graphics/android/android_platform.h"
#include "../src/server/graphics/android/default_framebuffer_factory.h"
#include "../src/server/graphics/android/fb_swapper.h"

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace mga= mir::graphics::android;

namespace mir
{
namespace graphics
{
namespace android
{
class CompositionBypassSwapper : public FBSwapper, public compositor::BufferSwapper
{
public:
    // TODO

    ~CompositionBypassSwapper() noexcept {}
};

class CompositionBypassAndroidPlatform : public AndroidPlatform
{
public:
    CompositionBypassAndroidPlatform(std::shared_ptr<DisplayReport> const& display_report) :
        AndroidPlatform(display_report) {}

    virtual std::shared_ptr<FramebufferFactory> create_frame_buffer_factory(
        const std::shared_ptr<GraphicBufferAllocator>& buffer_allocator);
};

class CompositionBypassFramebufferFactory : public DefaultFramebufferFactory
{
public:
    explicit CompositionBypassFramebufferFactory(std::shared_ptr<GraphicBufferAllocator> const& buffer_allocator) :
        DefaultFramebufferFactory(buffer_allocator) {}

    std::shared_ptr<ANativeWindow> create_fb_native_window(std::shared_ptr<DisplaySupportProvider> const&) const;
};
}
}
}

namespace
{
class CompositionBypassConfiguration : public mir::DefaultServerConfiguration
{
public:
//    using mir::DefaultServerConfiguration::DefaultServerConfiguration;
    CompositionBypassConfiguration(int argc, char const* argv[]) :
        mir::DefaultServerConfiguration(argc, argv) {}

    std::shared_ptr<mc::CompositingStrategy>
    the_compositing_strategy()
    {
        return compositing_strategy(
            [this]{ return std::make_shared<mc::BypassCompositingStrategy>(); });
    }

    std::shared_ptr<mg::Platform> the_graphics_platform()
    {
        return graphics_platform(
            [this]()
            {
                return std::make_shared<mga::CompositionBypassAndroidPlatform>(the_display_report());
            });
    }
};
}

int main(int argc, char const** argv)
try
{
    CompositionBypassConfiguration conf{argc, argv};

    mir::run_mir(conf, [&](mir::DisplayServer&){});

    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}

///////////////////////////////////////////////

auto mga::CompositionBypassAndroidPlatform::create_frame_buffer_factory(
        const std::shared_ptr<GraphicBufferAllocator>& /*buffer_allocator*/) -> std::shared_ptr<FramebufferFactory>
{
    // TODO here we need a bespoke "CompositionBypass" FramebufferFactory
    return {};
}

auto mga::CompositionBypassFramebufferFactory::create_fb_native_window(std::shared_ptr<DisplaySupportProvider> const&) const
    -> std::shared_ptr<ANativeWindow>
{
    // TODO create a "CompositionBypass" Swapper : public FBSwapper, public BufferSwapper
    return {};
}
