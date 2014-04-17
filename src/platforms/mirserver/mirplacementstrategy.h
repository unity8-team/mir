/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MIRSERVERQPA_MIR_PLACEMENT_STRATEGY_H
#define MIRSERVERQPA_MIR_PLACEMENT_STRATEGY_H

#include <mir/scene/placement_strategy.h>

#include <memory>

namespace mir {
    namespace shell {
        class DisplayLayout;
    }
}

class MirPlacementStrategy : public mir::scene::PlacementStrategy
{
public:
    MirPlacementStrategy(std::shared_ptr<mir::shell::DisplayLayout> const& display_layout);

    mir::scene::SurfaceCreationParameters place(mir::scene::Session const& session,
            mir::scene::SurfaceCreationParameters const& request_parameters) override;

private:
    std::shared_ptr<mir::shell::DisplayLayout> const m_displayLayout;
};

#endif //  MIRSERVERQPA_MIR_PLACEMENT_STRATEGY_H
