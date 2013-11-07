/*
 * Copyright (C) 2013 Canonical, Ltd.
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

#ifndef INITIALSURFACEPLACEMENTSTRATEGY_H
#define INITIALSURFACEPLACEMENTSTRATEGY_H

#include <memory>

#include "mirserver/mir/shell/placement_strategy.h"
#include "mirserver/mir/shell/display_layout.h"

class InitialSurfacePlacementStrategy : public mir::shell::PlacementStrategy
{
public:
    InitialSurfacePlacementStrategy(std::shared_ptr<mir::shell::DisplayLayout> const& displayLayout);

    mir::shell::SurfaceCreationParameters place(mir::shell::Session const& session,
                                                mir::shell::SurfaceCreationParameters const& requestParameters) override;

private:
    std::shared_ptr<mir::shell::DisplayLayout> const m_displayLayout;
};

#endif // INITIALSURFACEPLACEMENTSTRATEGY_H
