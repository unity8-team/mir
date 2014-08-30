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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#ifndef MIR_COMPOSITOR_COMPOSITOR_THREAD_FACTORY_H_
#define MIR_COMPOSITOR_COMPOSITOR_THREAD_FACTORY_H_

#include <memory>

namespace mir
{
namespace compositor
{
class CompositorThread;
class CompositorLoop;
class CompositorThreadFactory
{
public:
    CompositorThreadFactory();
    virtual ~CompositorThreadFactory();

    virtual std::unique_ptr<CompositorThread> create_compositor_thread_for(
            std::unique_ptr<CompositorLoop> loop);
};

}
}


#endif
