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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_FRAMEBUFFER_FACTORY_H_
#define MIR_GRAPHICS_ANDROID_FRAMEBUFFER_FACTORY_H_

#include <system/window.h>
#include <memory>

namespace mir
{
namespace graphics
{
namespace android
{
class DisplaySupportProvider;

class FramebufferFactory
{
public:
    virtual ~FramebufferFactory() = default;

    virtual std::shared_ptr<ANativeWindow> create_fb_native_window(std::shared_ptr<DisplaySupportProvider> const&) const = 0;
    virtual std::shared_ptr<DisplaySupportProvider> create_fb_device() const = 0; 

protected:
    FramebufferFactory() = default;
    FramebufferFactory& operator=(FramebufferFactory const&) = delete;
    FramebufferFactory(FramebufferFactory const&) = delete;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_FRAMEBUFFER_FACTORY_H_ */
