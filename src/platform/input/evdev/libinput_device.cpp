/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "libinput_device.h"
#include "libinput_wrapper.h"

#include "mir/input/input_event_handler_register.h"
#include <libinput.h>

namespace mie = mir::input::evdev;

mie::LibInputDevice::LibInputDevice(std::shared_ptr<mie::LibInputWrapper> const& lib, char const* path)
    : path(path), dev(nullptr,&libinput_device_unref), lib(lib)
{
}

mie::LibInputDevice::~LibInputDevice() = default;

void mie::LibInputDevice::start(InputEventHandlerRegister& registry, EventSink& sink)
{
    dev = lib->add_device(path);
    lib->start_device(registry, dev.get(), sink);
}

void mie::LibInputDevice::stop(InputEventHandlerRegister& registry)
{
    lib->stop_device(registry, dev.get());
    dev.reset();
}
