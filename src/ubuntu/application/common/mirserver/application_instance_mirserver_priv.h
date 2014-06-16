/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_INSTANCE_MIRSERVER_PRIV_H_
#define UBUNTU_APPLICATION_INSTANCE_MIRSERVER_PRIV_H_

#include <ubuntu/application/instance.h>

#include <memory>
#include <functional>

namespace mir
{
namespace scene
{
class Surface;
class SurfaceCoordinator;
class SessionListener;
class SurfaceCreationParameters;
}
}

namespace ubuntu
{
namespace application
{
namespace mir
{
class Description;
class Options;

namespace server
{

class Instance
{
public:
    Instance(std::shared_ptr< ::mir::scene::SurfaceCoordinator> const& surface_coordinator,
             std::shared_ptr< ::mir::scene::SessionListener> const& session_listener,
             ubuntu::application::mir::Description* description, 
             ubuntu::application::mir::Options *options);
    ~Instance() = default;
    
    UApplicationInstance* as_u_application_instance();
    static Instance* from_u_application_instance(UApplicationInstance* u_instance);
    
    void ref();
    void unref();
    
    std::shared_ptr< ::mir::scene::Surface> create_surface( ::mir::scene::SurfaceCreationParameters const& parameters);
    
protected:
    Instance(Instance const&) = delete;
    Instance& operator=(Instance const&) = delete;

private:
    typedef std::unique_ptr<Description, std::function<void(Description*)>> DescriptionPtr;
    typedef std::unique_ptr<Options, std::function<void(Options*)>> OptionsPtr;
    
    OptionsPtr options;
    DescriptionPtr description;

    std::shared_ptr< ::mir::scene::SurfaceCoordinator> const surface_coordinator;
    std::shared_ptr< ::mir::scene::SessionListener> const session_listener;

    int ref_count;
};

}
}
}
} // namespace ubuntu

#endif // UBUNTU_APPLICATION_INSTANCE_MIRSERVER_PRIV_H_
