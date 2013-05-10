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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_GBM_INTERNAL_CLIENT_H_
#define MIR_GRAPHICS_GBM_INTERNAL_CLIENT_H_

#include "mir/graphics/internal_client.h"
#include "internal_native_display.h"
#include <memory>

namespace mir
{
namespace frontend
{
class Surface;
}
namespace graphics
{
class Platform;
namespace gbm
{

class InternalClient : public mir::graphics::InternalClient
{
public:
    InternalClient(std::shared_ptr<MirMesaEGLNativeDisplay> const&,
                   std::shared_ptr<frontend::Surface> const&);
    EGLNativeDisplayType egl_native_display();
    EGLNativeWindowType egl_native_window();

private:
    std::shared_ptr<MirMesaEGLNativeDisplay> const native_display;
    std::shared_ptr<frontend::Surface> const surface;
};

}
}
}

#endif /* MIR_GRAPHICS_GBM_INTERNAL_CLIENT_H_ */
