/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "touch_stream_rewriter.h"

namespace mi = mir::input;

mi::TouchStreamRewriter::TouchStreamRewriter(std::shared_ptr<mi::InputDispatcher> const& next_dispatcher)
    : next_dispatcher(next_dispatcher)
{
}

void mi::TouchStreamRewriter::dispatch(MirEvent const& event)
{
    (void) event;
}

void mi::TouchStreamRewriter::start()
{
}

void mi::TouchStreamRewriter::stop()
{
}
