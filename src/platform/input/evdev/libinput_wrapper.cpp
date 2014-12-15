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

#include "libinput_wrapper.h"
#include "libinput_device.h"
#include "libinput.h"

#include "mir/input/input_event_handler_register.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

namespace mie = mir::input::evdev;

namespace
{
int fd_open(const char* path, int flags, void* /*userdata*/)
{
    return ::open(path, flags);
}

void fd_close(int fd, void* /*userdata*/)
{
    ::close(fd);
}

const libinput_interface fd_ops = {fd_open, fd_close};
}

mie::LibInputWrapper::LibInputWrapper()
    : lib(libinput_path_create_context(&fd_ops, nullptr),libinput_unref)
{
}

std::unique_ptr<::libinput_device,mie::LibInputWrapper::DeviceDeleter> mie::LibInputWrapper::add_device(std::string const& path)
{
    auto added_device = ::libinput_path_add_device(lib.get(), path.c_str());
    return std::unique_ptr<::libinput_device,DeviceDeleter>(
        ::libinput_device_ref(added_device),
        libinput_device_unref
);
}

void mie::LibInputWrapper::start_device(InputEventHandlerRegister& mplex, mie::LibInputDevice *dev)
{
    bool start_libinput_listening = active_devices.empty();

    auto existing_pos = find_device(dev->device());

    if (end(active_devices) == existing_pos)
        active_devices.push_back(dev);

    if (start_libinput_listening)
        mplex.register_fd_handler({libinput_get_fd(lib.get())}, this,
                                  [this](int /*fd*/){handle_devices();});
}

void mie::LibInputWrapper::stop_device(InputEventHandlerRegister& mplex, LibInputDevice* dev)
{
    bool devices_available = !active_devices.empty();

    active_devices.erase(
        remove(
            begin(active_devices),
            end(active_devices),
            dev),
        end(active_devices));

    if (devices_available && active_devices.empty())
        mplex.unregister_fd_handler(this);
}

void mie::LibInputWrapper::handle_devices()
{
    libinput_dispatch(lib.get());

    for (std::unique_ptr<libinput_event, void (*)(libinput_event*)> event{libinput_get_event(lib.get()),
                                                                          libinput_event_destroy};
         event.get(); event.reset(libinput_get_event(lib.get())))
    {
        auto active_device = find_device(libinput_event_get_device(event.get()));

        if (end(active_devices) != active_device)
            (*active_device)->process_event(event.get());
    }
}

auto mie::LibInputWrapper::find_device(libinput_device* dev) -> decltype(active_devices.begin())
{
    return find_if(begin(active_devices), end(active_devices), [dev](LibInputDevice* item)
                   { return item->device() == dev;});
}

mie::LibInputWrapper::~LibInputWrapper() = default;
