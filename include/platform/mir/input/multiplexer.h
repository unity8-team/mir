/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_MULTIPLEXER_H_
#define MIR_INPUT_MULTIPLEXER_H_

#include <functional>
#include <initializer_list>

namespace mir
{
namespace input
{

/**
 * Multiplexer is an interface to register handlers for different
 * events. All handlers are guaranteed to be executed in the same
 * thread.
 */
class Multiplexer
{
public:
    Multiplexer() = default;
    virtual ~Multiplexer() = default;
    virtual void register_fd_handler(
        std::initializer_list<int> fds,
        void const* owner,
        std::function<void(int)> const&& handler) = 0;

    virtual void unregister_fd_handler(void const* owner) = 0;

    virtual void enqueue_action(std::function<void()> const&& action) = 0;

protected:
    Multiplexer(Multiplexer const&) = delete;
    Multiplexer& operator=(Multiplexer const&) = delete;

};
}
}

#endif

