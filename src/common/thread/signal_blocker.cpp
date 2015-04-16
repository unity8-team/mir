/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored By: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir/signal_blocker.h"

#include <system_error>
#include <signal.h>
#include <boost/exception/all.hpp>

mir::SignalBlocker::SignalBlocker()
{
    sigset_t all_signals;
    sigfillset(&all_signals);

    if (auto error = pthread_sigmask(SIG_BLOCK, &all_signals, &previous_set))
        BOOST_THROW_EXCEPTION((
                    std::system_error{error,
                                      std::system_category(),
                                      "Failed to block signals"}));
}

mir::SignalBlocker::~SignalBlocker() noexcept(false)
{
    if (auto error = pthread_sigmask(SIG_SETMASK, &previous_set, nullptr))
    {
        if (!std::uncaught_exception())
        {
            BOOST_THROW_EXCEPTION((std::system_error{error,
                                                     std::system_category(),
                                                     "Failed to restore signal mask"}));
        }
    }
}
