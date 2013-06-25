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
 * Authored By: Robert Carr <racarr@canonical.com>
 */

#ifndef MIR_SHELL_APPLICATION_SESSION_H_
#define MIR_SHELL_APPLICATION_SESSION_H_

#include "mir/shell/session.h"

#include <map>

namespace mir
{
namespace events
{
class EventSink;
}
namespace surfaces
{
class Surface;
}
namespace shell
{
class SurfaceFactory;
class Surface;

class ApplicationSession : public Session
{
public:
    /* why two constructors? */
    explicit ApplicationSession(std::string const& session_name);
    ApplicationSession(
        std::string const& session_name,
        std::shared_ptr<events::EventSink> const& sink);

    ~ApplicationSession();

    /* hodge-podge */
    frontend::SurfaceId associate_surface(std::weak_ptr<surfaces::Surface> const& surface,
                                          std::shared_ptr<shell::Surface> const& shell_surface);
    void disassociate_surface(frontend::SurfaceId surface);

    //triggers state change in ms::Surface
    void hide();
    void show();

    //accesses shell surfaces
    std::shared_ptr<frontend::Surface> get_surface(frontend::SurfaceId surface) const;
    int configure_surface(frontend::SurfaceId id, MirSurfaceAttrib attrib, int value);

    /* msh::Session */
    std::string name() const;
    void force_requests_to_complete();
    std::shared_ptr<Surface> default_surface() const;

protected:
    ApplicationSession(ApplicationSession const&) = delete;
    ApplicationSession& operator=(ApplicationSession const&) = delete;

private:
//    std::shared_ptr<SurfaceFactory> const surface_factory;
    std::string const session_name;
    std::shared_ptr<events::EventSink> const event_sink;

    typedef std::pair<std::shared_ptr<shell::Surface>,
                      std::weak_ptr<surfaces::Surface>> SurfaceAssociation; 
    typedef std::map<frontend::SurfaceId, SurfaceAssociation> Surfaces;
    std::mutex mutable surfaces_mutex;
    Surfaces::const_iterator checked_find(frontend::SurfaceId id) const;
    Surfaces surfaces;
    int next_surface_id;
};

}
} // namespace mir

#endif // MIR_SHELL_APPLICATION_SESSION_H_
