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
 * Authored by:
 *   Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_SERVER_RENDER_WINDOW_H_
#define MIR_GRAPHICS_ANDROID_SERVER_RENDER_WINDOW_H_

#include "mir/graphics/android/android_driver_interpreter.h"
#include "mir/geometry/pixel_format.h"

#include <hardware/fb.h>
#include <hardware/hwcomposer.h>

#include <memory>
#include <unordered_map>

namespace mir
{
namespace compositor
{
class Buffer;
}
namespace graphics
{
namespace android
{

class FBSwapper;
class DisplaySupportProvider;
class InterpreterResourceCache;
class ServerRenderWindow : public AndroidDriverInterpreter 
{
public:
    ServerRenderWindow(std::shared_ptr<FBSwapper> const& swapper,
                       std::shared_ptr<DisplaySupportProvider> const& display_poster,
                       std::shared_ptr<InterpreterResourceCache> const&);

    ANativeWindowBuffer* driver_requests_buffer();
    void driver_returns_buffer(ANativeWindowBuffer*, std::shared_ptr<SyncObject> const&);
    void dispatch_driver_request_format(int format);
    int driver_requests_info(int key) const;

private:
    std::shared_ptr<FBSwapper> const swapper;
    std::shared_ptr<DisplaySupportProvider> const poster;
    std::shared_ptr<InterpreterResourceCache> const resource_cache;

    int format;
}; 

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_SERVER_RENDER_WINDOW_H_ */
