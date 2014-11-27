/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/graphics/android/sync_fence.h"
#include "platform.h"
#include "android_graphic_buffer_allocator.h"
#include "resource_factory.h"
#include "display.h"
#include "internal_client.h"
#include "output_builder.h"
#include "buffer_writer.h"
#include "hwc_loggers.h"
#include "ipc_operations.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir/graphics/buffer_ipc_message.h"
#include "mir/graphics/android/native_buffer.h"
#include "mir/graphics/buffer_id.h"
#include "mir/graphics/display_report.h"
#include "mir/options/option.h"
#include "mir/options/configuration.h"
#include "mir/abnormal_exit.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <mutex>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mf=mir::frontend;
namespace mo = mir::options;

namespace
{

char const* const hwc_log_opt = "hwc-report";
char const* const hwc_overlay_opt = "disable-overlays";

std::shared_ptr<mga::HwcLogger> make_logger(mo::Option const& options)
{
    if (!options.is_set(hwc_log_opt))
        return std::make_shared<mga::NullHwcLogger>();

    auto opt = options.get<std::string>(hwc_log_opt);
    if (opt == mo::log_opt_value)
        return std::make_shared<mga::HwcFormattedLogger>();
    else if (opt == mo::off_opt_value)
        return std::make_shared<mga::NullHwcLogger>();
    else
        throw mir::AbnormalExit(
            std::string("Invalid hwc-report option: " + opt + " (valid options are: \"" +
            mo::off_opt_value + "\" and \"" + mo::log_opt_value + "\")"));
}

mga::OverlayOptimization should_use_overlay_optimization(mo::Option const& options)
{
    if (!options.is_set(hwc_overlay_opt))
        return mga::OverlayOptimization::enabled;

    if (options.get<bool>(hwc_overlay_opt))
        return mga::OverlayOptimization::disabled;
    else
        return mga::OverlayOptimization::enabled;
}

}

mga::Platform::Platform(
    std::shared_ptr<mga::DisplayBuilder> const& display_builder,
    std::shared_ptr<mg::DisplayReport> const& display_report)
    : display_builder(display_builder),
      display_report(display_report),
      ipc_operations(std::make_shared<mga::IpcOperations>())
{
}

std::shared_ptr<mg::GraphicBufferAllocator> mga::Platform::create_buffer_allocator()
{
    if (quirks.gralloc_reopenable_after_close())
    {
        return std::make_shared<mga::AndroidGraphicBufferAllocator>();
    }
    else
    {
        //LP: 1371619. Some devices cannot call gralloc's open()/close() function repeatedly without crashing
        static std::mutex allocator_mutex;
        std::unique_lock<std::mutex> lk(allocator_mutex);
        static std::shared_ptr<mg::GraphicBufferAllocator> preserved_allocator;
        if (!preserved_allocator)
            preserved_allocator = std::make_shared<mga::AndroidGraphicBufferAllocator>();
        return preserved_allocator;
    }
}

std::shared_ptr<mga::GraphicBufferAllocator> mga::Platform::create_mga_buffer_allocator()
{
    return std::make_shared<mga::AndroidGraphicBufferAllocator>();
}

std::shared_ptr<mg::Display> mga::Platform::create_display(
    std::shared_ptr<mg::DisplayConfigurationPolicy> const&,
    std::shared_ptr<mg::GLProgramFactory> const& gl_program_factory,
    std::shared_ptr<mg::GLConfig> const& gl_config)
{
    return std::make_shared<mga::Display>(
        display_builder, gl_program_factory, gl_config, display_report);
}

std::shared_ptr<mg::PlatformIpcOperations> mga::Platform::make_ipc_operations() const
{
    return ipc_operations;
}

EGLNativeDisplayType mga::Platform::egl_native_display() const
{
    return EGL_DEFAULT_DISPLAY;
}

std::shared_ptr<mg::InternalClient> mga::Platform::create_internal_client()
{
    return std::make_shared<mga::InternalClient>();
}

std::shared_ptr<mg::BufferWriter> mga::Platform::make_buffer_writer()
{
    return std::make_shared<mga::BufferWriter>();
}

extern "C" std::shared_ptr<mg::Platform> mg::create_platform(
    std::shared_ptr<mo::Option> const& options,
    std::shared_ptr<mir::EmergencyCleanupRegistry> const& /*emergency_cleanup_registry*/,
    std::shared_ptr<DisplayReport> const& display_report)
{
    auto logger = make_logger(*options);
    auto overlay_option = should_use_overlay_optimization(*options);
    logger->log_overlay_optimization(overlay_option);
    auto display_resource_factory = std::make_shared<mga::ResourceFactory>();
    auto fb_allocator = std::make_shared<mga::AndroidGraphicBufferAllocator>();
    auto display_builder = std::make_shared<mga::OutputBuilder>(
        fb_allocator, display_resource_factory, display_report, overlay_option, logger);
    return std::make_shared<mga::Platform>(display_builder, display_report);
}

extern "C" std::shared_ptr<mg::NativePlatform> create_native_platform(
    std::shared_ptr<mg::DisplayReport> const& display_report,
    std::shared_ptr<mg::NestedContext> const&)
{
    //TODO: remove nullptr parameter once platform classes are sorted.
    //      mg::NativePlatform cannot create a display anyways, so it doesnt need a  display builder
    return std::make_shared<mga::Platform>(nullptr, display_report);
}

extern "C" void add_platform_options(
    boost::program_options::options_description& config)
{
    config.add_options()
        (hwc_log_opt,
         boost::program_options::value<std::string>()->default_value(std::string{mo::off_opt_value}),
         "[platform-specific] How to handle the HWC logging report. [{log,off}]")
        (hwc_overlay_opt,
         boost::program_options::value<bool>()->default_value(false),
         "[platform-specific] Whether to disable overlay optimizations [{on,off}]");
}

extern "C" mg::PlatformPriority probe_platform()
{
    int err;
    hw_module_t const* hw_module;

    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &hw_module);

    return err < 0 ? mg::PlatformPriority::unsupported : mg::PlatformPriority::best;
}

mg::ModuleProperties const description = {
    "android"
};

extern "C" mg::ModuleProperties const* describe_module()
{
    return &description;
}
