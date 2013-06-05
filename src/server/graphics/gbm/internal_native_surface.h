/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_GBM_INTERNAL_NATIVE_SURFACE_H_
#define MIR_GRAPHICS_GBM_INTERNAL_NATIVE_SURFACE_H_

#include "mir_toolkit/mesa/native_display.h"
#include <memory>

namespace mir
{
namespace frontend
{
class Surface;
}
namespace graphics
{
namespace gbm
{

class InternalNativeSurface : public MirMesaEGLNativeSurface
{
public:
    InternalNativeSurface(std::shared_ptr<frontend::Surface> const& surface);

    static void native_display_surface_advance_buffer(MirEGLNativeWindowType surface,
                                                      MirBufferPackage* package);
    static void native_display_surface_get_parameters(MirEGLNativeWindowType surface,
                                                      MirSurfaceParameters* parameters);
};

}
}
}
#endif /* MIR_GRAPHICS_GBM_INTERNAL_NATIVE_DISPLAY_H_ */
