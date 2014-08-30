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

#include "compositor_thread_factory.h"
#include "compositor_thread.h"

namespace mc = mir::compositor;

mc::CompositorThreadFactory::CompositorThreadFactory() = default;
mc::CompositorThreadFactory::~CompositorThreadFactory() = default;

std::unique_ptr<mc::CompositorThread>
mc::CompositorThreadFactory::create_compositor_thread_for(std::unique_ptr<mc::CompositorLoop> loop)
{
    std::unique_ptr<CompositorThread> compositor_thread{new CompositorThread(std::move(loop))};
    return compositor_thread;
}
