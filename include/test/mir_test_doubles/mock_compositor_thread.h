/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#ifndef MIR_TEST_DOUBLES_MOCK_COMPOSITOR_THREAD_H_
#define MIR_TEST_DOUBLES_MOCK_COMPOSITOR_THREAD_H_

#include "src/server/compositor/compositor_thread.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockCompositorThread : public compositor::CompositorThread
{
public:
    MockCompositorThread(std::unique_ptr<compositor::CompositorLoop>& loop)
        : compositor::CompositorThread(std::move(loop))
    {
    }

    ~MockCompositorThread()
    {
        destroyed();
    }

    void run(std::unique_ptr<compositor::CompositorLoop> loop) override
    {
        run_();
        compositor::CompositorThread::run(std::move(loop));
    }

    void pause() override
    {
        pause_();
        compositor::CompositorThread::pause();
    }

    MOCK_METHOD0(run_, void());
    MOCK_METHOD0(pause_, void());
    MOCK_METHOD0(destroyed, void());
};

}
}
}

#endif
