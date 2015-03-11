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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_SCENE_SURFACE_OBSERVER_H_
#define MIR_SCENE_SURFACE_OBSERVER_H_

namespace mir { namespace scene {

class Surface;

class SurfaceObserver
{
public:
    enum Change
    {
        type,
        state,
        swapinterval,
        focus,
        dpi,
        size,
        position,
        visibility,
        content,
        opacity,
        orientation,
        pref_orientation,
        transformation,
        input_mode,
        cursor,
        close,
        keymap
    };

    virtual void surface_changed(Surface const&, Change) = 0;

protected:
    SurfaceObserver() = default;
    virtual ~SurfaceObserver() = default;
    SurfaceObserver(SurfaceObserver const&) = delete;
    SurfaceObserver& operator=(SurfaceObserver const&) = delete;
};

} } // namespace mir::scene

#endif // MIR_SCENE_SURFACE_OBSERVER_H_
