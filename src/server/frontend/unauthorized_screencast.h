/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_FRONTEND_UNAUTHORIZED_SCREENCAST_H_
#define MIR_FRONTEND_UNAUTHORIZED_SCREENCAST_H_

#include "mir/graphics/shared_ptr_buffer.h"
#include "mir/frontend/screencast.h"

namespace mir
{
namespace frontend
{

class UnauthorizedScreencast : public Screencast
{
public:
    ScreencastSessionId create_session(
        geometry::Rectangle const& region,
        geometry::Size const& size,
        MirPixelFormat pixel_format) override;
    void destroy_session(frontend::ScreencastSessionId id) override;
    std::shared_ptr<graphics::Buffer> capture(frontend::ScreencastSessionId id) override;
};

}
}

#endif /* MIR_FRONTEND_UNAUTHORIZED_SCREENCAST_H_ */

