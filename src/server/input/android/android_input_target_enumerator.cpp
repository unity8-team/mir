/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "android_input_target_enumerator.h"

#include "android_window_handle_repository.h"

#include "mir/input/input_targets.h"

#include <InputWindow.h>

namespace mi = mir::input;
namespace mia = mi::android;

mia::InputTargetEnumerator::InputTargetEnumerator(std::shared_ptr<mi::InputTargets> const& targets,
                                                  std::shared_ptr<mia::WindowHandleRepository> const& repository)
    : targets(targets),
      repository(repository)
{
}

mia::InputTargetEnumerator::~InputTargetEnumerator() noexcept(true)
{
}

void mia::InputTargetEnumerator::for_each(std::function<void(droidinput::sp<droidinput::InputWindowHandle> const&)> const& callback)
{
    auto t = targets.lock();
    auto r = repository.lock();
    t->for_each([&callback, &r, this](std::shared_ptr<mi::SurfaceTarget> const& target){
            auto handle = r->handle_for_surface(target);
            callback(handle);
    });
}
