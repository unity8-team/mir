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

#ifndef QPA_MIR_INPUT_CONFIGURATION_H
#define QPA_MIR_INPUT_CONFIGURATION_H

#include <mir/input/input_dispatcher_configuration.h>

#include "qteventfeeder.h"

class MirInputDispatcherConfiguration : public mir::input::InputDispatcherConfiguration
{
public:
    MirInputDispatcherConfiguration();
    virtual ~MirInputDispatcherConfiguration() {}

    std::shared_ptr<mir::shell::InputTargeter> the_input_targeter() override;
    std::shared_ptr<mir::input::InputDispatcher> the_input_dispatcher() override;

    bool is_key_repeat_enabled() const override;

    void set_input_targets(std::shared_ptr<mir::input::InputTargets> const& targets) override;

private:
    std::shared_ptr<QtEventFeeder> mQtEventFeeder;
};

#endif // QPA_MIR_INPUT_CONFIGURATION_H
