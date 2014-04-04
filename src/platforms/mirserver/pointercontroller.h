/*
 * Copyright © 2014 Canonical Ltd.
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
 *
 * Authored by: Gerry Boland <gerry.boland@canonical.com>
 */

#ifndef POINTERCONTROLLER_H
#define POINTERCONTROLLER_H

#include <android-input/android/frameworks/base/services/input/PointerController.h>

#include <mir/input/cursor_listener.h>
#include <mir/input/input_region.h>

#include <memory>
#include <mutex>

namespace unitymir {

class PointerController : public android::PointerControllerInterface
{
public:
    explicit PointerController(const std::shared_ptr<mir::input::InputRegion> &input_region);
    explicit PointerController(const std::shared_ptr<mir::input::InputRegion> &input_region,
                               const std::shared_ptr<mir::input::CursorListener> &cursor_listener);

    virtual bool getBounds(float* out_min_x, float* out_min_y, float* out_max_x, float* out_max_y) const;
    virtual void move(float delta_x, float delta_y);
    virtual void setButtonState(int32_t button_state);
    virtual int32_t getButtonState() const;
    virtual void setPosition(float x, float y);
    virtual void getPosition(float *out_x, float *out_y) const;

    virtual void fade(Transition transition)
    {
        (void)transition;
    }
    virtual void unfade(Transition transition)
    {
        (void)transition;
    }

    virtual void setPresentation(Presentation presentation)
    {
        (void)presentation;
    }
    virtual void setSpots(const android::PointerCoords* spot_coords, uint32_t spot_count)
    {
        (void)spot_coords;
        (void)spot_count;
    }
    virtual void clearSpots()
    {
    }


private:
    bool get_bounds_locked(float *out_min_x, float* out_min_y, float* out_max_x, float* out_max_y) const;
    void notify_listener();
    // Could be a read/write mutex as this is a latency sensitive class.
    mutable std::mutex guard;
    int32_t state;
    float x, y;

    const std::shared_ptr<mir::input::InputRegion> m_inputRegion;
    const std::shared_ptr<mir::input::CursorListener> m_cursorListener;
};

} // unity-mir
#endif // POINTERCONTROLLER_H
