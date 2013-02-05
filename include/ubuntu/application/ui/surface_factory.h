/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_APPLICATION_UI_SURFACE_FACTORY_H_
#define UBUNTU_APPLICATION_UI_SURFACE_FACTORY_H_

#include "ubuntu/platform/shared_ptr.h"

namespace ubuntu
{
namespace application
{
namespace ui
{
class SurfaceProperties;

class SurfaceFactory : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<SurfaceFactory> Ptr;

    static const Ptr& instance();

    virtual Surface::Ptr create_surface(
        const ubuntu::application::ui::SurfaceProperties& props,
        const ubuntu::application::ui::input::Listener::Ptr& listener) = 0;

protected:
    SurfaceFactory() {}
    virtual ~SurfaceFactory() {}

    SurfaceFactory(const SurfaceFactory&) = delete;
    SurfaceFactory& operator=(const SurfaceFactory&) = delete;
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_SURFACE_FACTORY_H_
