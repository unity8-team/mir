/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GRAPHICS_NESTED_CONTEXT_H_
#define MIR_GRAPHICS_NESTED_CONTEXT_H_

#include <vector>

struct gbm_device;

namespace mir
{
namespace graphics
{

class GuestContext
{
public:
    virtual ~GuestContext() = default;

    virtual std::vector<int> platform_fd_items() = 0;
    virtual void drm_auth_magic(int magic) = 0;
    virtual void drm_set_gbm_device(struct gbm_device* dev) = 0;

protected:
    GuestContext() = default;
    GuestContext(GuestContext const&) = delete;
    GuestContext& operator=(GuestContext const&) = delete;
};

}
}

#endif /* MIR_GRAPHICS_NESTED_CONTEXT_H_ */
