/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Daniel d'Andrada <daniel.dandrada@canonical.com>
 */

#include "mirinputdispatcherconfiguration.h"

using namespace mir;

MirInputDispatcherConfiguration::MirInputDispatcherConfiguration()
    : mQtEventFeeder(std::make_shared<QtEventFeeder>())
{
}

std::shared_ptr<scene::InputRegistrar> MirInputDispatcherConfiguration::the_input_registrar()
{
    return std::static_pointer_cast<scene::InputRegistrar>(mQtEventFeeder);
}

std::shared_ptr<shell::InputTargeter> MirInputDispatcherConfiguration::the_input_targeter()
{
    return std::static_pointer_cast<shell::InputTargeter>(mQtEventFeeder);
}

std::shared_ptr<input::InputDispatcher> MirInputDispatcherConfiguration::the_input_dispatcher()
{
    return std::static_pointer_cast<input::InputDispatcher>(mQtEventFeeder);
}

bool MirInputDispatcherConfiguration::is_key_repeat_enabled() const
{
    // whatever...
    return true;
}

void MirInputDispatcherConfiguration::set_input_targets(std::shared_ptr<input::InputTargets> const& targets)
{
    // Ignore it. That kind of stuff is decided inside the QML scene
    (void)targets;
}
