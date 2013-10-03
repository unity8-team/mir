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

#include "android_input_injecter.h"
#include "android_input_registrar.h"

#include "android_input_window_handle.h"
#include "android_input_application_handle.h"

#include "mir/input/android/android_input_lexicon.h"

#include <InputDispatcher.h>

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <mutex>

namespace mi = mir::input;
namespace mia = mi::android;
namespace ms = mir::surfaces;

mia::InputInjecter::InputInjecter(droidinput::sp<droidinput::InputDispatcherInterface> const& input_dispatcher,
                                  std::shared_ptr<mia::WindowHandleRepository> const& repository) :
    input_dispatcher(input_dispatcher),
    repository(repository)
{
}

mia::InputInjecter::~InputInjecter() noexcept(true) {}

void mia::InputInjecter::inject_input(std::shared_ptr<mi::InputChannel const> const& focus_channel,
                                      MirEvent const& ev)
{
    auto window_handle = repository->handle_for_channel(focus_channel);
    
    if (window_handle == NULL)
        BOOST_THROW_EXCEPTION(std::logic_error("Attempt to inject input to an unregistered input channel"));

    droidinput::InputEvent *android_event;

    mia::Lexicon::translate(ev, &android_event);
    input_dispatcher->injectEventToWindow(window_handle, android_event);

    delete android_event;
}
