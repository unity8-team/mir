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

#include "mir/compositor/buffer_allocation_strategy.h"

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
class CompositionBypassSwapper : public FBSwapper, public mc::BufferSwapper
{
public:
    explicit CompositionBypassSwapper(std::vector<std::shared_ptr<compositor::Buffer>> const& buffers);

    std::shared_ptr<mc::Buffer> client_acquire() override;

    void client_release(std::shared_ptr<mc::Buffer> const& queued_buffer) override;

    std::shared_ptr<mc::Buffer> compositor_acquire() override;

    void compositor_release(std::shared_ptr<mc::Buffer> const& released_buffer) override;

    void force_requests_to_complete() override;

    ~CompositionBypassSwapper() noexcept {}
};

typedef mir::CachedPtr<CompositionBypassSwapper> CompositionBypassSwapperCache;

class CompositionBypassAndroidPlatform : public AndroidPlatform
{
public:
    CompositionBypassAndroidPlatform(
        std::shared_ptr<DisplayReport> const& display_report,
        CompositionBypassSwapperCache& composition_bypass_swapper) :
        AndroidPlatform(display_report),
        composition_bypass_swapper(composition_bypass_swapper) {}

    virtual std::shared_ptr<FramebufferFactory> create_frame_buffer_factory(
        const std::shared_ptr<GraphicBufferAllocator>& buffer_allocator);

private:
    CompositionBypassSwapperCache& composition_bypass_swapper;
};

class CompositionBypassFramebufferFactory : public DefaultFramebufferFactory
{
public:
    explicit CompositionBypassFramebufferFactory(
        std::shared_ptr<GraphicBufferAllocator> const& buffer_allocator,
        CompositionBypassSwapperCache& composition_bypass_swapper) :
        DefaultFramebufferFactory(buffer_allocator),
        composition_bypass_swapper(composition_bypass_swapper) {}

private:
//    virtual std::vector<std::shared_ptr<compositor::Buffer>> create_buffers(
//        std::shared_ptr<DisplaySupportProvider> const& info_provider) const;

    virtual std::shared_ptr<FBSwapper> create_swapper(
        std::vector<std::shared_ptr<compositor::Buffer>> const& buffers) const;

    CompositionBypassSwapperCache& composition_bypass_swapper;
};
}
}

namespace compositor
{
class CompositionBypassSwapperFactory : public BufferAllocationStrategy
{
public:

    explicit CompositionBypassSwapperFactory(
        mga::CompositionBypassSwapperCache& composition_bypass_swapper);

    std::unique_ptr<BufferSwapper> create_swapper(
        BufferProperties& actual_buffer_properties,
        BufferProperties const& requested_buffer_properties);

private:
    mga::CompositionBypassSwapperCache& composition_bypass_swapper;
};
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
                return std::make_shared<mga::CompositionBypassAndroidPlatform>(
                    the_display_report(),
                    composition_bypass_swapper);
            });
    }

    auto the_buffer_allocation_strategy() -> std::shared_ptr<mc::BufferAllocationStrategy>
    {
        return buffer_allocation_strategy(
            [this]()
            {
                 return std::make_shared<mc::CompositionBypassSwapperFactory>(composition_bypass_swapper);
            });
    }


    mga::CompositionBypassSwapperCache composition_bypass_swapper;
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

mga::CompositionBypassSwapper::CompositionBypassSwapper(
    std::vector<std::shared_ptr<compositor::Buffer>> const& /*buffers*/)
{
    // TODO
}

auto mga::CompositionBypassSwapper::client_acquire() -> std::shared_ptr<mc::Buffer>
{
    // TODO
    return {};
}

void mga::CompositionBypassSwapper::client_release(std::shared_ptr<mc::Buffer> const& /*queued_buffer*/)
{
    // TODO
}

auto mga::CompositionBypassSwapper::compositor_acquire() -> std::shared_ptr<mc::Buffer>
{
    // TODO
    return {};
}



void mga::CompositionBypassSwapper::compositor_release(std::shared_ptr<mc::Buffer> const& /*released_buffer*/)
{
    // TODO
}

void mga::CompositionBypassSwapper::force_requests_to_complete()
{
    // TODO
}


auto mga::CompositionBypassAndroidPlatform::create_frame_buffer_factory(
        const std::shared_ptr<GraphicBufferAllocator>& buffer_allocator) -> std::shared_ptr<FramebufferFactory>
{
    return std::make_shared<CompositionBypassFramebufferFactory>(
        buffer_allocator,
        composition_bypass_swapper);
}

//auto mga::CompositionBypassFramebufferFactory::create_buffers(
//    std::shared_ptr<DisplaySupportProvider> const& info_provider) const
//    -> std::vector<std::shared_ptr<compositor::Buffer>>
//{
//    // TODO - does this actually need customizing after all?
//    return DefaultFramebufferFactory::create_buffers(info_provider);
//}

auto mga::CompositionBypassFramebufferFactory::create_swapper(
    std::vector<std::shared_ptr<compositor::Buffer>> const& buffers) const
    -> std::shared_ptr<FBSwapper>
{
    return composition_bypass_swapper(
        [&buffers]
        {
            return std::make_shared<mga::CompositionBypassSwapper>(buffers);
        });
}


mc::CompositionBypassSwapperFactory::CompositionBypassSwapperFactory(
    mga::CompositionBypassSwapperCache& composition_bypass_swapper) :
    composition_bypass_swapper(composition_bypass_swapper)
{
}

auto mc::CompositionBypassSwapperFactory::create_swapper(
    BufferProperties& /*actual_buffer_properties*/,
    BufferProperties const& /*requested_buffer_properties*/) -> std::unique_ptr<BufferSwapper>
{
    auto sp = composition_bypass_swapper(
        []() -> std::shared_ptr<mga::CompositionBypassSwapper>
        {
            throw std::logic_error("This can't happen");
        });

    struct ProxyBufferSwapper : BufferSwapper
    {
        ProxyBufferSwapper(decltype(sp) self) : self(self) {}

        std::shared_ptr<mc::Buffer> client_acquire() override
            { return self->client_acquire(); }

        void client_release(std::shared_ptr<mc::Buffer> const& queued_buffer) override
            { return self->client_release(queued_buffer); }

        std::shared_ptr<mc::Buffer> compositor_acquire() override
            { return self->compositor_acquire(); }

        void compositor_release(std::shared_ptr<mc::Buffer> const& released_buffer) override
            { return self->compositor_release(released_buffer); }

        void force_requests_to_complete() override
            { return self->force_requests_to_complete(); }

        ~ProxyBufferSwapper() noexcept {}
        decltype(sp) self;
    };

    return std::unique_ptr<BufferSwapper>(new ProxyBufferSwapper(sp));
}
