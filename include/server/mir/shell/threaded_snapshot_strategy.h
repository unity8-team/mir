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
 * Authored By: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_SHELL_THREADED_SNAPSHOT_STRATEGY_H_
#define MIR_SHELL_THREADED_SNAPSHOT_STRATEGY_H_

#include "mir/shell/snapshot_strategy.h"

#include <memory>
#include <thread>
#include <functional>

namespace mir
{
namespace graphics
{
class PixelBuffer;
}

namespace shell
{
class SnapshottingFunctor;

class ThreadedSnapshotStrategy : public SnapshotStrategy
{
public:
    ThreadedSnapshotStrategy(std::shared_ptr<graphics::PixelBuffer> const& pixels);
    ~ThreadedSnapshotStrategy() noexcept;

    void take_snapshot_of(
        std::shared_ptr<SurfaceBufferAccess> const& surface_buffer_access,
        SnapshotCallback const& snapshot_taken);

private:
    std::shared_ptr<graphics::PixelBuffer> const pixels;
    std::unique_ptr<SnapshottingFunctor> functor;
    std::thread thread;
};

}
}

#endif /* MIR_SHELL_THREADED_SNAPSHOT_STRATEGY_H_ */
